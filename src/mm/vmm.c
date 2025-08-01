#include "../include/vmm.h"
#include "../include/pmm.h"
#include "../include/terminal.h"
#include "../include/panic.h"
#include <stddef.h>

// Current kernel page table (set during boot)
extern uint64_t* pml4;  // From kernel.c

// Get or create a page table entry
static uint64_t* vmm_get_or_create_table(uint64_t* parent_table, size_t index, uint64_t flags) {
    uint64_t entry = parent_table[index];
    
    if (!(entry & PAGE_PRESENT)) {
        // Allocate new table
        void* new_table = pmm_alloc_page();
        if (!new_table) {
            return NULL;
        }
        
        // Set entry with appropriate flags
        parent_table[index] = ((uint64_t)new_table & ~0xFFF) | flags | PAGE_PRESENT;
        return (uint64_t*)new_table;
    }
    
    // Return existing table
    return (uint64_t*)(entry & ~0xFFF);
}

// Create a new address space for a process
uint64_t* vmm_create_address_space(void) {
    // Allocate a new PML4 table
    uint64_t* new_pml4 = (uint64_t*)pmm_alloc_page();
    if (!new_pml4) {
        return NULL;
    }
    
    // Copy kernel mappings (upper half of address space)
    // Kernel space is 0xFFFF800000000000 and above (entries 256-511)
    for (int i = 256; i < 512; i++) {
        new_pml4[i] = pml4[i];
    }
    
    return new_pml4;
}

// Destroy an address space
void vmm_destroy_address_space(uint64_t* pml4_to_destroy) {
    if (!pml4_to_destroy || pml4_to_destroy == pml4) {
        return;  // Don't destroy kernel page table
    }
    
    // Free user space mappings (entries 0-255)
    // TODO: Recursively free all page tables and mapped pages
    // For now, just free the PML4
    pmm_free_page(pml4_to_destroy);
}

// Map a page in a specific address space
int vmm_map_page(uint64_t* pml4_table, uint64_t virt, uint64_t phys, uint64_t flags) {
    // Ensure page-aligned addresses
    virt &= ~0xFFF;
    phys &= ~0xFFF;
    
    // Get indices
    size_t pml4_idx = PML4_INDEX(virt);
    size_t pdpt_idx = PDPT_INDEX(virt);
    size_t pd_idx = PD_INDEX(virt);
    size_t pt_idx = PT_INDEX(virt);
    
    // Get or create PDPT
    uint64_t pdpt_flags = PAGE_WRITABLE | PAGE_USER;
    if (virt >= KERNEL_BASE) {
        pdpt_flags &= ~PAGE_USER;  // Kernel pages
    }
    
    uint64_t* pdpt = vmm_get_or_create_table(pml4_table, pml4_idx, pdpt_flags);
    if (!pdpt) return -1;
    
    // Get or create PD
    uint64_t* pd = vmm_get_or_create_table(pdpt, pdpt_idx, pdpt_flags);
    if (!pd) return -1;
    
    // Get or create PT
    uint64_t* pt = vmm_get_or_create_table(pd, pd_idx, pdpt_flags);
    if (!pt) return -1;
    
    // Map the page
    pt[pt_idx] = (phys & ~0xFFF) | flags | PAGE_PRESENT;
    
    // Flush TLB for this address
    asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
    
    return 0;
}

// Unmap a page
void vmm_unmap_page(uint64_t* pml4_table, uint64_t virt) {
    virt &= ~0xFFF;
    
    // Get indices
    size_t pml4_idx = PML4_INDEX(virt);
    size_t pdpt_idx = PDPT_INDEX(virt);
    size_t pd_idx = PD_INDEX(virt);
    size_t pt_idx = PT_INDEX(virt);
    
    // Navigate to page table
    uint64_t* pdpt = (uint64_t*)(pml4_table[pml4_idx] & ~0xFFF);
    if (!pdpt || !(pml4_table[pml4_idx] & PAGE_PRESENT)) return;
    
    uint64_t* pd = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFF);
    if (!pd || !(pdpt[pdpt_idx] & PAGE_PRESENT)) return;
    
    uint64_t* pt = (uint64_t*)(pd[pd_idx] & ~0xFFF);
    if (!pt || !(pd[pd_idx] & PAGE_PRESENT)) return;
    
    // Clear the page table entry
    pt[pt_idx] = 0;
    
    // Flush TLB
    asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

// Get physical address from virtual
uint64_t vmm_get_physical(uint64_t* pml4_table, uint64_t virt) {
    // Get indices
    size_t pml4_idx = PML4_INDEX(virt);
    size_t pdpt_idx = PDPT_INDEX(virt);
    size_t pd_idx = PD_INDEX(virt);
    size_t pt_idx = PT_INDEX(virt);
    
    // Navigate page tables
    if (!(pml4_table[pml4_idx] & PAGE_PRESENT)) return 0;
    uint64_t* pdpt = (uint64_t*)(pml4_table[pml4_idx] & ~0xFFF);
    
    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) return 0;
    uint64_t* pd = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFF);
    
    if (!(pd[pd_idx] & PAGE_PRESENT)) return 0;
    uint64_t* pt = (uint64_t*)(pd[pd_idx] & ~0xFFF);
    
    if (!(pt[pt_idx] & PAGE_PRESENT)) return 0;
    
    // Return physical address plus offset
    return (pt[pt_idx] & ~0xFFF) | (virt & 0xFFF);
}

// Switch to a different address space
void vmm_switch_address_space(uint64_t* new_pml4) {
    asm volatile("mov %0, %%cr3" : : "r"(new_pml4) : "memory");
}

// Allocate user pages for a process
int vmm_alloc_user_pages(process_t* process, uint64_t virt_addr, size_t count) {
    // Ensure user space address
    if (virt_addr >= KERNEL_BASE) {
        return -1;
    }
    
    for (size_t i = 0; i < count; i++) {
        void* phys_page = pmm_alloc_page();
        if (!phys_page) {
            // TODO: Cleanup on failure
            return -1;
        }
        
        uint64_t virt = virt_addr + (i * PAGE_SIZE);
        uint64_t phys = (uint64_t)phys_page;
        
        if (vmm_map_page(process->page_table, virt, phys, 
                        PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER) < 0) {
            pmm_free_page(phys_page);
            return -1;
        }
        
        process->pages_allocated++;
    }
    
    return 0;
}

// Set up user stack for a process
int vmm_setup_user_stack(process_t* process) {
    process->stack_top = USER_STACK_TOP;
    process->stack_bottom = USER_STACK_TOP - USER_STACK_SIZE;
    
    // Allocate pages for stack
    size_t stack_pages = USER_STACK_SIZE / PAGE_SIZE;
    return vmm_alloc_user_pages(process, process->stack_bottom, stack_pages);
}

// Set up user heap for a process
int vmm_setup_user_heap(process_t* process) {
    process->heap_start = USER_HEAP_START;
    process->heap_current = USER_HEAP_START;
    process->heap_max = USER_HEAP_START + 0x10000000;  // 256MB heap limit
    
    // Don't pre-allocate heap pages, they'll be allocated on demand via sbrk
    return 0;
}

// Helper functions for address space cloning
static uint64_t clone_page(uint64_t parent_page_phys, uint64_t flags) {
    // Allocate new page
    void* child_page = pmm_alloc_page();
    if (!child_page) return 0;
    
    // Copy contents
    uint8_t* src = (uint8_t*)parent_page_phys;
    uint8_t* dst = (uint8_t*)child_page;
    
    for (int i = 0; i < PAGE_SIZE; i++) {
        dst[i] = src[i];
    }
    
    return ((uint64_t)child_page) | flags;
}

static uint64_t clone_pt(uint64_t parent_pt_phys) {
    uint64_t* parent_pt = (uint64_t*)parent_pt_phys;
    uint64_t* child_pt = (uint64_t*)pmm_alloc_page();
    
    if (!child_pt) return 0;
    
    for (int i = 0; i < 512; i++) {
        if (parent_pt[i] & PAGE_PRESENT) {
            // Clone the actual page
            uint64_t page_phys = parent_pt[i] & ~0xFFF;
            uint64_t flags = parent_pt[i] & 0xFFF;
            child_pt[i] = clone_page(page_phys, flags);
            
            if (!child_pt[i]) {
                // Cleanup on failure
                pmm_free_page(child_pt);
                return 0;
            }
        } else {
            child_pt[i] = 0;
        }
    }
    
    return (uint64_t)child_pt;
}

static uint64_t clone_pd(uint64_t parent_pd_phys) {
    uint64_t* parent_pd = (uint64_t*)parent_pd_phys;
    uint64_t* child_pd = (uint64_t*)pmm_alloc_page();
    
    if (!child_pd) return 0;
    
    for (int i = 0; i < 512; i++) {
        if (parent_pd[i] & PAGE_PRESENT) {
            uint64_t child_pt = clone_pt(parent_pd[i] & ~0xFFF);
            if (!child_pt) {
                pmm_free_page(child_pd);
                return 0;
            }
            child_pd[i] = child_pt | (parent_pd[i] & 0xFFF);
        } else {
            child_pd[i] = 0;
        }
    }
    
    return (uint64_t)child_pd;
}

static uint64_t clone_pdpt(uint64_t parent_pdpt_phys) {
    uint64_t* parent_pdpt = (uint64_t*)parent_pdpt_phys;
    uint64_t* child_pdpt = (uint64_t*)pmm_alloc_page();
    
    if (!child_pdpt) return 0;
    
    for (int i = 0; i < 512; i++) {
        if (parent_pdpt[i] & PAGE_PRESENT) {
            uint64_t child_pd = clone_pd(parent_pdpt[i] & ~0xFFF);
            if (!child_pd) {
                pmm_free_page(child_pdpt);
                return 0;
            }
            child_pdpt[i] = child_pd | (parent_pdpt[i] & 0xFFF);
        } else {
            child_pdpt[i] = 0;
        }
    }
    
    return (uint64_t)child_pdpt;
}

// Clone an entire address space (for fork)
uint64_t* vmm_clone_address_space(uint64_t* parent_pml4) {
    // Allocate new PML4
    uint64_t* child_pml4 = (uint64_t*)pmm_alloc_page();
    if (!child_pml4) return NULL;
    
    // Clear the new PML4
    for (int i = 0; i < 512; i++) {
        child_pml4[i] = 0;
    }
    
    // Copy kernel mappings (upper half: entries 256-511)
    for (int i = 256; i < 512; i++) {
        child_pml4[i] = parent_pml4[i];
    }
    
    // Clone user mappings (lower half: entries 0-255)
    for (int i = 0; i < 256; i++) {
        if (parent_pml4[i] & PAGE_PRESENT) {
            uint64_t child_pdpt = clone_pdpt(parent_pml4[i] & ~0xFFF);
            if (!child_pdpt) {
                // Cleanup on failure
                vmm_destroy_address_space(child_pml4);
                return NULL;
            }
            child_pml4[i] = child_pdpt | (parent_pml4[i] & 0xFFF);
        }
    }
    
    return child_pml4;
}

// Clear user space mappings (for exec)
void vmm_clear_user_space(uint64_t* pml4) {
    // Clear user space entries (0-255)
    for (int i = 0; i < 256; i++) {
        if (pml4[i] & PAGE_PRESENT) {
            // TODO: Recursively free all page tables and pages
            // For now, just clear the entry
            pml4[i] = 0;
        }
    }
    
    // Flush TLB
    asm volatile("mov %%cr3, %%rax; mov %%rax, %%cr3" ::: "rax", "memory");
}