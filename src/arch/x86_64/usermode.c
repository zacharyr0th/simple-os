#include <stdint.h>
#include "../include/process.h"
#include "../include/tss.h"
#include "../include/kmalloc.h"
#include "../include/terminal.h"
#include "../include/panic.h"

// Segment selectors
#define KERNEL_CODE_SEL 0x08  // GDT entry 1
#define KERNEL_DATA_SEL 0x10  // GDT entry 2
#define USER_CODE_SEL   0x1B  // GDT entry 3 | RPL 3
#define USER_DATA_SEL   0x23  // GDT entry 4 | RPL 3

// Switch current process to user mode
void switch_to_user_mode(void* entry_point, void* user_stack) {
    // Get current process
    process_t* current = process_get_current();
    if (!current) {
        panic("switch_to_user_mode: No current process!");
    }
    
    // Set the kernel stack in TSS for when we return from user mode
    tss_set_kernel_stack((uint64_t)current->kernel_stack + current->kernel_stack_size);
    
    terminal_writestring("Switching to user mode: entry=");
    // TODO: Print entry point
    terminal_writestring(", stack=");
    // TODO: Print stack
    terminal_writestring("\n");
    
    // Use inline assembly to switch to user mode
    asm volatile(
        "mov %0, %%rsp\n"        // Set user stack
        "pushq %1\n"             // Push user data segment
        "pushq %0\n"             // Push user stack pointer
        "pushfq\n"               // Push RFLAGS
        "pushq %2\n"             // Push user code segment
        "pushq %3\n"             // Push entry point
        "mov %1, %%ax\n"         // Load user data segment
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "iretq\n"                // Return to user mode
        :
        : "r"(user_stack), "i"(USER_DATA_SEL), "i"(USER_CODE_SEL), "r"(entry_point)
        : "memory"
    );
}

// Test function that will run in user mode
void user_mode_test(void) {
    // This will run in ring 3!
    // Try to make a system call to write
    const char* msg = "Hello from user mode!\n";
    asm volatile(
        "mov $2, %%rax\n"        // sys_write
        "mov $1, %%rdi\n"        // stdout
        "mov %0, %%rsi\n"        // message
        "mov $22, %%rdx\n"       // length
        "int $0x80\n"
        :
        : "r"(msg)
        : "rax", "rdi", "rsi", "rdx"
    );
    
    // Test getpid syscall
    uint64_t pid;
    asm volatile(
        "mov $4, %%rax\n"        // sys_getpid
        "int $0x80\n"
        "mov %%rax, %0"
        : "=r"(pid) : : "rax"
    );
    
    // Write PID message
    const char* pid_msg = "User mode process PID obtained!\n";
    asm volatile(
        "mov $2, %%rax\n"        // sys_write
        "mov $1, %%rdi\n"        // stdout
        "mov %0, %%rsi\n"        // message
        "mov $32, %%rdx\n"       // length
        "int $0x80\n"
        :
        : "r"(pid_msg)
        : "rax", "rdi", "rsi", "rdx"
    );
    
    // Loop forever using sleep syscall
    while (1) {
        const char* loop_msg = "[User Mode] Still running...\n";
        asm volatile(
            "mov $2, %%rax\n"        // sys_write
            "mov $1, %%rdi\n"        // stdout
            "mov %0, %%rsi\n"        // message
            "mov $29, %%rdx\n"       // length
            "int $0x80\n"
            :
            : "r"(loop_msg)
            : "rax", "rdi", "rsi", "rdx"
        );
        
        // Sleep for 3 seconds
        asm volatile(
            "mov $5, %%rax\n"        // sys_sleep
            "mov $3000, %%rdi\n"     // 3000ms
            "int $0x80\n"
            ::: "rax", "rdi"
        );
    }
}

// Create a user mode process
process_t* create_user_process(const char* name, void (*entry_point)(void)) {
    // Create process as usual
    process_t* proc = process_create(name, NULL, 1);  // NULL because we'll set up user entry
    if (!proc) {
        return NULL;
    }
    
    // Allocate user stack
    void* user_stack = kmalloc(8192);
    if (!user_stack) {
        process_destroy(proc);
        return NULL;
    }
    
    // Set up the process to start in user mode
    // We'll modify the context to use a trampoline
    proc->entry_point = entry_point;
    
    // The kernel stack already has the context set up
    // We need to modify it to switch to user mode
    
    terminal_writestring("Created user mode process: ");
    terminal_writestring(name);
    terminal_writestring("\n");
    
    return proc;
}

// Test user mode functionality
void test_user_mode(void) {
    terminal_writestring("\n=== Testing User Mode ===\n");
    
    // Allocate a user stack
    void* user_stack = kmalloc(4096);
    if (!user_stack) {
        panic("Failed to allocate user stack!");
    }
    
    // Stack grows down, so point to the top
    user_stack = (uint8_t*)user_stack + 4096;
    
    // Switch to user mode and run the test
    terminal_writestring("Attempting to switch to user mode...\n");
    switch_to_user_mode((void*)user_mode_test, user_stack);
    
    // Should never reach here
    panic("Returned from user mode!");
}