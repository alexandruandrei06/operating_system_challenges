// SPDX-License-Identifier: BSD-3-Clause

#include <fcntl.h>
#include <internal/syscall.h>
#include <stdarg.h>
#include <errno.h>

int open(const char *filename, int flags, ...)
{
	int ret;
	// use a system call to open the file specified by filename with the given flags
	ret = syscall(__NR_open, filename, flags);

	// check if an error occurred during the system call
	if (ret < 0) {
		// set errno
		errno = -ret;
		// return -1 to signal an error in opening the file
		return -1;
	}

	// return the file descriptor associated with the opened file
	return ret;
}
