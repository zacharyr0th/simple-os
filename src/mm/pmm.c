#include "../include/pmm.h"
#include "../include/terminal.h"
#include "../include/panic.h"
#include <stdint.h>

// Simple bitmap-based physical memory manager

// Start managing memory after 4MB (kernel and initial structures)
#define PMM_START 0x400000
#define BITMAP_SIZE (128 * 1024)  // Support up to 4GB of RAM (128KB bitmap)

static uint32_t pmm_bitmap[BITMAP_SIZE / 4];  // Bitmap of free pages
static size_t pmm_total_pages = 0;
static size_t pmm_free_pages = 0;
static size_t pmm_reserved_pages = 0;

// Find first free page in bitmap
static int bitmap_find_free(void) {
    for (size_t i = 0; i < BITMAP_SIZE / 4; i++) {
        if (pmm_bitmap[i] != 0xFFFFFFFF) {
            // Found a word with free bit
            for (int bit = 0; bit < 32; bit++) {
                if (!(pmm_bitmap[i] & (1 << bit))) {
                    return i * 32 + bit;
                }
            }
        }
    }
    return -1;  // No free pages
}

// Mark page as used
static void bitmap_set(size_t page) {
    size_t idx = page / 32;
    size_t bit = page % 32;
    pmm_bitmap[idx] |= (1 << bit);
}

// Mark page as free
static void bitmap_clear(size_t page) {
    size_t idx = page / 32;
    size_t bit = page % 32;
    pmm_bitmap[idx] &= ~(1 << bit);
}

// Test if page is used
static bool bitmap_test(size_t page) {
    size_t idx = page / 32;
    size_t bit = page % 32;
    return pmm_bitmap[idx] & (1 << bit);
}

// Initialize physical memory manager
void pmm_init(uint64_t memory_size) {
    // Calculate total pages
    pmm_total_pages = memory_size / PAGE_SIZE;
    
    // Initially mark all pages as used
    for (size_t i = 0; i < BITMAP_SIZE / 4; i++) {
        pmm_bitmap[i] = 0xFFFFFFFF;
    }
    
    // Mark available pages as free (after kernel)
    size_t first_free_page = PMM_START / PAGE_SIZE;
    size_t last_page = pmm_total_pages;
    
    if (last_page > BITMAP_SIZE * 32) {
        last_page = BITMAP_SIZE * 32;  // Limit to bitmap size
    }
    
    pmm_free_pages = 0;
    for (size_t page = first_free_page; page < last_page; page++) {
        bitmap_clear(page);
        pmm_free_pages++;
    }
    
    pmm_reserved_pages = first_free_page;  // Pages before PMM_START
    
    terminal_writestring("PMM initialized: ");
    // TODO: Print memory stats
    terminal_writestring(" MB total, ");
    terminal_writestring(" MB free\n");
}

// Allocate a single physical page
void* pmm_alloc_page(void) {
    int page = bitmap_find_free();
    if (page == -1) {
        return NULL;  // Out of memory
    }
    
    bitmap_set(page);
    pmm_free_pages--;
    
    // Clear the page
    uint64_t addr = (uint64_t)page * PAGE_SIZE;
    uint64_t* ptr = (uint64_t*)addr;
    for (int i = 0; i < PAGE_SIZE / 8; i++) {
        ptr[i] = 0;
    }
    
    return (void*)addr;
}

// Free a physical page
void pmm_free_page(void* page_addr) {
    uint64_t addr = (uint64_t)page_addr;
    
    // Validate address
    if (addr % PAGE_SIZE != 0 || addr < PMM_START) {
        panic("pmm_free_page: Invalid page address");
        return;
    }
    
    size_t page = addr / PAGE_SIZE;
    if (page >= pmm_total_pages) {
        panic("pmm_free_page: Page out of range");
        return;
    }
    
    if (!bitmap_test(page)) {
        panic("pmm_free_page: Double free detected");
        return;
    }
    
    bitmap_clear(page);
    pmm_free_pages++;
}

// Allocate multiple contiguous pages
void* pmm_alloc_pages(size_t count) {
    if (count == 0) return NULL;
    
    // Find contiguous free pages
    for (size_t start = PMM_START / PAGE_SIZE; start + count <= pmm_total_pages; start++) {
        bool found = true;
        
        // Check if all pages are free
        for (size_t i = 0; i < count; i++) {
            if (bitmap_test(start + i)) {
                found = false;
                break;
            }
        }
        
        if (found) {
            // Allocate all pages
            for (size_t i = 0; i < count; i++) {
                bitmap_set(start + i);
            }
            pmm_free_pages -= count;
            
            // Clear the pages
            uint64_t addr = start * PAGE_SIZE;
            uint64_t* ptr = (uint64_t*)addr;
            for (size_t i = 0; i < count * PAGE_SIZE / 8; i++) {
                ptr[i] = 0;
            }
            
            return (void*)addr;
        }
    }
    
    return NULL;  // No contiguous block found
}

// Free multiple contiguous pages
void pmm_free_pages(void* page_addr, size_t count) {
    if (count == 0) return;
    
    uint64_t addr = (uint64_t)page_addr;
    size_t start_page = addr / PAGE_SIZE;
    
    for (size_t i = 0; i < count; i++) {
        pmm_free_page((void*)(addr + i * PAGE_SIZE));
    }
}

// Get memory statistics
void pmm_get_stats(size_t* total_pages, size_t* free_pages, size_t* used_pages) {
    if (total_pages) *total_pages = pmm_total_pages;
    if (free_pages) *free_pages = pmm_free_pages;
    if (used_pages) *used_pages = pmm_total_pages - pmm_free_pages;
}