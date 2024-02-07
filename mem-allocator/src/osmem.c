// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"
#include "block_meta.h"

#define META_SIZE sizeof(struct block_meta)
#define MMAP_THRESHOLD (128 * 1024)
#define N_MAX 2147483647

static struct block_meta *list_head;
static struct block_meta *list_tail;

// This function calculates the amount of padding needed to align a block of memory
size_t padding(size_t size)
{
	// In this case no padding is needed
	if (size % 8 == 0)
		return 0;

	// Otherwise, return the amount of padding needed
	return (8 - (size % 8));
}

// This function preallocates a block of MMAP_THRESHOLD size using the sbrk syscall
void heap_preallocation(void)
{
	struct block_meta *new_block = sbrk(MMAP_THRESHOLD);

	new_block->size = MMAP_THRESHOLD - META_SIZE - padding(META_SIZE);
	DIE(new_block == MAP_FAILED, "malloc sbrk syscall failed\n");

	// Mark the block as free and set its next and prev pointers to NULL
	new_block->status = STATUS_FREE;
	new_block->next = NULL;
	new_block->prev = NULL;

	// If the list is empty, set the new block as the head of the list
	if (list_head == NULL)
		list_head = new_block;

	// // If the list is not empty, add the new block to the end of the list
	if (list_tail) {
		list_tail->next = new_block;
		new_block->prev = list_tail;
	}

	// Set the new block as the tail of the list
	list_tail = new_block;
}

// This function finds the best free block in the heap to allocate memory of the given size
struct block_meta *find_best_block(size_t size)
{
	struct block_meta *current = list_head;
	struct block_meta *best_block = NULL;
	size_t best_size = N_MAX;

	while (current) {
		// If the current block is free and has enough space to allocate the requested size
		if (current->status == STATUS_FREE && current->size >= size) {
			size_t dif = current->size - size;
			// Update the best size if necessary
			if (dif < best_size) {
				best_size = dif;
				best_block = current;
			}
		}
		current = current->next;
	}
	return best_block;
}

// This function coalesces adjacent free blocks in the heap
void coalesce_free_blocks(void)
{
	// If the list of blocks is not empty, iterate through the list
	if (list_head) {
		struct block_meta *current = list_head;

		while (current) {
			// If the current block is free, merge it with adjacent free blocks
			if (current->status == STATUS_FREE) {
				while (current->next && current->next->status == STATUS_FREE) {
					current->size += current->next->size + META_SIZE + padding(META_SIZE);
					current->next = current->next->next;
				}
			}
			// If the current block is the last block in the list, update the tail pointer
			if (current->next == NULL)
				list_tail = current;
			current = current->next;
		}
	}
}

// This function extends the last block in the heap by the given size
void *extend_last_block(size_t size_new_block, int is_calloc)
{
	// Call the sbrk syscall to increase the heap size by the given size
	void *ret = sbrk(size_new_block - list_tail->size);

	DIE(ret == MAP_FAILED, "malloc sbrk syscall failed\n");

	// Update the size and status of the last block
	list_tail->size = size_new_block;
	list_tail->status = STATUS_ALLOC;

	// If the function is called by calloc, the memory is zeroed out
	if (is_calloc)
		memset((void *)list_tail + META_SIZE + padding(META_SIZE), 0, size_new_block);

	return ((void *)list_tail + META_SIZE + padding(META_SIZE));
}

// This function adds a new block to the heap
void *add_new_block(size_t size_new_block, int is_calloc)
{
	struct block_meta *new_block = sbrk(size_new_block + META_SIZE + padding(META_SIZE));

	DIE(new_block == MAP_FAILED, "malloc sbrk syscall failed\n");
	// Update the size and status of the new block
	new_block->size = size_new_block;
	new_block->status = STATUS_ALLOC;

	// Update the list of blocks
	new_block->next = NULL;
	new_block->prev = list_tail;
	list_tail->next = new_block;
	list_tail = new_block;

	// If the function is called by calloc, the memory is zeroed out
	if (is_calloc)
		memset((void *)new_block + META_SIZE + padding(META_SIZE), 0, size_new_block);

	return ((void *)new_block + META_SIZE + padding(META_SIZE));
}

// This function splits a block into two blocks
void split_block(struct block_meta *best_block, size_t size_best_block)
{
	struct block_meta *next = best_block->next;

	// Newblock starts after the bestblock
	struct block_meta *new_block = (void *)(best_block) + size_best_block + META_SIZE + padding(META_SIZE);

	// Set newblock metadata and update the list of blocks
	new_block->status = STATUS_FREE;
	new_block->size = best_block->size - size_best_block - META_SIZE - padding(META_SIZE);
	new_block->next = next;

	if (next)
		next->prev = new_block;
	else
		list_tail = new_block;

	best_block->next = new_block;
	new_block->prev = best_block;

	best_block->size = size_best_block;
}

// This function allocates memory using mmap syscall
void *memory_mapping(size_t size, int is_calloc)
{
	// Allocate memory using mmap syscall
	size_t total_size = META_SIZE + padding(META_SIZE) + size + padding(size);
	struct block_meta *new_block = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	DIE(new_block == MAP_FAILED, "malloc mmap syscall failed\n");

	// Update the newblock metadata
	new_block->size = size + padding(size);
	new_block->status = STATUS_MAPPED;
	new_block->next = NULL;
	new_block->prev = list_tail;

	// Update the list of blocks
	if (list_head == NULL) {
		list_head = new_block;
		list_tail = new_block;
	} else {
		list_tail->next = new_block;
		list_tail = new_block;
	}

	// If the function is called by calloc, the memory is zeroed out
	if (is_calloc)
		memset((void *)new_block + META_SIZE + padding(META_SIZE), 0, total_size);

	return ((void *)new_block + META_SIZE + padding(META_SIZE));
}

int checkPrealloc(void)
{
	struct block_meta *curr = list_head;

	while (curr) {
		if (curr->status != STATUS_MAPPED)
			return 0;
		curr = curr->next;
	}

	return 1;
}

void *os_malloc(size_t size)
{
	// Check if the size is 0 and return error
	if (size == 0)
		return NULL;
	// Coalesce free blocks
	coalesce_free_blocks();

	// Check if the size is smaller than the threshold
	if (size + META_SIZE < MMAP_THRESHOLD) {
		// Check if the list is null and proceed heap preallocation
		if (checkPrealloc())
			heap_preallocation();

		size_t size_new_block = size + padding(size);
		// Find the best block
		struct block_meta *best_block = find_best_block(size_new_block);
		// If there is no best block
		if (!best_block) {
			// If the last block is free, extend it
			if (list_tail->status == STATUS_FREE) {
				return extend_last_block(size_new_block, 0);
			// Else, add a new block
			} else {
				return add_new_block(size_new_block, 0);
			}
		// If there is a best block
		} else {// This function calculates the amount of padding needed to align a block of memory
			// If the best block is exactly the size of the requested memory
			if (size_new_block == best_block->size) {
				// Update the status of the best block
				best_block->status = STATUS_ALLOC;
				return ((void *)best_block + META_SIZE + padding(META_SIZE));
			// Else, use the best block and split it if necessary
			} else {
				best_block->status = STATUS_ALLOC;
				size_t new_block_size = best_block->size - size_new_block;
				// Split the best block
				if (META_SIZE + padding(META_SIZE) < new_block_size)
					split_block(best_block, size_new_block);

				return ((void *)best_block + META_SIZE + padding(META_SIZE));
			}
		}
	} else {
		// If the size is bigger than the threshold, use mmap syscall
		return memory_mapping(size, 0);
	}
}

// This function frees the memory block pointed by ptr
void os_free(void *ptr)
{
	if (!ptr)
		return;

	struct block_meta *current = (struct block_meta *)(ptr - META_SIZE - padding(META_SIZE));

	// If the block is allocated, mark it as free
	if (current->status == STATUS_ALLOC) {
		current->status = STATUS_FREE;
	// If the block is mapped, unmap it and remove it from the linked list
	} else if (current->status == STATUS_MAPPED) {
		// If the block is the head of the list, update the head pointer
		if (current == list_head) {
			list_head = current->next;
			if (list_head)
				list_head->prev = NULL;

		// If the block is the tail of the list, update the tail pointer
		} else if (current == list_tail) {
			list_tail = current->prev;
			if (list_tail)
				list_tail->next = NULL;

		// If the block is in the middle of the list, update the list
		} else {
			current->next->prev = current->prev;
			current->prev->next = current->next;
		}
		int ret = munmap(current, current->size + META_SIZE + padding(META_SIZE));

		DIE(ret == -1, "free munmap syscall failed\n");
	}
}


void *os_calloc(size_t nmemb, size_t size)
{
	size *= nmemb;
	// Check if the size is 0 and return error
	if (size == 0)
		return NULL;

	// Coalesce free blocks
	coalesce_free_blocks();

	size_t threshold = getpagesize();

	// Check if the size is smaller than the threshold
	if (size + META_SIZE < threshold) {
		// Check if the list is null and proceed heap preallocation
		if (checkPrealloc())
			heap_preallocation();

		size_t size_new_block = size + padding(size);
		// Find the best block
		struct block_meta *best_block = find_best_block(size_new_block);
		// If there is no best block
		if (!best_block) {
			// If the last block is free, extend it
			if (list_tail->status == STATUS_FREE) {
				return extend_last_block(size_new_block, 1);
			// Else, add a new block
			} else {
				return add_new_block(size_new_block, 1);
			}
		// If there is a best block
		} else {
			// If the best block is exactly the size of the requested memory
			if (size_new_block == best_block->size) {
				// Update the status of the best block
				best_block->status = STATUS_ALLOC;
				memset((void *)best_block + META_SIZE + padding(META_SIZE), 0, best_block->size);
				return ((void *)best_block + META_SIZE + padding(META_SIZE));
			// Else, use the best block and split it if necessary
			} else {
				best_block->status = STATUS_ALLOC;
				size_t new_block_size = best_block->size - size_new_block;
				// Split the best block
				if (META_SIZE + padding(META_SIZE) < new_block_size)
					split_block(best_block, size_new_block);

				memset((void *)best_block + META_SIZE + padding(META_SIZE), 0, best_block->size);
				return ((void *)best_block + META_SIZE + padding(META_SIZE));
			}
		}
	} else {
		// If the size is bigger than the threshold, use mmap syscall
		return memory_mapping(size, 1);
	}
}

// This function finds the last block allocated with brk that is free
struct block_meta *find_last_brk_free(void)
{
	if (list_tail == NULL)
		return NULL;

	struct block_meta *curr = list_tail;

	while (curr->status == STATUS_MAPPED)
		curr = curr->prev;

	if (curr->status == STATUS_FREE)
		return curr;
	else
		return NULL;
}

// This function reallocates a block of memory previously allocated with os_malloc or os_calloc
void *os_realloc(void *ptr, size_t size)
{
	// If ptr is NULL, behave like os_malloc(size)
	if (!ptr)
		return os_malloc(size);

	// If size is 0, behave like os_free(ptr)
	if (size == 0) {
		os_free(ptr);
		return NULL;
	}

	coalesce_free_blocks();
	size_t size_new_block = size + padding(size);

	// Get a pointer to the metadata of the current block
	struct block_meta *current = (struct block_meta *)(ptr - META_SIZE - padding(META_SIZE));

	// If the current block is free, return NULL
	if (current->status == STATUS_FREE)
		return NULL;

	// If the current block is mapped, allocate a new block of the requested size
	if (current->status == STATUS_MAPPED) {
		void *new_ptr = os_malloc(size);

		// Copy the contents of the old block to the new block
		if (current->size < size)
			memcpy(new_ptr, ptr, current->size);
		else
			memcpy(new_ptr, ptr, size);
		// Free the old block
		os_free(ptr);
		return new_ptr;
	}
	// If the current block is the last block in the list
	if (current == list_tail) {
		// If size is bigger than the current block size, extend the block
		if (current->size < size_new_block) {
			void *ptr = extend_last_block(size_new_block, 0);
			return ptr;
		// Else, split the block if necessary
		} else {
			if (current->size - size_new_block >= META_SIZE + padding(META_SIZE) + 8)
				split_block(current, size_new_block);
			return ptr;
		}
	} else {
		// If the next block is free, merge the two blocks
		if (current->next->status == STATUS_FREE) {
			current->size += current->next->size + META_SIZE + padding(META_SIZE);
			if (current->next->next != NULL)
				current->next->next->prev = current;
			current->next = current->next->next;
		}
		// If the size is smaller than the current block size, split the block if necessary
		if (size_new_block <= current->size) {
			if (current->size - size_new_block >= META_SIZE + padding(META_SIZE) + 8)
				split_block(current, size_new_block);
			return ptr;

		} else {
			// If the size is bigger than the threshold, allocate a new block
			if (size_new_block >= MMAP_THRESHOLD) {
				void *new_ptr = os_malloc(size);

				memcpy(new_ptr, ptr, current->size);
				os_free(ptr);
				return new_ptr;
			}

			struct block_meta *best_block = find_best_block(size_new_block);
			struct block_meta *last_free = find_last_brk_free();

			//  If there is no best block and the last block alloced with brk is free, extend it
			if (last_free && best_block == NULL) {
				void *ret = sbrk(size_new_block - last_free->size);

				DIE(ret == MAP_FAILED, "malloc sbrk syscall failed\n");
				last_free->size = size_new_block;
				last_free->status = STATUS_ALLOC;

				memcpy((void *)last_free + META_SIZE + padding(META_SIZE), ptr, current->size);
				os_free(ptr);
				return (void *)last_free + META_SIZE + padding(META_SIZE);
			// Else, allocate a new block of the requested size
			} else {
				void *new_ptr = os_malloc(size);

				memcpy(new_ptr, ptr, current->size);
				os_free(ptr);
				return new_ptr;
			}
		}
	}
}
