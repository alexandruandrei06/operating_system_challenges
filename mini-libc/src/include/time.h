#ifndef	__TIME_H__
#define	__TIME_H__	1

struct timespec {
    long tv_sec;
    long tv_nsec;
};

int nanosleep(const struct timespec *t1, struct timespec *t2);

#endif
