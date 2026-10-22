// SPDX-License-Identifier: BSD-3-Clause

#include <internal/syscall.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

int nanosleep(const struct timespec *t1, struct timespec *t2)
{
	long ret = syscall(__NR_nanosleep, t1, t2);

	if (ret < 0) {
		errno = (-1) * ret;
		return -1;
	}

	return ret;
}

unsigned int sleep(unsigned int seconds)
{
	struct timespec remaining;
	struct timespec requested = {
		seconds, 0
	};

	return nanosleep(&requested, &remaining);
}
