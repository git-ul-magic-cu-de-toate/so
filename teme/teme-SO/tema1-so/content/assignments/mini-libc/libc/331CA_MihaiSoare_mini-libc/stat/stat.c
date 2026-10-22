// SPDX-License-Identifier: BSD-3-Clause

#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <internal/syscall.h>

int stat(const char *restrict path, struct stat *restrict buf)
{
	long ret = syscall(__NR_stat, path, buf);

	if (ret < 0) {
		errno = (-1) * ret;
		return -1;
	}

	return ret;
}
