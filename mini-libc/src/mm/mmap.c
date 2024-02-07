// SPDX-License-Identifier: BSD-3-Clause

#include <sys/mman.h>
#include <errno.h>
#include <internal/syscall.h>


void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	// call the mmap system call with the provided arguments
	void *ret = (void *)syscall(__NR_mmap, addr, length, prot, flags, fd, offset);

	// check flags to be valid
	if ((flags & (MAP_ANONYMOUS)) == 0) {
		// set errno to EBADF in case of an error
		errno = EBADF;
		return MAP_FAILED;
	}

	// if neither MAP_PRIVATE nor MAP_SHARED is set,
	// set errno to EINVAL and return MAP_FAILED
	if ((flags & (MAP_PRIVATE | MAP_SHARED)) == 0) {
		errno = EINVAL;
		return MAP_FAILED;
	}

	if (ret == MAP_FAILED) {
		// errno set to show the error
		errno = -(long)ret;
		return MAP_FAILED;
	}

	return ret;
}

void *mremap(void *old_address, size_t old_size, size_t new_size, int flags)
{
	// call the mremap system call with the provided arguments
	void *ret = (void *)syscall(__NR_mremap, old_address, old_size, new_size, flags);

	if (ret == MAP_FAILED) {
		errno = -(long)ret;
		return MAP_FAILED;
	}

	return ret;
}

int munmap(void *addr, size_t length)
{
	// call the munmap system call with the provided arguments
	int ret = syscall(__NR_munmap, addr, length);

	if (!ret) {
		errno = -(long)ret;
		return -1;
	}

	return ret;
}
