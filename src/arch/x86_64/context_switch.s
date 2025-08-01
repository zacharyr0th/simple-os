# Context switching code for x86_64
# void context_switch(context_t* old_context, context_t* new_context)
#
# Parameters:
#   %rdi - pointer to old context structure to save current state
#   %rsi - pointer to new context structure to load

.global context_switch
.type context_switch, @function

context_switch:
    # Save old context
    # Check if old_context is NULL (initial switch)
    test %rdi, %rdi
    jz load_new_context
    
    # Save callee-saved registers
    movq %r15, 0(%rdi)      # r15
    movq %r14, 8(%rdi)      # r14
    movq %r13, 16(%rdi)     # r13
    movq %r12, 24(%rdi)     # r12
    movq %rbx, 32(%rdi)     # rbx
    movq %rbp, 40(%rdi)     # rbp
    
    # Save stack pointer
    movq %rsp, 48(%rdi)     # rsp
    
    # Save return address (instruction pointer)
    movq (%rsp), %rax       # Get return address from stack
    movq %rax, 56(%rdi)     # rip
    
    # Save RFLAGS
    pushfq
    popq %rax
    movq %rax, 64(%rdi)     # rflags

load_new_context:
    # Load new context
    # Restore callee-saved registers
    movq 0(%rsi), %r15      # r15
    movq 8(%rsi), %r14      # r14
    movq 16(%rsi), %r13     # r13
    movq 24(%rsi), %r12     # r12
    movq 32(%rsi), %rbx     # rbx
    movq 40(%rsi), %rbp     # rbp
    
    # Restore RFLAGS
    movq 64(%rsi), %rax     # rflags
    pushq %rax
    popfq
    
    # Restore stack pointer
    movq 48(%rsi), %rsp     # rsp
    
    # Jump to new context (simulate return)
    movq 56(%rsi), %rax     # rip
    jmp *%rax

# Entry point for new processes
.global process_entry_trampoline
.type process_entry_trampoline, @function

process_entry_trampoline:
    # The entry point address is in %rdi (first argument)
    call *%rdi
    
    # Process returned, call exit
    movq $0, %rdi           # Exit status 0
    call process_exit
    
    # Should never reach here
1:  hlt
    jmp 1b