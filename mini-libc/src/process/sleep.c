#include <internal/syscall.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

unsigned int sleep(unsigned int seconds) {
	// create a timespec structure with seconds as the
	// sleep duration and 0 nanoseconds
    struct timespec t1 = {seconds, 0};

	// call the nanosleep function with t1 and
	// a NULL pointer for the remaining time
    int ret = nanosleep(&t1, NULL);

    if (ret < 0) {
		errno = -ret;
		return -1;
	}

	return ret;
}
