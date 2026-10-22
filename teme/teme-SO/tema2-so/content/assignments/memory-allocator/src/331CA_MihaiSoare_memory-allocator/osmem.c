// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"
#include "helpers.h"

// used for preallocation of heap
static void *heap_start;
// used as a head of the list of memory blocks
static struct block_meta *block_list;

void *os_malloc(size_t size)
{
	if (size == 0)
		return NULL;

	if (size < MMAP_THRESHOLD) {
		if (!heap_start)
			memory_init();

		coalescing_free_blocks();

		void *allocated_memory = find_best_fit(size);

		if (allocated_memory)
			return allocated_memory;
	} else if (size >= MMAP_THRESHOLD) {
		size_t total_size = META_SIZE + ALIGN(size);

		void *mem = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

		DIE(mem == MAP_FAILED, "mmap failed when tried __os_malloc__");

		struct block_meta *new_block = mem;

		new_block->next = NULL;
		new_block->size = total_size - META_SIZE;
		new_block->status = STATUS_MAPPED;

		return (struct block_meta *) mem + 1;
	}

	return NULL;
}

void memory_init(void)
{
	heap_start = sbrk(MMAP_THRESHOLD);
	DIE(heap_start == (void *) -1, "couldn't create a new heap area");

	struct block_meta *block = heap_start;

	block->size = MMAP_THRESHOLD - META_SIZE;
	block->status = STATUS_FREE;
	block->next = NULL;
	block_list = block;
}

void coalescing_free_blocks(void)
{
	struct block_meta *current = block_list;

	while (current && current->next) {
		if (current->status == STATUS_FREE) {
			struct block_meta *free_block = current->next;

			// check each next block if it is free, then coalesce with
			// the first free found block
			while (free_block && free_block->status == STATUS_FREE) {
				// adds new size
				current->size += free_block->size + META_SIZE;
				// check if there are only 2 elements left
				current->next = free_block->next;

				free_block = free_block->next;
			}
		}
		current = current->next;
	}
}

struct block_meta *split_block(struct block_meta *current, size_t total_size)
{
	if (current->size > total_size + META_SIZE) {
		struct block_meta *new_block;

		new_block = (struct block_meta *)((char *) current + total_size + META_SIZE);

		// create new block of the old memory
		new_block->size = current->size - total_size - META_SIZE;
		new_block->status = STATUS_FREE;
		new_block->next = current->next;

		// modify the allocated meta data
		current->size = total_size;
		current->next = new_block;
	}
	current->status = STATUS_ALLOC;
	return current;
}

void *find_best_fit(size_t size)
{
	struct block_meta *current = block_list;
	size_t total_size = ALIGN(size);
	struct block_meta *prev = NULL;
	struct block_meta *best = NULL;

	// checks all list if there is a free memory block to allocate for the call
	while (current) {
		if (current->status == STATUS_FREE) {
			if (current->size >= total_size && best == NULL)
				best = current;
			else if (best && current->size >= total_size
						  && current->size < best->size)
				best = current;
		}

		if (current->next == NULL)
			prev = current;

		current = current->next;
	}

	if (best != NULL)
		return (struct block_meta *) split_block(best, total_size) + 1;

	void *new_block;
	// last memory allocated is free but needs to allocate more size to it
	if (prev && prev->status == STATUS_FREE) {
		new_block = sbrk(total_size - prev->size);

		DIE(new_block == (void *) -1, "couldn't create a new_block");

		prev->size = total_size;
		prev->status = STATUS_ALLOC;

		return (struct block_meta *) prev + 1;
	}
	// needs to create a full new block
	new_block = sbrk(total_size + META_SIZE);

	DIE(new_block == (void *) -1, "couldn't create a new_block");

	// instatiating the metadata
	struct block_meta *block = new_block;

	block->size = total_size;
	block->status = STATUS_ALLOC;
	block->next = NULL;

	// link the previous memory to the new allocated memory
	prev->next = block;

	return (struct block_meta *) block + 1;
}

void os_free(void *ptr)
{
	if (ptr == NULL)
		return;

	struct block_meta *current = (struct block_meta *) ptr - 1;

	if (current->status == STATUS_MAPPED) {
		int re = munmap(current, current->size + META_SIZE);

		DIE(re == -1, "munmap failed when used __os_free__");
	} else if (current->status == STATUS_ALLOC) {
		current->status = STATUS_FREE;
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	size = nmemb * size;
	size = ALIGN(size);

	if (size == 0)
		return NULL;

	int threshold = getpagesize();

	if (size < threshold - META_SIZE) {
		if (!heap_start)
			memory_init();

		coalescing_free_blocks();

		void *allocated_memory = find_best_fit(size);

		if (allocated_memory) {
			memset(allocated_memory, 0, size);
			return allocated_memory;
		}
	} else if (size >= threshold - META_SIZE) {
		size_t total_size = META_SIZE + ALIGN(size);

		void *mem = mmap(NULL, total_size, PROT_READ | PROT_WRITE,
						MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

		DIE(mem == MAP_FAILED, "mmap failed when tried to call __os_calloc__");

		struct block_meta *new_block = mem;

		new_block->next = NULL;
		new_block->size = total_size - META_SIZE;
		new_block->status = STATUS_MAPPED;

		void *allocated_memory = (struct block_meta *) mem + 1;

		return allocated_memory;
	}

	return NULL;
}

struct block_meta *coalesce_new_blocks(void *ptr, size_t size)
{
	struct block_meta *current = (struct block_meta *)ptr - 1;

	while (current && current->next) {
		if (current->next->status != STATUS_FREE) {
			void *new_ptr = os_malloc(size);

			memmove(new_ptr, ptr, current->size);
			os_free(current + 1);

			return (struct block_meta *) new_ptr - 1;
		}

		struct block_meta *free_block = current->next;

		while (free_block && free_block->status == STATUS_FREE) {
			current->size += free_block->size + META_SIZE;
			current->next = free_block->next;

			if (current->size >= size)
				return split_block(current, size);

			free_block = free_block->next;
		}

		current = current->next;
	}

	return NULL;
}

void *os_realloc(void *ptr, size_t size)
{
	if (size == 0) {
		os_free(ptr);
		return NULL;
	}

	size = ALIGN(size);

	if (ptr == (void *) 0)
		return os_malloc(size);

	struct block_meta *block = (struct block_meta *)ptr - 1;

	if (block->status == STATUS_MAPPED) {
		void *new_ptr = os_malloc(size);

		memmove(new_ptr, ptr, size);
		os_free(ptr);

		return new_ptr;
	}

	if (block->status == STATUS_FREE)
		return NULL;

	if (block->status == STATUS_ALLOC) {
		if (size <= block->size)
			return (struct block_meta *) split_block(block, size) + 1;

		if (size >= MMAP_THRESHOLD) {
			void *new_ptr = os_malloc(size);

			block->status = STATUS_FREE;
			memmove(new_ptr, ptr, block->size);

			return new_ptr;
		}

		struct block_meta *check_block = coalesce_new_blocks(ptr, size);

		// wasn't found any block to coalesce, then expand the actual block
		if (check_block == NULL) {
			void *new_block = sbrk(size - block->size);

			DIE(new_block == (void *) -1, "couldn't create a new heap area");

			block->size = size;
			block->status = STATUS_ALLOC;

			return block + 1;
		} else {
			return check_block + 1;
		}
	}

	return NULL;
}
