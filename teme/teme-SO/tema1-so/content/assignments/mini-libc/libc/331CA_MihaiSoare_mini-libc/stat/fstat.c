// SPDX-License-Identifier: BSD-3-Clause

#include <sys/stat.h>
#include <errno.h>
#include <internal/syscall.h>

int fstat(int fd, struct stat *st)
{
	long ret = syscall(__NR_fstat, fd, st);

	if (ret < 0) {
		errno = (-1) * ret;
		return -1;
	}

	return ret;
}
