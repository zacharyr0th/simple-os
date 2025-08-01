#include <stdint.h>
#include <stdarg.h>
#include "../include/terminal.h"
#include "../include/isr.h"

// Helper to print hex values
static void print_hex(const char* label, uint64_t value) {
    terminal_writestring(label);
    terminal_writestring(": 0x");
    
    // Print 16 hex digits for 64-bit value
    for (int i = 60; i >= 0; i -= 4) {
        uint8_t digit = (value >> i) & 0xF;
        if (digit < 10) {
            terminal_putchar('0' + digit);
        } else {
            terminal_putchar('A' + digit - 10);
        }
    }
    terminal_writestring("  ");
}

// Kernel panic - halt system with error message
void panic(const char* msg) {
    // Disable interrupts
    asm volatile("cli");
    
    // Print panic header
    terminal_writestring("\n\n");
    terminal_writestring("================================================================================\n");
    terminal_writestring("                              KERNEL PANIC\n");
    terminal_writestring("================================================================================\n\n");
    terminal_writestring("Error: ");
    terminal_writestring(msg);
    terminal_writestring("\n\n");
    terminal_writestring("System halted. Please restart your computer.\n");
    
    // Halt forever
    while (1) {
        asm volatile("hlt");
    }
}

// Panic with register dump
void panic_with_regs(const char* msg, registers_t* regs) {
    // Disable interrupts
    asm volatile("cli");
    
    // Print panic header
    terminal_writestring("\n\n");
    terminal_writestring("================================================================================\n");
    terminal_writestring("                              KERNEL PANIC\n");
    terminal_writestring("================================================================================\n\n");
    terminal_writestring("Error: ");
    terminal_writestring(msg);
    terminal_writestring("\n\n");
    
    // Print interrupt info
    terminal_writestring("Interrupt: ");
    print_hex("INT", regs->int_no);
    print_hex("ERR", regs->err_code);
    terminal_writestring("\n\n");
    
    // Print general purpose registers
    terminal_writestring("Registers:\n");
    print_hex("RAX", regs->rax);
    print_hex("RBX", regs->rbx);
    print_hex("RCX", regs->rcx);
    print_hex("RDX", regs->rdx);
    terminal_writestring("\n");
    
    print_hex("RSI", regs->rsi);
    print_hex("RDI", regs->rdi);
    print_hex("RBP", regs->rbp);
    print_hex("RSP", regs->rsp);
    terminal_writestring("\n");
    
    print_hex("R8 ", regs->r8);
    print_hex("R9 ", regs->r9);
    print_hex("R10", regs->r10);
    print_hex("R11", regs->r11);
    terminal_writestring("\n");
    
    print_hex("R12", regs->r12);
    print_hex("R13", regs->r13);
    print_hex("R14", regs->r14);
    print_hex("R15", regs->r15);
    terminal_writestring("\n\n");
    
    // Print instruction pointer and flags
    terminal_writestring("Execution:\n");
    print_hex("RIP", regs->rip);
    print_hex("CS ", regs->cs);
    print_hex("RFLAGS", regs->rflags);
    terminal_writestring("\n\n");
    
    terminal_writestring("System halted. Please restart your computer.\n");
    
    // Halt forever
    while (1) {
        asm volatile("hlt");
    }
}

// Exception messages
static const char* exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",
    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

// General exception handler
void exception_handler(registers_t* regs) {
    if (regs->int_no < 32) {
        panic_with_regs(exception_messages[regs->int_no], regs);
    } else {
        panic_with_regs("Unknown exception", regs);
    }
}