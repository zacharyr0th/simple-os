#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stdbool.h>
#include "process.h"

// Virtual Memory Manager - manages virtual address spaces

// Page flags
#define PAGE_PRESENT    (1 << 0)
#define PAGE_WRITABLE   (1 << 1)
#define PAGE_USER       (1 << 2)
#define PAGE_WRITE_THROUGH (1 << 3)
#define PAGE_CACHE_DISABLE (1 << 4)
#define PAGE_ACCESSED   (1 << 5)
#define PAGE_DIRTY      (1 << 6)
#define PAGE_HUGE       (1 << 7)
#define PAGE_GLOBAL     (1 << 8)
#define PAGE_NX         (1ULL << 63)

// Standard user space memory layout
#define USER_STACK_TOP    0x00007FFFFFFFE000  // Just below kernel space
#define USER_STACK_SIZE   0x100000            // 1MB stack
#define USER_HEAP_START   0x400000            // After typical ELF load address
#define USER_CODE_START   0x100000            // Default code location
#define KERNEL_BASE       0xFFFF800000000000  // Higher half kernel

// Page table indices from virtual address
#define PML4_INDEX(addr) (((addr) >> 39) & 0x1FF)
#define PDPT_INDEX(addr) (((addr) >> 30) & 0x1FF)
#define PD_INDEX(addr)   (((addr) >> 21) & 0x1FF)
#define PT_INDEX(addr)   (((addr) >> 12) & 0x1FF)

// Create a new page table for a process
uint64_t* vmm_create_address_space(void);

// Destroy a page table
void vmm_destroy_address_space(uint64_t* pml4);

// Map a page in a specific address space
int vmm_map_page(uint64_t* pml4, uint64_t virt, uint64_t phys, uint64_t flags);

// Unmap a page
void vmm_unmap_page(uint64_t* pml4, uint64_t virt);

// Get physical address from virtual
uint64_t vmm_get_physical(uint64_t* pml4, uint64_t virt);

// Switch to a different address space
void vmm_switch_address_space(uint64_t* pml4);

// Process-specific memory functions
int vmm_alloc_user_pages(process_t* process, uint64_t virt_addr, size_t count);
int vmm_setup_user_stack(process_t* process);
int vmm_setup_user_heap(process_t* process);

// Address space cloning for fork
uint64_t* vmm_clone_address_space(uint64_t* parent_pml4);
void vmm_clear_user_space(uint64_t* pml4);

#endif // VMM_H