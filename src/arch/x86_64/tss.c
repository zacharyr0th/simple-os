#include "../include/tss.h"
#include "../include/terminal.h"
#include <stdint.h>

// Global TSS instance
tss_t tss;

// Default kernel stack (used when no process-specific stack)
static uint8_t default_kernel_stack[8192] __attribute__((aligned(16)));

// Initialize the TSS
void tss_init(void) {
    // Clear the TSS structure
    uint8_t* tss_ptr = (uint8_t*)&tss;
    for (size_t i = 0; i < sizeof(tss_t); i++) {
        tss_ptr[i] = 0;
    }
    
    // Set up the kernel stack pointer (RSP0)
    // This will be used when transitioning from ring 3 to ring 0
    tss.rsp0 = (uint64_t)(default_kernel_stack + sizeof(default_kernel_stack));
    
    // Set I/O permission bitmap offset to beyond TSS size
    // This effectively disables I/O permissions
    tss.iomap_base = sizeof(tss_t);
    
    // We could set up IST (Interrupt Stack Table) entries here
    // for special handlers like NMI, double fault, etc.
    // For now, leave them as 0 (unused)
    
    terminal_writestring("TSS initialized at ");
    // TODO: Print TSS address
    terminal_writestring("\n");
}

// Set the kernel stack for ring 0
void tss_set_kernel_stack(uint64_t stack) {
    tss.rsp0 = stack;
}

// Get the current kernel stack
uint64_t tss_get_kernel_stack(void) {
    return tss.rsp0;
}