// SPDX-License-Identifier: BSD-3-Clause

#include <sys/stat.h>
#include <internal/syscall.h>
#include <errno.h>

int fstat(int fd, struct stat *st)
{
	// call the fstat system call with the provided file descriptor and st
	int ret = syscall(__NR_fstat, fd, st);

	// check if the syscall returned an error
	if (ret < 0) {
		errno = -ret;
		return -1;
	}

	return ret;
}
