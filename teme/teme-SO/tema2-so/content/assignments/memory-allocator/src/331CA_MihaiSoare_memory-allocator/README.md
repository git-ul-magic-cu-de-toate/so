321CA - Mihai Daniel Soare
Operating Systems

Assignment 2 - Memory Allocator

April 2023
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Built a library which implements the ``malloc``, ``calloc``, ``realloc`` and ``free`` functions for
allocating memory which uses different techniques which are going to be described for each function.

The start of allocated memory is kept in a list of blocks. Each block of memory has a header and
a payload that is of size that the caller of the function wanted.

In the list will be kept only the headers of this memory allocated, and when needed could go
to the payload by shifting to right the size of the struct.

``malloc``

- there are 2 ways of allocating memory: using ``sbrk()`` or ``mmap()`` syscalls

- if a MMAP_THRESHOLD of 128 KB is surpassed when allocating memory, the memory will be allocated
with ``mmap()`` syscall
- otherwise the memory is allocated with ``sbrk()`` syscall

- the memory allocated with ``mmap()`` is not stored anywhere in this library, but will be allocated
with a header which can tell you if the memory was allocated before or not (for free purposes)
- the headers of memory allocated with ``sbrk()`` are stored in the list of meta blocks

- each time when allocating memory with ``sbrk()`` these steps are followed:
a. preallocates heap memory if there weren't any allocations made before
b. coalescing all free adjacent blocks
c. find best fit for the allocated size

- each syscall returns a pointer to the start of the payload for user


``free``

- checks the type of memory allocated:
  if the memory was allocated with ``sbrk()`` then just stores
    in the meta header of the block that the block is FREE to be used later
  otherwise uses ``munmap()`` syscall to deallocates the private memory mapped

``calloc``

- uses similar strategies as malloc, the difference is that instead of using mmap threshold uses
pagesizes as threshold
- sets the memory allocated to 0

``realloc``

- size == 0 => frees the memory and returns NULL
- ptr == 0 => allocates a new memory block of the given size
- the block is MAPPED => allocates a new memory using malloc with mmap() and copies the old
memory to the new one
- the block is FREE => returns NULL
- the block is ALLOCATED =>
  a) wanted size is lesser than the block's size, then just uses the same block and splits it
  b) otherwise checks if the next blocks can be coalesced and do that until there are no more
  blocks to be combined
  c) if there wasn't any free block with enough memory, expands the last block

For checking linter errors I have used:
"https://github.com/torvalds/linux/blob/master/scripts/checkpatch.pl"
Inspiration:
"https://moss.cs.iit.edu/cs351/slides/slides-malloc.pdf"
"https://danluu.com/malloc-tutorial/"