#include "../include/process.h"
#include "../include/kmalloc.h"
#include "../include/terminal.h"
#include "../include/panic.h"
#include "../include/scheduler.h"
#include "../include/tss.h"
#include "../include/vmm.h"
#include "../include/pmm.h"

// From syscall.c
extern void init_process_fd_table(process_t* proc);

// Assembly functions
extern void context_switch(context_t* old_context, context_t* new_context);
extern void process_entry_trampoline(void);

// Process table
process_t* process_table[MAX_PROCESSES];
static uint32_t next_pid = 1;

// Scheduler queues
static process_t* ready_queue_head = NULL;
static process_t* ready_queue_tail = NULL;
process_t* current_process = NULL;

// Idle process
static process_t idle_process;
static uint8_t idle_stack[KERNEL_STACK_SIZE] __attribute__((aligned(16)));

// Helper: Add process to ready queue
void ready_queue_push(process_t* proc) {
    proc->next = NULL;
    proc->prev = ready_queue_tail;
    
    if (ready_queue_tail) {
        ready_queue_tail->next = proc;
    } else {
        ready_queue_head = proc;
    }
    ready_queue_tail = proc;
}

// Helper: Remove process from ready queue
static void ready_queue_remove(process_t* proc) {
    if (proc->prev) {
        proc->prev->next = proc->next;
    } else {
        ready_queue_head = proc->next;
    }
    
    if (proc->next) {
        proc->next->prev = proc->prev;
    } else {
        ready_queue_tail = proc->prev;
    }
    
    proc->next = NULL;
    proc->prev = NULL;
}

// Helper: Get next ready process
process_t* ready_queue_pop(void) {
    process_t* proc = ready_queue_head;
    if (proc) {
        ready_queue_remove(proc);
    }
    return proc;
}

// Idle process - runs when nothing else is ready
static void idle_task(void) {
    while (1) {
        asm volatile("hlt");
    }
}

// Initialize process management
void process_init(void) {
    // Clear process table
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i] = NULL;
    }
    
    // Initialize idle process
    idle_process.pid = 0;
    strcpy(idle_process.name, "idle");
    idle_process.state = PROCESS_STATE_READY;
    idle_process.kernel_stack = idle_stack;
    idle_process.kernel_stack_size = KERNEL_STACK_SIZE;
    idle_process.priority = 255;  // Lowest priority
    idle_process.ticks_total = 0;
    idle_process.ticks_remaining = 1;
    idle_process.entry_point = idle_task;
    
    // Set up idle process context
    idle_process.context.rsp = (uint64_t)(idle_stack + KERNEL_STACK_SIZE);
    idle_process.context.rip = (uint64_t)idle_task;
    idle_process.context.rflags = 0x202;  // Interrupts enabled
    
    // Idle process is always in table[0]
    process_table[0] = &idle_process;
    
    // Initialize idle process fd table
    init_process_fd_table(&idle_process);
    
    terminal_writestring("Process management initialized\n");
}

// Create a new process
process_t* process_create(const char* name, void (*entry_point)(void), uint32_t priority) {
    // Find free slot
    int slot = -1;
    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (process_table[i] == NULL) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        terminal_writestring("Error: Process table full\n");
        return NULL;
    }
    
    // Allocate PCB
    process_t* proc = (process_t*)kzalloc(sizeof(process_t));
    if (!proc) {
        panic("process_create: Out of memory for PCB");
        return NULL;
    }
    
    // Allocate kernel stack
    proc->kernel_stack = kmalloc(KERNEL_STACK_SIZE);
    if (!proc->kernel_stack) {
        kfree(proc);
        panic("process_create: Out of memory for kernel stack");
        return NULL;
    }
    
    // Initialize PCB
    proc->pid = next_pid++;
    strncpy(proc->name, name, 31);
    proc->name[31] = '\0';
    proc->state = PROCESS_STATE_READY;
    proc->kernel_stack_size = KERNEL_STACK_SIZE;
    proc->priority = priority;
    proc->ticks_total = 0;
    proc->ticks_remaining = DEFAULT_QUANTUM;
    proc->entry_point = entry_point;
    
    // Create separate address space for the process
    proc->page_table = vmm_create_address_space();
    if (!proc->page_table) {
        kfree(proc->kernel_stack);
        kfree(proc);
        panic("process_create: Failed to create address space");
        return NULL;
    }
    
    // Set up user stack
    if (vmm_setup_user_stack(proc) < 0) {
        vmm_destroy_address_space(proc->page_table);
        kfree(proc->kernel_stack);
        kfree(proc);
        panic("process_create: Failed to set up user stack");
        return NULL;
    }
    
    // Set up user heap
    if (vmm_setup_user_heap(proc) < 0) {
        vmm_destroy_address_space(proc->page_table);
        kfree(proc->kernel_stack);
        kfree(proc);
        panic("process_create: Failed to set up user heap");
        return NULL;
    }
    
    // Initialize memory statistics
    proc->pages_allocated = 0;
    proc->page_faults = 0;
    
    // Set up initial context
    uint64_t* stack_top = (uint64_t*)((uint8_t*)proc->kernel_stack + KERNEL_STACK_SIZE);
    
    // Push entry point for process_entry_trampoline
    *(--stack_top) = (uint64_t)entry_point;
    
    proc->context.rsp = (uint64_t)stack_top;
    proc->context.rip = (uint64_t)process_entry_trampoline;
    proc->context.rflags = 0x202;  // Interrupts enabled
    
    // Clear other registers
    proc->context.r15 = 0;
    proc->context.r14 = 0;
    proc->context.r13 = 0;
    proc->context.r12 = 0;
    proc->context.rbx = 0;
    proc->context.rbp = 0;
    
    // Add to process table
    process_table[slot] = proc;
    
    // Initialize file descriptor table
    init_process_fd_table(proc);
    
    // Add to ready queue
    ready_queue_push(proc);
    
    terminal_writestring("Created process: ");
    terminal_writestring(name);
    terminal_writestring(" (PID ");
    // TODO: Print PID
    terminal_writestring(")\n");
    
    return proc;
}

// Destroy a process
void process_destroy(process_t* process) {
    if (!process || process == &idle_process) {
        return;
    }
    
    // Remove from queues
    if (process->state == PROCESS_STATE_READY) {
        ready_queue_remove(process);
    }
    
    // Remove from process table
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i] == process) {
            process_table[i] = NULL;
            break;
        }
    }
    
    // Free resources
    if (process->kernel_stack) {
        kfree(process->kernel_stack);
    }
    
    // Free file descriptor table
    if (process->fd_table) {
        kfree(process->fd_table);
    }
    
    // Free page table and address space
    if (process->page_table) {
        vmm_destroy_address_space(process->page_table);
    }
    
    kfree(process);
}

// Get current process
process_t* process_get_current(void) {
    return current_process ? current_process : &idle_process;
}

// Get current PID
uint32_t process_get_pid(void) {
    process_t* proc = process_get_current();
    return proc->pid;
}

// Get current process name
const char* process_get_name(void) {
    process_t* proc = process_get_current();
    return proc->name;
}

// Get process state
process_state_t process_get_state(process_t* process) {
    return process ? process->state : PROCESS_STATE_TERMINATED;
}

// Yield CPU to next process  
void process_yield(void) {
    // Update TSS with current process's kernel stack
    process_t* current = process_get_current();
    if (current && current != &idle_process) {
        tss_set_kernel_stack((uint64_t)current->kernel_stack + current->kernel_stack_size);
    }
    
    // This will be called from timer interrupt
    schedule();
}

// Block current process
void process_block(void) {
    if (current_process && current_process != &idle_process) {
        current_process->state = PROCESS_STATE_BLOCKED;
        schedule();
    }
}

// Unblock a process
void process_unblock(process_t* process) {
    if (process && process->state == PROCESS_STATE_BLOCKED) {
        process->state = PROCESS_STATE_READY;
        ready_queue_push(process);
    }
}

// Exit current process
void process_exit(int status) {
    (void)status;  // TODO: Handle exit status
    
    if (current_process && current_process != &idle_process) {
        terminal_writestring("Process exiting: ");
        terminal_writestring(current_process->name);
        terminal_writestring("\n");
        
        current_process->state = PROCESS_STATE_TERMINATED;
        
        // Clean up will happen later
        // For now, just schedule next process
        schedule();
        
        // Should never return
        panic("process_exit: schedule() returned!");
    }
}

// Print all processes (for debugging)
void process_print_all(void) {
    terminal_writestring("\nProcess List:\n");
    terminal_writestring("PID  Name                     State      Ticks\n");
    terminal_writestring("---  ----------------------  ---------  ------\n");
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* proc = process_table[i];
        if (proc) {
            // TODO: Implement proper printing with formatting
            terminal_writestring("  ");
            terminal_writestring(proc->name);
            terminal_writestring("\n");
        }
    }
}

// Helper to copy string
static void strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

static void strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
}

// Helper functions for fork/exec

// Allocate a new process structure
process_t* allocate_process_struct(void) {
    // Find free slot
    int slot = -1;
    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (process_table[i] == NULL) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        return NULL;  // No free slots
    }
    
    // Allocate PCB
    process_t* proc = (process_t*)kzalloc(sizeof(process_t));
    if (!proc) {
        return NULL;
    }
    
    // Allocate kernel stack
    proc->kernel_stack = kmalloc(KERNEL_STACK_SIZE);
    if (!proc->kernel_stack) {
        kfree(proc);
        return NULL;
    }
    
    // Assign PID and add to table
    proc->pid = next_pid++;
    proc->kernel_stack_size = KERNEL_STACK_SIZE;
    process_table[slot] = proc;
    
    // Initialize file descriptor table
    init_process_fd_table(proc);
    
    return proc;
}

// Free a process structure
void free_process_struct(process_t* process) {
    if (!process) return;
    
    // Remove from process table
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i] == process) {
            process_table[i] = NULL;
            break;
        }
    }
    
    // Free resources
    if (process->kernel_stack) {
        kfree(process->kernel_stack);
    }
    
    if (process->page_table) {
        vmm_destroy_address_space(process->page_table);
    }
    
    kfree(process);
}

// Find a zombie child process
process_t* find_zombie_child(uint32_t parent_pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* proc = process_table[i];
        if (proc && proc->parent_pid == parent_pid && 
            proc->state == PROCESS_STATE_ZOMBIE) {
            return proc;
        }
    }
    return NULL;
}