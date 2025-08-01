# This assembly file contains low-level functions for x86_64 system initialization and interrupt handling.
# It includes routines for loading GDT and IDT, enabling paging, and handling interrupt service routines (ISRs).

.global load_gdt
.global load_idt
.global enable_paging
.global isr0
.global isr1
.global isr2
.global isr3
.global isr4
.global isr5
.global isr6
.global isr7
.global isr8
.global isr10
.global isr11
.global isr12
.global isr13
.global isr14
.global isr16
.global isr17
.global isr18
.global isr19
.global isr20
.global irq0
.global irq1
.global isr128

load_gdt:
    lgdt (%rdi)
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    pushq $0x08
    pushq $.reload_cs
    retfq
.reload_cs:
    ret

load_idt:
    lidt (%rdi)
    ret

enable_paging:
    mov %rdi, %cr3
    mov %cr0, %rax
    mov $0x80000001, %rbx
    or %rbx, %rax
    mov %rax, %cr0
    ret

# CPU exceptions (0-31)
# Some exceptions push error codes, others don't

# 0: Divide by zero
isr0:
    cli
    pushq $0        # Dummy error code
    pushq $0        # Interrupt number
    jmp isr_common_stub

# 1: Debug
isr1:
    cli
    pushq $0        # Dummy error code
    pushq $1
    jmp isr_common_stub

# 2: Non-maskable interrupt
isr2:
    cli
    pushq $0
    pushq $2
    jmp isr_common_stub

# 3: Breakpoint
isr3:
    cli
    pushq $0
    pushq $3
    jmp isr_common_stub

# 4: Overflow
isr4:
    cli
    pushq $0
    pushq $4
    jmp isr_common_stub

# 5: Bound range exceeded
isr5:
    cli
    pushq $0
    pushq $5
    jmp isr_common_stub

# 6: Invalid opcode
isr6:
    cli
    pushq $0
    pushq $6
    jmp isr_common_stub

# 7: Device not available
isr7:
    cli
    pushq $0
    pushq $7
    jmp isr_common_stub

# 8: Double fault (pushes error code)
isr8:
    cli
    pushq $8        # Error code already pushed by CPU
    jmp isr_common_stub

# 10: Invalid TSS (pushes error code)
isr10:
    cli
    pushq $10
    jmp isr_common_stub

# 11: Segment not present (pushes error code)
isr11:
    cli
    pushq $11
    jmp isr_common_stub

# 12: Stack segment fault (pushes error code)
isr12:
    cli
    pushq $12
    jmp isr_common_stub

# 13: General protection fault (pushes error code)
isr13:
    cli
    pushq $13
    jmp isr_common_stub

# 14: Page fault (pushes error code)
isr14:
    cli
    pushq $14
    jmp isr_common_stub

# 16: x87 FPU error
isr16:
    cli
    pushq $0
    pushq $16
    jmp isr_common_stub

# 17: Alignment check (pushes error code)
isr17:
    cli
    pushq $17
    jmp isr_common_stub

# 18: Machine check
isr18:
    cli
    pushq $0
    pushq $18
    jmp isr_common_stub

# 19: SIMD floating-point
isr19:
    cli
    pushq $0
    pushq $19
    jmp isr_common_stub

# 20: Virtualization
isr20:
    cli
    pushq $0
    pushq $20
    jmp isr_common_stub

isr_common_stub:
    # Save all registers
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rbx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # Pass register struct pointer to handler
    mov %rsp, %rdi
    call isr_handler

    # Restore all registers
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rbx
    popq %rdx
    popq %rcx
    popq %rax

    # Clean up error code and interrupt number
    addq $16, %rsp
    sti
    iretq

# IRQ handlers
irq0:
    cli
    pushq $0
    pushq $32
    jmp isr_common_stub

irq1:
    cli
    pushq $0
    pushq $33
    jmp isr_common_stub

# System call handler (INT 0x80 = 128)
isr128:
    cli
    pushq $0
    pushq $128
    jmp isr_common_stub
