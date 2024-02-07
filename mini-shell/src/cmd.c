// SPDX-License-Identifier: BSD-3-Clause

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <fcntl.h>
#include <unistd.h>

#include "cmd.h"
#include "utils.h"

#define READ		0
#define WRITE		1

/**
 * Internal exit/quit command.
 */
static int shell_exit(void)
{
	exit(0);
}

/**
 * Internal change-directory command.
 */
static bool shell_cd(word_t *dir)
{
	if (!dir || !dir->string) {
		fprintf(stderr, "cd: missing argument\n");
		return false;
	}

	char *path = get_word(dir);

	if (chdir(path) != 0) {
		free(path);
		perror("cd");
		return false;
	}
	free(path);
	return true;
}

static bool shell_pwd(const char *out, bool append)
{
	char cwd[1024];

	if (getcwd(cwd, sizeof(cwd)) == NULL) {
		perror("pwd");
		return false;
	}

	int fd_out;
	int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);

	if (out != NULL) {
		fd_out = open(out, flags, 0644);

		if (fd_out == -1)
			return false;

		dprintf(fd_out, "%s\n", cwd);
		close(fd_out);
	}

	return true;
}

bool redirect_command(const char *in, const char *out, const char *err,
					bool append, bool skipDup, bool is_pwd)
{
	int fd_out, fd_err;
	int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);

	// Redirect standard input if needed
	if (in != NULL) {
		int fd_in = open(in, O_RDONLY);

		if (fd_in == -1)
			return false;

		if (!skipDup && dup2(fd_in, STDIN_FILENO) == -1) {
			close(fd_in);
			return false;
		}
		close(fd_in);
	}

	// Redirect both stdout and stderr to the same file if out and err are the same
	if (out != NULL && err != NULL && strcmp(out, err) == 0) {
		fd_out = open(out, flags, 0644);

		if (fd_out == -1)
			return false;

		if (!skipDup && (dup2(fd_out, STDOUT_FILENO) == -1
			|| dup2(fd_out, STDERR_FILENO) == -1)) {
			close(fd_out);
			return false;
		}
		close(fd_out);
	} else {
		// Redirect standard output if needed
		if (out != NULL) {
			fd_out = open(out, flags, 0644);

			if (fd_out == -1)
				return false;

			if (!skipDup && dup2(fd_out, STDOUT_FILENO) == -1) {
				close(fd_out);
				return false;
			}
			close(fd_out);
		}

		// Redirect standard error if needed
		if (err != NULL) {
			fd_err = open(err, flags, 0644);

			if (fd_err == -1)
				return false;

			if (!skipDup && dup2(fd_err, STDERR_FILENO) == -1) {
				close(fd_err);
				return false;
			}
			close(fd_err);
		}
	}

	return true;
}

/**
 * Parse a simple command (internal, environment variable assignment,
 * external command).
 */
static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	int status = 0;
	char *command = get_word(s->verb);
	char *in = get_word(s->in);
	char *out = get_word(s->out);
	char *err = get_word(s->err);

	// Sanity checks
	if (!s || !s->verb) {
		fprintf(stderr, "Invalid command\n");
		return EXIT_FAILURE;
	}

	int append = false;

	if (s->io_flags == IO_OUT_APPEND || s->io_flags == IO_ERR_APPEND)
		append = true;

	// Check if it's a built-in command and execute it
	if (strcmp(command, "cd") == 0) {
		redirect_command(in, out, err, append, true, false);
		return shell_cd(s->params) ? EXIT_SUCCESS : EXIT_FAILURE;
	} else if (strcmp(command, "pwd") == 0) {
		return shell_pwd(out, append) ? EXIT_SUCCESS : EXIT_FAILURE;
	} else if (strcmp(s->verb->string, "exit") == 0
			|| strcmp(s->verb->string, "quit") == 0) {
		shell_exit();
	}

	/* If variable assignment, execute the assignment and return
	 * the exit status.
	 */
	if (s->verb->next_part != NULL) {
		char *variable = (char *)s->verb->string;

		/* and if that part is an equal */
		if (strcmp(s->verb->next_part->string, "=") == 0) {
			if (s->verb->next_part->next_part != NULL) {
				/* set the variable if the value exists */
				char *value = get_word(s->verb->next_part->next_part);

				status = setenv(variable, value, 1);
				free(value);

				if (status < 0)
					fprintf(stderr, "Setenv error\n");

				free(variable);
				return status;

			} else {
				fprintf(stderr, "Command error\n");
				free(variable);
				return EXIT_FAILURE;
			}
		}
	}

	pid_t pid = fork();

	if (pid == -1) {
		perror("fork");
		return EXIT_FAILURE;
	} else if (pid == 0) {
		// Child process

		redirect_command(in, out, err, append, false, false);
		int argv_size;
		char **argv = get_argv(s, &argv_size);

		execvp(command, argv);

		fprintf(stderr, "Execution failed for '%s'\n", command);

		for (int i = 0; i < argv_size; i++)
			free(argv[i]);

		free(argv);

		exit(EXIT_FAILURE);

	} else {
		// Parent process
		waitpid(pid, &status, 0);

		if (WIFEXITED(status))
			return WEXITSTATUS(status);
	}

	return status;
}

/**
 * Process two commands in parallel, by creating two children.
 */
static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	/* Execute cmd1 and cmd2 simultaneously. */
	pid_t pid1, pid2;

	pid1 = fork();

	if (pid1 == -1) {
		perror("fork");
		return false;
	}

	if (pid1 == 0) {
		// Child process for cmd1
		exit(parse_command(cmd1, level + 1, father));
	}

	pid2 = fork();

	if (pid2 == -1) {
		perror("fork");
		return false;
	}

	if (pid2 == 0) {
		// Child process for cmd2
		exit(parse_command(cmd2, level + 1, father));
	}

	// Parent process waits for both children
	int status1, status2;

	waitpid(pid1, &status1, 0);
	waitpid(pid2, &status2, 0);

	return WIFEXITED(status2) && WEXITSTATUS(status2) == 0;
}

/**
 * Run commands by creating an anonymous pipe (cmd1 | cmd2).
 */
static int run_on_pipe(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	/* Redirect the output of cmd1 to the input of cmd2. */
	int pipefd[2];

	if (pipe(pipefd) == -1) {
		perror("pipe");
		exit(EXIT_FAILURE);
	}

	pid_t pid1 = fork();

	if (pid1 == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if (pid1 == 0) {
		// Child process for cmd1
		close(pipefd[READ]);

		int ret = dup2(pipefd[WRITE], STDOUT_FILENO);

		if (ret == -1) {
			perror("dup2");
			close(pipefd[WRITE]);
			exit(EXIT_FAILURE);
		}

		close(pipefd[WRITE]);
		exit(parse_command(cmd1, level + 1, father));
	}

	pid_t pid2 = fork();

	if (pid2 == -1) {
		perror("fork");
		return false;
	}

	if (pid2 == 0) {
		// Child process for cmd2
		close(pipefd[WRITE]);

		int ret = dup2(pipefd[READ], STDIN_FILENO);

		if (ret == -1) {
			perror("dup2");
			close(pipefd[READ]);
			exit(EXIT_FAILURE);
		}

		close(pipefd[READ]);
		exit(parse_command(cmd2, level + 1, father));
	}

	close(pipefd[READ]);
	close(pipefd[WRITE]);

	int status1, status2;

	waitpid(pid1, &status1, 0);
	waitpid(pid2, &status2, 0);

	if (WIFEXITED(status2))
		return WEXITSTATUS(status2);

	DIE(1, "Child process did not terminate correctly\n");
}

/**
 * Parse and execute a command.
 */
int parse_command(command_t *c, int level, command_t *father)
{
	int status = 0;

	if (c->op == OP_NONE) {
		/* Execute a simple command. */
		status = parse_simple(c->scmd, level + 1, father);
		return status;
	}

	switch (c->op) {
	case OP_SEQUENTIAL:
		/* Execute the commands one after the other. */
		status = parse_command(c->cmd1, level + 1, c);
		status = parse_command(c->cmd2, level + 1, c);
		break;

	case OP_PARALLEL:
		/* Execute the commands simultaneously. */
		run_in_parallel(c->cmd1, c->cmd2, level + 1, c);
		break;

	case OP_CONDITIONAL_NZERO:
		/* Execute the second command only if the first one
		 * returns non zero.
		 */
		status = parse_command(c->cmd1, level + 1, c);
		if (status != EXIT_SUCCESS)
			status = parse_command(c->cmd2, level + 1, c);
		break;

	case OP_CONDITIONAL_ZERO:
		/* Execute the second command only if the first one
		 * returns zero.
		 */
		status = parse_command(c->cmd1, level + 1, c);
		if (status == EXIT_SUCCESS)
			status = parse_command(c->cmd2, level + 1, c);
		break;

	case OP_PIPE:
		/* Redirect the output of the first command to the
		 * input of the second.
		 */
		status = run_on_pipe(c->cmd1, c->cmd2, level + 1, c);
		break;

	default:
		return SHELL_EXIT;
	}

	return status;
}
