// SPDX-License-Identifier: BSD-3-Clause
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>
#include <sys/eventfd.h>
#include <libaio.h>
#include <errno.h>

#include "aws.h"
#include "utils/util.h"
#include "utils/debug.h"
#include "utils/sock_util.h"
#include "utils/w_epoll.h"

/* server socket file descriptor */
static int listenfd;

/* epoll file descriptor */
static int epollfd;

static int aws_on_path_cb(http_parser *p, const char *buf, size_t len)
{
	struct connection *conn = (struct connection *)p->data;

	memcpy(conn->request_path, buf, len);
	conn->request_path[len] = '\0';
	conn->have_path = 1;

	return 0;
}

static void connection_prepare_send_reply_header(struct connection *conn)
{
	char header[BUFSIZ];
	int header_length;

	// Set the Content-Type for .bat files
	const char *content_type = "application/octet-stream";

	// Construct the HTTP header
	header_length = snprintf(header, BUFSIZ,
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %zu\r\n"
		"Connection: close\r\n"
		"\r\n",
		content_type, conn->file_size);

	// Check if the header is too large for the buffer
	if (header_length >= BUFSIZ) {
		perror("Header too large for buffer");
		conn->state = STATE_CONNECTION_CLOSED;
		return;
	}

	// Copy the header to the connection's send buffer
	memcpy(conn->send_buffer, header, header_length);
	conn->send_len = header_length;
	conn->send_pos = 0;
}

static void connection_prepare_send_404(struct connection *conn)
{
	/* Prepare the connection buffer to send the 404 header */
	char buff[BUFSIZ] =  "HTTP/1.1 404 Not Found\r\n"
						"Content-Type: text/html\r\n"
						"Content-Length: 0\r\n"
						"Connection: Closed\r\n"
						"\r\n";
	memcpy(conn->send_buffer, buff, sizeof(buff));
	conn->send_len = strlen(conn->send_buffer);
	conn->send_pos = 0;
	conn->state = STATE_SENDING_404;
}

static enum resource_type connection_get_resource_type(struct connection *conn)
{
	if (strstr(conn->request_path, "static") != NULL) {
		conn->res_type = RESOURCE_TYPE_STATIC;
		memcpy(conn->filename, conn->request_path, strlen(conn->request_path));
	} else if (strstr(conn->request_path, "dynamic") != NULL) {
		conn->res_type = RESOURCE_TYPE_DYNAMIC;
		memcpy(conn->filename, conn->request_path, strlen(conn->request_path));
	} else {
		conn->res_type = RESOURCE_TYPE_NONE;
	}

	return conn->res_type;
}

struct connection *connection_create(int sockfd)
{
	/* Initialize connection structure on given socket. */
	struct connection *conn = malloc(sizeof(*conn));

	DIE(conn == NULL, "malloc");

	conn->sockfd = sockfd;
	memset(conn->filename, 0, BUFSIZ);
	memset(conn->recv_buffer, 0, BUFSIZ);
	memset(conn->send_buffer, 0, BUFSIZ);
	memset(conn->request_path, 0, BUFSIZ);

	/* Initialize the asynchronous I/O context */
	if (io_setup(128, &conn->ctx) < 0) {
		perror("io_setup failed");
		free(conn);
		return NULL;
	}

	conn->state = STATE_INITIAL;
	return conn;
}

void connection_remove(struct connection *conn)
{
	int rc;

	rc = w_epoll_remove_ptr(epollfd, conn->sockfd, conn);
	DIE(rc < 0, "w_epoll_remove_ptr");

	io_destroy(conn->ctx);
	close(conn->sockfd);
	free(conn);
}

void handle_new_connection(void)
{
	static int sockfd;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	struct sockaddr_in addr;
	struct connection *conn;
	int rc;

	/* Accept new connection */
	sockfd = accept(listenfd, (SSA *) &addr, &addrlen);
	DIE(sockfd < 0, "accept");

	dlog(LOG_ERR, "Accepted connection from: %s:%d\n",
		inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

	/* Set socket to be non-blocking */
	int flags = fcntl(sockfd, F_GETFL, 0);

	DIE(flags < 0, "fcntl");

	rc = fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
	DIE(rc < 0, "w_set_nonblock");

	/* Instantiate new connection handler */
	conn = connection_create(sockfd);
	DIE(conn == NULL, "connection_create");

	/* Add socket to epoll */
	rc = w_epoll_add_ptr_in(epollfd, sockfd, conn);
	DIE(rc < 0, "w_epoll_add_fd_in");

	/* Initialize HTTP_REQUEST parser */
	http_parser_init(&conn->request_parser, HTTP_REQUEST);
	conn->request_parser.data = conn;
}

void receive_data(struct connection *conn)
{
	ssize_t bytes_recv;
	int rc;
	char abuffer[64];
	size_t total_bytes_recv = 0;
	char *recv_ptr = conn->recv_buffer;

	rc = get_peer_address(conn->sockfd, abuffer, 64);
	if (rc < 0) {
		ERR("get_peer_address");
		connection_remove(conn);
		return;
	}

	while (total_bytes_recv < BUFSIZ) {
		bytes_recv = recv(conn->sockfd, recv_ptr, BUFSIZ - total_bytes_recv, 0);
		if (bytes_recv < 0) {
			dlog(LOG_ERR, "Error in communication from: %s\n", abuffer);
			connection_remove(conn);
			break;
		}
		if (bytes_recv == 0) {
			dlog(LOG_INFO, "Connection closed from: %s\n", abuffer);
			connection_remove(conn);
			break;
		}

		total_bytes_recv += bytes_recv;
		recv_ptr += bytes_recv;

		// Check if the end of the message is reached
		if (strstr(conn->recv_buffer, "\r\n\r\n") != NULL)
			break;
	}

	conn->recv_len = total_bytes_recv;
	conn->state = STATE_REQUEST_RECEIVED;
}

int connection_open_file(struct connection *conn)
{
	/* Open the file */
	conn->fd = open(conn->filename + 1, O_RDONLY);
	if (conn->fd < 0) {
		perror("Failed to open file");
		return -1;
	}

	/* Get the file size */
	struct stat stat_buf;

	if (fstat(conn->fd, &stat_buf) < 0) {
		perror("Failed to get file size");
		close(conn->fd);
		return -1;
	}
	conn->file_size = stat_buf.st_size;

	return 0;
}

int parse_header(struct connection *conn)
{
	/* Parse the HTTP header and extract the file path. */
	http_parser_settings settings_on_path = {
		.on_message_begin = 0,
		.on_header_field = 0,
		.on_header_value = 0,
		.on_path = aws_on_path_cb,
		.on_url = 0,
		.on_fragment = 0,
		.on_query_string = 0,
		.on_body = 0,
		.on_headers_complete = 0,
		.on_message_complete = 0
	};

	size_t bytes_parsed;

	bytes_parsed = http_parser_execute(&conn->request_parser, &settings_on_path, conn->recv_buffer, conn->recv_len);

	return bytes_parsed;
}

enum connection_state connection_send_static(struct connection *conn)
{
	off_t offset = 0;

	while (offset < conn->file_size) {
		ssize_t sent = sendfile(conn->sockfd, conn->fd, &offset, conn->file_size - offset);

		if (sent <= 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				/* Would block, try again later */
				return STATE_SENDING_DATA;
			}
			perror("sendfile");
			close(conn->fd);
			return STATE_CONNECTION_CLOSED;
		}
	}

	/* File completely sent */
	return STATE_DATA_SENT;
}

void connection_start_async_io(struct connection *conn)
{
	struct iocb cb;
	struct iocb *cbs[1];
	int ret;

	// Read up to BUFSIZ bytes from the file
	size_t to_read = conn->file_size - conn->file_pos;

	if (to_read > BUFSIZ)
		to_read = BUFSIZ;

	// Prepare the control block for the read operation
	io_prep_pread(&cb, conn->fd, conn->send_buffer, to_read, conn->file_pos);
	cbs[0] = &cb;

	// Submit the read request
	ret = io_submit(conn->ctx, 1, cbs);
	if (ret != 1) {
		perror("io_submit failed");
		conn->state = STATE_CONNECTION_CLOSED;
		close(conn->fd);
	} else {
		conn->async_read_len = to_read;
	}

	struct epoll_event ev;

	ev.events = EPOLLOUT | EPOLLET;
	ev.data.ptr = conn;
	int rc = epoll_ctl(epollfd, EPOLL_CTL_MOD, conn->sockfd, &ev);

	DIE(rc < 0, "epoll_ctl EPOLL_CTL_MOD EPOLLOUT");
}

void connection_complete_async_io(struct connection *conn)
{
	struct io_event events[1];
	struct timespec timeout = {0, 0};
	int ret;

	// We wait for the completion of the asynchronous I/O operation
	ret = io_getevents(conn->ctx, 1, 1, events, &timeout);
	if (ret < 0) {
		perror("io_getevents failed");
		conn->state = STATE_CONNECTION_CLOSED;
		return;
	}

	if (events[0].res2 != 0) {
		perror("AIO operation failed");
		conn->state = STATE_CONNECTION_CLOSED;
		return;
	}

	// Update the position in the file and the length of the data read for sending
	conn->file_pos += events[0].res;
	conn->send_len = events[0].res;
	conn->send_pos = 0;

	// Check if there is more data to read and send
	if (conn->file_pos < conn->file_size) {
		conn->state = STATE_SENDING_DATA;
	} else {
		// The entire file has been read and sent
		conn->state = STATE_DATA_SENT;
	}
}

int connection_send_dynamic(struct connection *conn)
{
	ssize_t bytes_sent;

	while (conn->send_pos < conn->send_len) {
		bytes_sent = send(conn->sockfd, conn->send_buffer + conn->send_pos,
							conn->send_len - conn->send_pos, 0);
		if (bytes_sent < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return 0;

			perror("send failed");
			return -1;
		}

		conn->send_pos += bytes_sent;
	}

	if (conn->send_pos == conn->send_len) {
		// If there is still data to be read from the file
		if (conn->file_pos < conn->file_size) {
			// Start asynchronous I/O
			connection_start_async_io(conn);
		} else {
			conn->state = STATE_DATA_SENT;
		}
	}

	dlog(LOG_DEBUG, "Exiting..\n");
	return 0;
}

int connection_send_data(struct connection *conn)
{
	ssize_t bytes_sent;
	int rc;
	char abuffer[64];
	size_t total_bytes_sent = 0;
	const char *send_ptr = conn->send_buffer;

	rc = get_peer_address(conn->sockfd, abuffer, 64);
	if (rc < 0) {
		ERR("get_peer_address");
		connection_remove(conn);
		return -1;
	}

	dlog(LOG_DEBUG, "Sending message to %s\n", abuffer);

	while (total_bytes_sent < conn->send_len) {
		bytes_sent = send(conn->sockfd, send_ptr, conn->send_len - total_bytes_sent, 0);
		if (bytes_sent < 0) {
			dlog(LOG_ERR, "Error in communication to %s\n", abuffer);
			connection_remove(conn);
			return -1;
		}

		if (bytes_sent == 0) {
			dlog(LOG_INFO, "Connection closed to %s\n", abuffer);
			connection_remove(conn);
			return total_bytes_sent;
		}

		total_bytes_sent += bytes_sent;
		send_ptr += bytes_sent;
	}

	return total_bytes_sent;
}

int send_file(struct connection *conn)
{
	if (connection_open_file(conn) == 0) {
		connection_prepare_send_reply_header(conn);
		connection_send_data(conn);
		switch (conn->res_type) {
		/* Send static data*/
		case RESOURCE_TYPE_STATIC:
			conn->state = connection_send_static(conn);
			break;
		/* Prepare sending dynamic data */
		case RESOURCE_TYPE_DYNAMIC:
			connection_start_async_io(conn);
			return 1;
		default:
			break;
		}
		connection_remove(conn);
		return 1;

	} else {
		conn->state = STATE_SENDING_404;
	}

	return 0;
}


void handle_input(struct connection *conn)
{
	while (1) {
		switch (conn->state) {
		case STATE_INITIAL:
			conn->state = STATE_RECEIVING_DATA;
			receive_data(conn);
			break;
		case STATE_REQUEST_RECEIVED:
			parse_header(conn);
			if (conn->have_path == 0) {
				conn->state = STATE_SENDING_404;
			} else {
				connection_get_resource_type(conn);
				if (conn->res_type == RESOURCE_TYPE_STATIC || conn->res_type == RESOURCE_TYPE_DYNAMIC) {
					if (send_file(conn))
						return;
				} else {
					conn->state = STATE_SENDING_404;
				}
			}
			break;
		case STATE_SENDING_404:
			connection_prepare_send_404(conn);
			connection_send_data(conn);
			connection_remove(conn);
			return;
		case STATE_CONNECTION_CLOSED:
			dlog(LOG_DEBUG, "STATE_CONNECTION_CLOSED\n");
			connection_remove(conn);
			return;
		default:
			printf("Unhandled state %d\n", conn->state);
			return;
		}
	}
}

void handle_output(struct connection *conn)
{
	connection_complete_async_io(conn);
	int ret = connection_send_dynamic(conn);

	if (ret < 0)
		conn->state = STATE_CONNECTION_CLOSED;
}

void handle_client(uint32_t event, struct connection *conn)
{
	if (event & EPOLLOUT)
		handle_output(conn);

	if (event & EPOLLIN)
		handle_input(conn);
}

int main(void)
{
	int rc;

	/* Initialize multiplexing */
	epollfd = w_epoll_create();
	DIE(epollfd < 0, "w_epoll_create");

	/* Create server socket */
	listenfd = tcp_create_listener(AWS_LISTEN_PORT,
		DEFAULT_LISTEN_BACKLOG);
	DIE(listenfd < 0, "tcp_create_listener");

	/* Add server socket to epoll object */
	rc = w_epoll_add_fd_in(epollfd, listenfd);
	DIE(rc < 0, "w_epoll_add_fd_in");

	while (1) {
		struct epoll_event rev;

		/* Wait for events */
		rc = w_epoll_wait_infinite(epollfd, &rev);
		DIE(rc < 0, "w_epoll_wait_infinite");

		if (rev.data.fd == listenfd) {
			dlog(LOG_DEBUG, "New connection\n");
			if (rev.events & EPOLLIN)
				handle_new_connection();
		} else {
			dlog(LOG_DEBUG, "New message\n");
			handle_client(rev.events, rev.data.ptr);
		}
	}

	return 0;
}
