// SPDX-License-Identifier: BSD-3-Clause

#include <internal/mm/mem_list.h>
#include <internal/types.h>
#include <internal/essentials.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <internal/syscall.h>

void *malloc(size_t size)
{
	if (size == 0)
		return NULL;

	void *block = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	if (block == MAP_FAILED)
		return NULL;

	int ret = mem_list_add(block, size);

	if (ret == -1) {
		munmap(block, size);
		return NULL;
	} else {
		return mem_list_head.prev->start;
	}
}

void *calloc(size_t nmemb, size_t size)
{
	struct mem_list *mem = malloc(nmemb * size);

	memset(mem->start, 0, mem->len);

	return malloc(nmemb * size);
}

void free(void *ptr)
{
	struct mem_list *mem = mem_list_find(ptr);

	munmap(mem->start, mem->len);

	mem_list_del(ptr);
}

void *realloc(void *ptr, size_t size)
{
	struct mem_list *mem = malloc(size);
	struct mem_list *src = (struct mem_list *) ptr;

	memcpy(mem->start, src->start, src->len);

	free(ptr);

	return mem;
}

void *reallocarray(void *ptr, size_t nmemb, size_t size)
{
	return realloc(ptr, nmemb * size);
}
