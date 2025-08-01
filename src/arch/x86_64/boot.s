# Multiboot2 header and boot code for SimpleOS
# This file sets up the initial boot environment and calls the kernel

.section .multiboot
.align 8
multiboot_header:
    # Magic number
    .long 0xe85250d6                # Multiboot2 magic
    .long 0                         # Architecture (0 = i386/x86-64)
    .long multiboot_header_end - multiboot_header  # Header length
    .long -(0xe85250d6 + 0 + (multiboot_header_end - multiboot_header))  # Checksum

    # End tag
    .word 0                         # Type
    .word 0                         # Flags
    .long 8                         # Size
multiboot_header_end:

.section .bss
.align 16
stack_bottom:
    .skip 16384                     # 16 KiB stack
stack_top:

.section .text
.global _start
.type _start, @function
_start:
    # Set up stack
    mov $stack_top, %rsp
    
    # Clear RFLAGS
    push $0
    popf
    
    # Pass multiboot info to kernel_main
    push %rbx                       # Multiboot info structure
    push %rax                       # Multiboot magic number
    
    # Call kernel
    call kernel_main
    
    # If kernel returns, hang
    cli
1:  hlt
    jmp 1b

.size _start, . - _start