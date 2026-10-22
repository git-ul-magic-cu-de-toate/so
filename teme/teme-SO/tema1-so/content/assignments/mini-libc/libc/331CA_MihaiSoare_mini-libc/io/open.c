// SPDX-License-Identifier: BSD-3-Clause

#include <fcntl.h>
#include <internal/syscall.h>
#include <stdarg.h>
#include <errno.h>

int open(const char *filename, int flags, ...)
{
	long ret;

	if ((flags & O_CREAT) == O_CREAT) {
		va_list valist;
		long mode;

		va_start(valist, flags);
		mode = va_arg(valist, mode_t);
		va_end(valist);

		ret = syscall(__NR_open, filename, flags, mode);
	} else {
		ret = syscall(__NR_open, filename, flags);
	}

	if (ret < 0) {
		errno = (-1) * ret;
		return -1;
	}

	return ret;
}
