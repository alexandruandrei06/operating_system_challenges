#include <internal/syscall.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

int nanosleep(const struct timespec *t1, struct timespec *t2)
{
	// call the nanosleep system call with the provided timespec pointers
    int ret = syscall(__NR_nanosleep, t1, t2);

    if (ret < 0) {
		errno = -ret;
		return -1;
	}

	return ret;
}
