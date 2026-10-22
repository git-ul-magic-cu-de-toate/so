// SPDX-License-Identifier: BSD-3-Clause

#include <string.h>
#include <stdlib.h>

char *strcpy(char *destination, const char *source)
{
	if (destination == NULL)
		return NULL;

	size_t i = 0;

	for (i = 0; source[i] != '\0'; i++)
		destination[i] = source[i];

	destination[i] = '\0';

	return destination;
}

char *strncpy(char *destination, const char *source, size_t len)
{
	if (destination == NULL)
		return NULL;

	size_t i = 0;

	for (i = 0; i < len; i++)
		destination[i] = source[i];

	return destination;
}

char *strcat(char *destination, const char *source)
{
	size_t i = 0;

	long len_str1 = strlen(destination);
	long len_str2 = strlen(source);

	while (source[i] != '\0') {
		destination[len_str1 + i] = source[i];
		i++;
	}

	destination[len_str1 + len_str2] = '\0';

	return destination;
}

char *strncat(char *destination, const char *source, size_t len)
{
	size_t i = 0;

	long len_str1 = strlen(destination);

	while (i != len) {
		destination[len_str1 + i] = source[i];
		i++;
	}

	destination[len_str1 + i] = '\0';

	return destination;
}

int strcmp(const char *str1, const char *str2)
{
	size_t i = 0, j = 0;
	int len_str1 = strlen(str1);
	int len_str2 = strlen(str2);

	if (len_str1 > len_str2)
		return 1;
	else if (len_str1 < len_str2)
		return -1;

	while (str1[i] != '\0' && str2[j] != '\0') {
		if (str1[i] > str2[j])
			return 1;
		else if (str1[i] < str2[j])
			return -1;

		i++;
		j++;
	}

	return 0;
}

int strncmp(const char *str1, const char *str2, size_t len)
{
	size_t i = 0;

	while (i != len) {
		if (str1[i] == '\0' && str2[i] == '\0')
			break;
		else if (str1[i] == '\0')
			return -1;
		else if (str2[i] == '\0')
			return 1;
		else if (str1[i] > str2[i])
			return 1;
		else if (str1[i] < str2[i])
			return -1;

		i++;
	}
	return 0;
}

size_t strlen(const char *str)
{
	size_t i = 0;

	for (; *str != '\0'; str++, i++)
		;

	return i;
}

char *strchr(const char *str, int c)
{
	if (str == NULL)
		return NULL;

	size_t i = 0;

	while (str[i] != '\0') {
		if (str[i] == c)
			return (char *) str + i;
		i++;
	}

	return NULL;
}

char *strrchr(const char *str, int c)
{
	for (int i = strlen(str); i >= 0; i--)
		if (str[i] == c)
			return (char *) str + i;

	return NULL;
}

char *strstr(const char *str1, const char *str2)
{
	long len_str2 = strlen(str2);

	while (strlen(str1) != (size_t) len_str2) {
		if (strncmp(str1, str2, len_str2) == 0)
			return (char *) str1;

		str1++;
	}

	return NULL;
}

char *strrstr(const char *str1, const char *str2)
{
	long len_str1 = strlen(str1);
	long len_str2 = strlen(str2);

	int i = (len_str1 - len_str2 - 1);

	while (i >= 0) {
		char *ptr = (char *) str1 + i;

		if (strstr(ptr, str2) == NULL)
			i--;
		else
			return ptr;
	}

	return NULL;
}

void *memcpy(void *destination, const void *source, size_t num)
{
	if (destination == NULL)
		return NULL;

	char *dst = (char *) destination;
	char *src = (char *) source;

	size_t i = 0;

	for (i = 0; i < num; i++)
		dst[i] = src[i];

	return destination;
}

void *memmove(void *destination, const void *source, size_t num)
{
	void *tmp = malloc(num);

	if (tmp == NULL)
		return NULL;

	memcpy(tmp, source, num);
	memcpy(destination, tmp, num);

	free(tmp);

	return destination;
}

int memcmp(const void *ptr1, const void *ptr2, size_t num)
{
	size_t i = 0;

	char *cptr1 = (char *) ptr1;
	char *cptr2 = (char *) ptr2;

	while (i != num) {
		if (cptr1[i] == 0 && cptr2[i] == 0)
			break;
		else if (cptr1[i] == 0)
			return -1;
		else if (cptr2[i] == 0)
			return 1;
		else if (cptr1[i] > cptr2[i])
			return 1;
		else if (cptr1[i] < cptr2[i])
			return -1;
		i++;
	}

	return 0;
}

void *memset(void *source, int value, size_t num)
{
	if (source == NULL)
		return NULL;

	char *cptr = source;

	for (size_t i = 0; i < num; i++)
		cptr[i] = value;

	return source;
}
