// SPDX-License-Identifier: BSD-3-Clause

#include <internal/mm/mem_list.h>
#include <internal/types.h>
#include <internal/essentials.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>

void *malloc(size_t size)
{
	// check if the requested size is 0
	// and return NULL to indicate an empty allocation
	if (size == 0) {
        return NULL;
    }

	// allocate memory using the mmap system call with specified flags
	void *newP = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	// if the mmap operation fails, return NULL to indicate memory allocation failure
	if (newP == MAP_FAILED){
		return NULL;
	}
	// add the newly allocated memory to a memory management list
	mem_list_add(newP, size);

	// return a pointer to the allocated memory block
	return newP;
}

void *calloc(size_t nmemb, size_t size)
{
	if (nmemb  == 0 || size == 0) {
		return NULL;
	}
	// allocate memory for the requested number of elements and their size
	void *newP =  malloc(nmemb *  size);
	if (newP == NULL){
		return NULL;
	}
	// use memset to initialize all bytes in the allocated memory block to zero
	memset(newP, 0, nmemb * size);
	return newP;
}

void free(void *ptr)
{
	// find the memory block's information in the memory list
	struct mem_list *node = mem_list_find(ptr);
	// deallocate the memory block using munmap
	munmap(ptr, node->len);
	// remove the memory block from the memory list
	mem_list_del(node->start);
}

void *realloc(void *ptr, size_t size)
{
	struct mem_list *node = mem_list_find(ptr);
	// resize the memory block using mremap with MREMAP_MAYMOVE flag
	void *new_start = mremap(node->start, node->len, size, MREMAP_MAYMOVE);

	if (new_start == MAP_FAILED) {
		// if the mremap operation fails, delete the memory block
		// from the custom memory list
		mem_list_del(node);
		return NULL;
	}
	 // update the memory block's start and length
	node->start = new_start;
	node->len = size;

	return new_start;
}

void *reallocarray(void *ptr, size_t nmemb, size_t size)
{
	// use realloc to resize the memory block
	return realloc(ptr, nmemb * size);
}
