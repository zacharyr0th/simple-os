#ifndef KMALLOC_H
#define KMALLOC_H

#include <stddef.h>

// Allocate memory
void* kmalloc(size_t size);

// Free memory
void kfree(void* ptr);

// Allocate and zero memory
void* kzalloc(size_t size);

// Reallocate memory
void* krealloc(void* ptr, size_t new_size);

// Get heap statistics
void kmalloc_stats(size_t* allocated, size_t* free, size_t* count);

#endif // KMALLOC_H