/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define DIE(assertion, call_description)						\
	do {										\
		if (assertion) {							\
			fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);		\
			perror(call_description);					\
			exit(errno);							\
		}									\
	} while (0)

/* Structure to hold memory block metadata */
struct block_meta {
	size_t size;
	int status;
	struct block_meta *next;
};

/* Block metadata status values */
#define STATUS_FREE   0
#define STATUS_ALLOC  1
#define STATUS_MAPPED 2

#define MMAP_THRESHOLD (128 * 1024)
#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define META_SIZE (ALIGN(sizeof(struct block_meta)))

// preallocates a heap memory of MMAP_THRESHOLD and initializes
// the head of the list where the memory blocks will be stored
void memory_init(void);

// coalesces free memory blocks which are adjacent to one free block
// used for implementing `os_malloc`
void coalescing_free_blocks(void);

// split a block of memory into two blocks of sizes or allocates a free
// memory for the call
struct block_meta *split_block(struct block_meta *current, size_t total_size);

// find the best fit block for allocating memory for a call
void *find_best_fit(size_t size);

// iterate through the list of blocks and coalesce all adjacent ones, but
// checking when the size needed for reallocating memory was found
struct block_meta *coalesce_new_blocks(void *ptr, size_t size);
