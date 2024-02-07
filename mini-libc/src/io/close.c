// SPDX-License-Identifier: BSD-3-Clause

#include <unistd.h>
#include <internal/syscall.h>
#include <stdarg.h>
#include <errno.h>

int close(int fd)
{
	// system call to close the file associated with the file descriptor
	int ret = syscall(__NR_close, fd);

	// same opperations as in open.c
	if (ret < 0) {
		errno = -ret;
		return -1;
	}

	return ret;
}
