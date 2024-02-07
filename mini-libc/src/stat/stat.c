// SPDX-License-Identifier: BSD-3-Clause

#include <sys/stat.h>
#include <internal/syscall.h>
#include <fcntl.h>
#include <errno.h>

int stat(const char *restrict path, struct stat *restrict buf)
{
	// call the stat system call with the provided path and buffer
	int ret = syscall(__NR_stat, path, buf);

	// check if the syscall returned an error
	if (ret < 0) {
		errno = -ret;
		return -1;
	}

	return ret;
}
