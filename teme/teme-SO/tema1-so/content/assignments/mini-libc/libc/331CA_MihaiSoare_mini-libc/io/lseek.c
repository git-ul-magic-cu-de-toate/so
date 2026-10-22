// SPDX-License-Identifier: BSD-3-Clause

#include <unistd.h>
#include <internal/syscall.h>
#include <errno.h>

off_t lseek(int fd, off_t offset, int whence)
{
	long ret = syscall(__NR_lseek, fd, offset, whence);

	if (ret < 0) {
		errno = (-1) * ret;
		return -1;
	}

	return (int) ret;
}
