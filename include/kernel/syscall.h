#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include "isr.h"

// System call numbers
#define SYS_EXIT    1
#define SYS_WRITE   2
#define SYS_READ    3
#define SYS_GETPID  4
#define SYS_SLEEP   5
#define SYS_SBRK    6
#define SYS_FORK    7
#define SYS_WAIT    8
#define SYS_EXECVE  9
#define SYS_PS      10
#define SYS_OPEN    11
#define SYS_CLOSE   12
#define SYS_STAT    13
#define SYS_MKDIR   14
#define SYS_READDIR 15
#define SYS_KILL    16
#define SYS_PIPE    17
#define SYS_DUP2    18

// Initialize system call interface
void init_syscalls(void);

// System call handler
void syscall_handler(registers_t* regs);

// User-space system call wrappers (for future use)
static inline uint64_t syscall0(uint64_t num) {
    uint64_t ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num)
        : "memory"
    );
    return ret;
}

static inline uint64_t syscall1(uint64_t num, uint64_t arg1) {
    uint64_t ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "D"(arg1)
        : "memory"
    );
    return ret;
}

static inline uint64_t syscall2(uint64_t num, uint64_t arg1, uint64_t arg2) {
    uint64_t ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2)
        : "memory"
    );
    return ret;
}

static inline uint64_t syscall3(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    uint64_t ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
        : "memory"
    );
    return ret;
}

#endif // SYSCALL_H