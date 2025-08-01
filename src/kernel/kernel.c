// This is the main kernel file for SimpleOS, implementing core OS functionality.
// It includes memory management, process scheduling, interrupt handling, and basic I/O operations.

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../include/terminal.h"
#include "../include/isr.h"
#include "../include/ports.h"
#include "../include/timer.h"
#include "../include/panic.h"
#include "../include/process.h"
#include "../include/scheduler.h"
#include "../include/kmalloc.h"
#include "../include/keyboard.h"
#include "../include/exceptions.h"
#include "../include/syscall.h"
#include "../include/tss.h"
#include "../include/usermode.h"
#include "../include/pmm.h"
#include "../include/vmm.h"
#include "../include/elf.h"
#include "../include/scheduler.h"
#include "../include/../userspace/hello_binary.h"

// External assembly functions
extern void load_gdt(uintptr_t gdt_ptr);
extern void load_idt(uintptr_t idt_ptr);
extern void enable_paging(uintptr_t* pml4);

// Function prototypes
void init_gdt(void);
void init_idt(void);
void init_paging(void);
void init_keyboard(void);
void init_pic(void);
void test_fork_exec(void);
void fork_test_main(void);
void test_shell(void);
void fs_init(void);
void vt_init(void);

// Memory management
#define PAGE_SIZE 4096
#define ENTRIES_PER_TABLE 512

// Page tables - make pml4 globally accessible for VMM
uint64_t* pml4 = (uint64_t*)0x1000000;
uintptr_t* pdpt = (uintptr_t*)0x1001000;
uintptr_t* pd = (uintptr_t*)0x1002000;
uintptr_t* pt = (uintptr_t*)0x1003000;

// Heap configuration
#define HEAP_START 0x2000000
#define HEAP_SIZE 0x1000000
uint8_t* heap_start = (uint8_t*)HEAP_START; 
uint8_t* heap_end = (uint8_t*)(HEAP_START + HEAP_SIZE);
uint8_t* heap_current = (uint8_t*)HEAP_START;

// Simple memory allocator
void* kmalloc(size_t size) {
    if (heap_current + size > heap_end) {
        return NULL;  // Out of memory
    }
    void* result = (void*)heap_current;
    heap_current += size;
    return result;
}

// Test processes for multitasking demo
void test_process_1(void) {
    int counter = 0;
    while(1) {
        terminal_writestring("[Process 1] Running - count: ");
        // TODO: Print counter
        terminal_writestring("\n");
        counter++;
        sleep_ms(1000);  // Sleep for 1 second
    }
}

// Test process for memory isolation
void test_memory_process(void) {
    terminal_writestring("[Memory Test] Process starting\n");
    
    // Test sbrk allocation
    void* heap_ptr = (void*)0;
    asm volatile(
        "mov $6, %%rax\n"      // SYS_SBRK
        "mov $4096, %%rdi\n"   // Allocate 4KB
        "int $0x80\n"
        "mov %%rax, %0"
        : "=r"(heap_ptr) : : "rax", "rdi"
    );
    
    if (heap_ptr != (void*)-1) {
        terminal_writestring("[Memory Test] Allocated heap at: ");
        // TODO: Print address
        terminal_writestring("\n");
        
        // Write to allocated memory
        char* buffer = (char*)heap_ptr;
        const char* test_msg = "Hello from process heap!";
        
        // Copy test message to heap
        for (int i = 0; test_msg[i] && i < 25; i++) {
            buffer[i] = test_msg[i];
        }
        buffer[25] = '\0';
        
        terminal_writestring("[Memory Test] Wrote to heap: ");
        terminal_writestring(buffer);
        terminal_writestring("\n");
    } else {
        terminal_writestring("[Memory Test] Failed to allocate heap!\n");
    }
    
    // Test stack allocation
    char stack_buffer[1024];
    const char* stack_msg = "Stack allocation works!";
    for (int i = 0; stack_msg[i] && i < 23; i++) {
        stack_buffer[i] = stack_msg[i];
    }
    stack_buffer[23] = '\0';
    
    terminal_writestring("[Memory Test] Stack test: ");
    terminal_writestring(stack_buffer);
    terminal_writestring("\n");
    
    while(1) {
        terminal_writestring("[Memory Test] Process running...\n");
        sleep_ms(3000);
    }
}

// Test process using system calls
void test_syscall_process(void) {
    // Test write syscall
    const char* msg = "Hello from syscall process!\n";
    asm volatile(
        "mov $2, %%rax\n"      // SYS_WRITE
        "mov $1, %%rdi\n"      // stdout
        "mov %0, %%rsi\n"      // buffer
        "mov $28, %%rdx\n"     // length
        "int $0x80"
        : : "r"(msg) : "rax", "rdi", "rsi", "rdx"
    );
    
    // Test getpid syscall
    uint64_t pid;
    asm volatile(
        "mov $4, %%rax\n"      // SYS_GETPID
        "int $0x80\n"
        "mov %%rax, %0"
        : "=r"(pid) : : "rax"
    );
    
    terminal_writestring("My PID from syscall: ");
    // TODO: Print PID
    terminal_writestring("\n");
    
    // Test sleep syscall
    while(1) {
        terminal_writestring("[Syscall Process] Using sys_sleep...\n");
        asm volatile(
            "mov $5, %%rax\n"      // SYS_SLEEP
            "mov $2000, %%rdi\n"   // 2000ms
            "int $0x80"
            : : : "rax", "rdi"
        );
    }
}

void test_process_2(void) {
    int counter = 0;
    while(1) {
        terminal_writestring("[Process 2] Running - count: ");
        // TODO: Print counter
        terminal_writestring("\n");
        counter++;
        sleep_ms(1500);  // Sleep for 1.5 seconds
    }
}

void test_process_3(void) {
    while(1) {
        terminal_writestring("[Process 3] Computing...\n");
        // CPU-intensive task
        volatile uint64_t sum = 0;
        for (volatile int i = 0; i < 10000000; i++) {
            sum += i;
        }
        terminal_writestring("[Process 3] Done computing\n");
        sleep_ms(2000);
    }
}

// Test ELF loader
void test_elf_loader(void) {
    terminal_writestring("\n=== Testing ELF Loader ===\n");
    
    // Test ELF validation with our minimal binary
    terminal_writestring("ELF binary size: ");
    // TODO: Print hello_elf_len
    terminal_writestring(" bytes\n");
    
    // Try to create process from ELF
    terminal_writestring("Creating ELF process...\n");
    process_t* proc = elf_create_process(
        (void*)hello_elf,     // Binary data
        hello_elf_len,        // Size
        "hello_elf"           // Process name
    );
    
    if (proc) {
        terminal_writestring("ELF process created successfully!\n");
        terminal_writestring("Entry point: ");
        // TODO: Print entry point address
        terminal_writestring("\n");
        terminal_writestring("Process should be ready to run\n");
    } else {
        terminal_writestring("Failed to create ELF process\n");
    }
}

// GDT structures
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uintptr_t base;
} __attribute__((packed));

#define GDT_ENTRIES 7  // Increased for TSS (uses 2 entries)
struct gdt_entry gdt[GDT_ENTRIES];
struct gdt_ptr gp;

// IDT structures
struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uintptr_t base;
} __attribute__((packed));

#define IDT_ENTRIES 256
struct idt_entry idt[IDT_ENTRIES];
struct idt_ptr ip;

// ISR function prototypes  
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void irq0(void);
extern void irq1(void);
extern void isr128(void);  // INT 0x80 syscall

// Interrupt handler table
static isr_t interrupt_handlers[256] = { 0 };

// Register an interrupt handler
void register_interrupt_handler(uint8_t n, isr_t handler) {
    interrupt_handlers[n] = handler;
}

// Set up a GDT entry
void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[num].access = access;
}

// Set up a TSS descriptor (uses 2 GDT entries in long mode)
void gdt_set_tss(int num, uint64_t base, uint32_t limit) {
    // TSS descriptor is 16 bytes (uses two GDT entries)
    struct gdt_entry* tss_low = &gdt[num];
    struct gdt_entry* tss_high = &gdt[num + 1];
    
    // Low 64 bits
    tss_low->limit_low = limit & 0xFFFF;
    tss_low->base_low = base & 0xFFFF;
    tss_low->base_middle = (base >> 16) & 0xFF;
    tss_low->access = 0x89;  // Present, ring 0, 64-bit TSS
    tss_low->granularity = ((limit >> 16) & 0x0F) | 0x00;
    tss_low->base_high = (base >> 24) & 0xFF;
    
    // High 64 bits (extended base address for 64-bit)
    tss_high->limit_low = (base >> 32) & 0xFFFF;
    tss_high->base_low = (base >> 48) & 0xFFFF;
    tss_high->base_middle = 0;
    tss_high->access = 0;
    tss_high->granularity = 0;
    tss_high->base_high = 0;
}

// Initialize GDT
void init_gdt(void) {
    gp.limit = (sizeof(struct gdt_entry) * GDT_ENTRIES) - 1;
    gp.base = (uintptr_t)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);                // Null segment
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xAF); // Kernel code segment (ring 0)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Kernel data segment (ring 0)
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xAF); // User code segment (ring 3)
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User data segment (ring 3)
    
    // TSS will be set up after GDT is loaded
    
    load_gdt((uintptr_t)&gp);
    
    // Now set up TSS (entries 5-6)
    extern tss_t tss;
    gdt_set_tss(5, (uint64_t)&tss, sizeof(tss_t) - 1);
    
    // Reload GDT to include TSS
    load_gdt((uintptr_t)&gp);
    
    // Load TSS (0x28 = GDT entry 5 * 8)
    asm volatile("ltr %0" : : "r"((uint16_t)0x28));
}

// Set up an IDT entry
void idt_set_gate(uint8_t num, uintptr_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low = base & 0xFFFF;
    idt[num].offset_middle = (base >> 16) & 0xFFFF;
    idt[num].offset_high = (base >> 32) & 0xFFFFFFFF;
    idt[num].selector = sel;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
    idt[num].ist = 0;
}

// Initialize IDT
void init_idt(void) {
    ip.limit = (sizeof(struct idt_entry) * IDT_ENTRIES) - 1;
    ip.base = (uintptr_t)&idt;

    // Use memset from string.h or implement it separately
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    // Set up CPU exception handlers (0-31)
    idt_set_gate(0, (uintptr_t)isr0, 0x08, 0x8E);   // Division by zero
    idt_set_gate(1, (uintptr_t)isr1, 0x08, 0x8E);   // Debug
    idt_set_gate(2, (uintptr_t)isr2, 0x08, 0x8E);   // NMI
    idt_set_gate(3, (uintptr_t)isr3, 0x08, 0x8E);   // Breakpoint
    idt_set_gate(4, (uintptr_t)isr4, 0x08, 0x8E);   // Overflow
    idt_set_gate(5, (uintptr_t)isr5, 0x08, 0x8E);   // Bound range
    idt_set_gate(6, (uintptr_t)isr6, 0x08, 0x8E);   // Invalid opcode
    idt_set_gate(7, (uintptr_t)isr7, 0x08, 0x8E);   // Device not available
    idt_set_gate(8, (uintptr_t)isr8, 0x08, 0x8E);   // Double fault
    idt_set_gate(10, (uintptr_t)isr10, 0x08, 0x8E); // Invalid TSS
    idt_set_gate(11, (uintptr_t)isr11, 0x08, 0x8E); // Segment not present
    idt_set_gate(12, (uintptr_t)isr12, 0x08, 0x8E); // Stack fault
    idt_set_gate(13, (uintptr_t)isr13, 0x08, 0x8E); // General protection
    idt_set_gate(14, (uintptr_t)isr14, 0x08, 0x8E); // Page fault
    idt_set_gate(16, (uintptr_t)isr16, 0x08, 0x8E); // x87 FPU error
    idt_set_gate(17, (uintptr_t)isr17, 0x08, 0x8E); // Alignment check
    idt_set_gate(18, (uintptr_t)isr18, 0x08, 0x8E); // Machine check
    idt_set_gate(19, (uintptr_t)isr19, 0x08, 0x8E); // SIMD FP
    idt_set_gate(20, (uintptr_t)isr20, 0x08, 0x8E); // Virtualization
    
    // Map hardware IRQs (32-47)
    idt_set_gate(32, (uintptr_t)irq0, 0x08, 0x8E);  // Timer
    idt_set_gate(33, (uintptr_t)irq1, 0x08, 0x8E);  // Keyboard
    
    // System call - Note: 0xEE instead of 0x8E to allow user mode access (DPL=3)
    idt_set_gate(128, (uintptr_t)isr128, 0x08, 0xEE); // INT 0x80

    load_idt((uintptr_t)&ip);
}

// Initialize the 8259 PIC
void init_pic(void) {
    // ICW1 - Initialize PICs
    outb(0x20, 0x11);  // Master PIC command port
    outb(0xA0, 0x11);  // Slave PIC command port
    
    // ICW2 - Set interrupt vector offset
    outb(0x21, 0x20);  // Master PIC vector offset (32)
    outb(0xA1, 0x28);  // Slave PIC vector offset (40)
    
    // ICW3 - Set up cascade
    outb(0x21, 0x04);  // Tell master PIC there's slave at IRQ2
    outb(0xA1, 0x02);  // Tell slave PIC its cascade identity
    
    // ICW4 - Set mode
    outb(0x21, 0x01);  // 8086 mode
    outb(0xA1, 0x01);
    
    // OCW1 - Mask interrupts (enable timer and keyboard)
    outb(0x21, 0xFC);  // Master PIC: Enable IRQ0 (timer) and IRQ1 (keyboard)
    outb(0xA1, 0xFF);  // Slave PIC: Disable all
}

// Set up paging for 64-bit long mode
void init_paging(void) {
    for (int i = 0; i < ENTRIES_PER_TABLE; i++) {
        pt[i] = (i * PAGE_SIZE) | 3; // Present + Writable
    }
    pd[0] = (uintptr_t)pt | 3;
    pdpt[0] = (uintptr_t)pd | 3;
    pml4[0] = (uintptr_t)pdpt | 3;

    enable_paging((uintptr_t*)pml4);
}

// ISR handler
void isr_handler(registers_t* regs) {
    // Handle CPU exceptions (0-31)
    if (regs->int_no < 32) {
        exception_handler(regs);
        return;
    }
    
    // Handle other interrupts
    if (interrupt_handlers[regs->int_no] != 0) {
        isr_t handler = interrupt_handlers[regs->int_no];
        handler(regs);
    } else {
        terminal_writestring("Unhandled interrupt: ");
        // TODO: Print interrupt number
        terminal_writestring("\n");
    }
}

// Kernel main function
void kernel_main(void) {
    // Initialize core systems
    init_vga();
    terminal_writestring("SimpleOS v0.2 - Now with Multitasking!\n");
    terminal_writestring("=====================================\n\n");
    
    // Initialize physical memory manager
    // For now, assume we have 64MB of physical memory starting at 2MB
    // This is a simple assumption - real OS would get this from multiboot
    pmm_init(64 * 1024 * 1024);  // 64MB
    
    init_gdt();
    tss_init();  // Initialize TSS before loading GDT with TSS
    init_pic();
    init_idt();
    init_exceptions();  // Initialize exception handlers
    init_paging();
    init_timer(100);  // 100 Hz = 10ms ticks
    
    // Initialize process and scheduling
    process_init();
    scheduler_init();
    
    init_keyboard();
    init_syscalls();  // Initialize system call interface
    fs_init();        // Initialize filesystem
    vt_init();        // Initialize virtual terminals
    terminal_enable_vt(); // Enable VT mode
    
    terminal_writestring("System initialized. Creating test processes...\n\n");
    terminal_writestring("Press Alt+F1 through Alt+F4 to switch virtual terminals\n\n");
    
    // Enable interrupts
    asm volatile("sti");
    
    // Create test processes
    process_t* p1 = process_create("TestProc1", test_process_1, 1);
    process_t* p2 = process_create("TestProc2", test_process_2, 1);
    process_t* p3 = process_create("SyscallTest", test_syscall_process, 1);
    process_t* p4 = process_create("MemoryTest", test_memory_process, 1);
    
    if (!p1 || !p2 || !p3 || !p4) {
        panic("Failed to create test processes!");
    }
    
    terminal_writestring("\nStarting scheduler...\n");
    terminal_writestring("You should see processes interleaving their output.\n");
    terminal_writestring("Commands: 'p' = process list, 's' = scheduler stats, 'f' = test page fault\n");
    terminal_writestring("          't' = test syscall, 'u' = test user mode, 'e' = test ELF loader\n");
    terminal_writestring("          'F' = test fork/exec, 'S' = start shell\n\n");
    
    // Enable scheduler - this will switch to first process
    scheduler_enable();
    
    // Kernel main becomes the idle loop
    // This code only runs when no other process is ready
    while(1) {
        // Check for debug commands
        if (keyboard_has_char()) {
            char c = keyboard_getchar();
            if (c == 'p') {
                process_print_all();
            } else if (c == 's') {
                scheduler_stats();
            } else if (c == 'f') {
                // Test page fault by accessing invalid memory
                terminal_writestring("\nTriggering page fault test...\n");
                volatile uint64_t* bad_ptr = (uint64_t*)0xDEADBEEF000;
                *bad_ptr = 42;  // This will page fault
            } else if (c == 't') {
                // Test system call from kernel
                terminal_writestring("\nTesting direct syscall from kernel...\n");
                asm volatile(
                    "mov $2, %%rax\n"      // SYS_WRITE
                    "mov $1, %%rdi\n"      // stdout
                    "lea msg(%%rip), %%rsi\n"
                    "mov $19, %%rdx\n"     // length
                    "int $0x80\n"
                    "jmp done\n"
                    "msg: .ascii \"Syscall works!\\n\\0\\0\\0\\0\"\n"
                    "done:"
                    : : : "rax", "rdi", "rsi", "rdx"
                );
            } else if (c == 'u') {
                // Test user mode
                terminal_writestring("\nTesting user mode...\n");
                test_user_mode();
            } else if (c == 'e') {
                // Test ELF loader
                test_elf_loader();
            } else if (c == 'F') {
                // Test fork/exec
                test_fork_exec();
            } else if (c == 'S') {
                // Start shell via init
                test_shell();
            }
        }
        
        // Halt CPU until next interrupt
        asm volatile("hlt");
    }
}

// Test fork/exec system calls
void test_fork_exec(void) {
    terminal_writestring("\n=== Testing Fork/Exec ===\n");
    
    // Create a test process that will fork
    process_t* test = process_create("fork_test", fork_test_main, 1);
    if (test) {
        terminal_writestring("Created fork test process\n");
    } else {
        terminal_writestring("Failed to create fork test process\n");
    }
}

// Test program that demonstrates fork
void fork_test_main(void) {
    terminal_writestring("[FORK_TEST] Starting fork test\n");
    
    // Get our PID
    uint64_t my_pid;
    asm volatile(
        "mov $4, %%rax\n"      // SYS_GETPID
        "int $0x80\n"
        "mov %%rax, %0"
        : "=r"(my_pid) : : "rax"
    );
    
    terminal_writestring("[FORK_TEST] My PID is: ");
    // TODO: Print PID
    terminal_writestring("\n");
    
    // Fork a child
    int64_t pid;
    asm volatile(
        "mov $7, %%rax\n"      // SYS_FORK
        "int $0x80\n"
        "mov %%rax, %0"
        : "=r"(pid) : : "rax"
    );
    
    if (pid == 0) {
        // Child process
        terminal_writestring("[CHILD] I'm the child process!\n");
        
        // Try to exec a program
        const char* path = "/bin/hello";
        asm volatile(
            "mov $9, %%rax\n"      // SYS_EXECVE
            "mov %0, %%rdi\n"      // path
            "mov $0, %%rsi\n"      // argv (NULL)
            "mov $0, %%rdx\n"      // envp (NULL) 
            "int $0x80"
            : : "r"(path) : "rax", "rdi", "rsi", "rdx"
        );
        
        // If exec fails, exit
        terminal_writestring("[CHILD] Exec failed, exiting\n");
        asm volatile(
            "mov $1, %%rax\n"      // SYS_EXIT
            "mov $1, %%rdi\n"      // exit code
            "int $0x80"
            : : : "rax", "rdi"
        );
    } else if (pid > 0) {
        // Parent process
        terminal_writestring("[PARENT] Created child with PID: ");
        // TODO: Print child PID
        terminal_writestring("\n");
        
        // Wait for child
        int status;
        int64_t child_pid;
        asm volatile(
            "mov $8, %%rax\n"      // SYS_WAIT
            "mov %1, %%rdi\n"      // status pointer
            "int $0x80\n"
            "mov %%rax, %0"
            : "=r"(child_pid) : "r"(&status) : "rax", "rdi"
        );
        
        terminal_writestring("[PARENT] Child exited with status: ");
        // TODO: Print status
        terminal_writestring("\n");
        
        // Exit parent
        asm volatile(
            "mov $1, %%rax\n"      // SYS_EXIT
            "mov $0, %%rdi\n"      // exit code
            "int $0x80"
            : : : "rax", "rdi"
        );
    } else {
        terminal_writestring("[FORK_TEST] Fork failed!\n");
        asm volatile(
            "mov $1, %%rax\n"      // SYS_EXIT
            "mov $1, %%rdi\n"      // exit code
            "int $0x80"
            : : : "rax", "rdi"
        );
    }
}

// External declaration for init_main (from syscall.c includes)
extern void init_main(void);

// Test shell by starting init process
void test_shell(void) {
    terminal_writestring("\n=== Starting Shell via Init ===\n");
    
    // Create init process
    process_t* init = process_create("init", init_main, 1);
    if (init) {
        terminal_writestring("Init process created! Shell should start automatically.\n");
        // Set parent_pid to 0 to make this the root process
        init->parent_pid = 0;
        
        // Start shells on other terminals
        extern int vt_get_current(void);
        extern void vt_switch(int terminal);
        
        int current_vt = vt_get_current();
        
        // Start shell on VT1
        vt_switch(1);
        terminal_writestring("\n=== Virtual Terminal 2 ===\n");
        terminal_writestring("Press 'S' to start shell on this terminal\n");
        
        // Start shell on VT2
        vt_switch(2);
        terminal_writestring("\n=== Virtual Terminal 3 ===\n");
        terminal_writestring("Press 'S' to start shell on this terminal\n");
        
        // Back to original terminal
        vt_switch(current_vt);
    } else {
        terminal_writestring("Failed to create init process\n");
    }
}