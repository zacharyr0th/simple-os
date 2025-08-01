#include "../include/../syscall.h"
#include "../../include/string.h"

// System call wrappers for init process
static int sys_write(int fd, const char* buf, int len) {
    int ret;
    asm volatile(
        "mov $2, %%rax\n"
        "mov %1, %%rdi\n"
        "mov %2, %%rsi\n"
        "mov %3, %%rdx\n"
        "int $0x80\n"
        "mov %%rax, %0"
        : "=r"(ret)
        : "r"(fd), "r"(buf), "r"(len)
        : "rax", "rdi", "rsi", "rdx"
    );
    return ret;
}

static int sys_fork(void) {
    int ret;
    asm volatile(
        "mov $7, %%rax\n"
        "int $0x80\n"
        "mov %%rax, %0"
        : "=r"(ret) : : "rax"
    );
    return ret;
}

static int sys_wait(int* status) {
    int ret;
    asm volatile(
        "mov $8, %%rax\n"
        "mov %1, %%rdi\n"
        "int $0x80\n"
        "mov %%rax, %0"
        : "=r"(ret)
        : "r"(status)
        : "rax", "rdi"
    );
    return ret;
}

static int sys_execve(const char* path, char* const argv[], char* const envp[]) {
    int ret;
    asm volatile(
        "mov $9, %%rax\n"
        "mov %1, %%rdi\n"
        "mov %2, %%rsi\n"
        "mov %3, %%rdx\n"
        "int $0x80\n"
        "mov %%rax, %0"
        : "=r"(ret)
        : "r"(path), "r"(argv), "r"(envp)
        : "rax", "rdi", "rsi", "rdx"
    );
    return ret;
}

static void sys_exit(int code) {
    asm volatile(
        "mov $1, %%rax\n"
        "mov %0, %%rdi\n"
        "int $0x80"
        : : "r"(code) : "rax", "rdi"
    );
}

static void sys_sleep(int ms) {
    asm volatile(
        "mov $5, %%rax\n"
        "mov %0, %%rdi\n"
        "int $0x80"
        : : "r"(ms) : "rax", "rdi"
    );
}

// Simple number to string conversion  
static void int_to_str(int num, char* buf) {
    if (num == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    
    int i = 0;
    if (num < 0) {
        buf[i++] = '-';
        num = -num;
    }
    
    int digits = 0;
    int temp = num;
    while (temp > 0) {
        digits++;
        temp /= 10;
    }
    
    buf[i + digits] = '\0';
    for (int j = digits - 1; j >= 0; j--) {
        buf[i + j] = '0' + (num % 10);
        num /= 10;
    }
}

static int str_len(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

void init_main(void) {
    sys_write(1, "[init] Starting SimpleOS init process...\n", 41);
    
    // Start the shell
    int shell_pid = sys_fork();
    if (shell_pid == 0) {
        // Child becomes shell
        char* argv[] = {"/bin/shell", 0};
        sys_execve("/bin/shell", argv, 0);
        sys_write(1, "[init] Failed to exec shell!\n", 30);
        sys_exit(1);  // Should not reach here
    } else if (shell_pid > 0) {
        char buf[64];
        sys_write(1, "[init] Shell started with PID ", 31);
        int_to_str(shell_pid, buf);
        sys_write(1, buf, str_len(buf));
        sys_write(1, "\n", 1);
    } else {
        sys_write(1, "[init] Failed to fork shell!\n", 30);
    }
    
    // Init's job: reap orphaned children
    sys_write(1, "[init] Entering main loop - reaping children\n", 46);
    
    while (1) {
        int status;
        int pid = sys_wait(&status);
        if (pid > 0) {
            char buf[128];
            sys_write(1, "[init] Reaped child PID ", 24);
            int_to_str(pid, buf);
            sys_write(1, buf, str_len(buf));
            sys_write(1, " with status ", 13);
            int_to_str(status, buf);
            sys_write(1, buf, str_len(buf));
            sys_write(1, "\n", 1);
            
            // If the shell died, restart it
            if (pid == shell_pid) {
                sys_write(1, "[init] Shell died! Restarting...\n", 34);
                shell_pid = sys_fork();
                if (shell_pid == 0) {
                    char* argv[] = {"/bin/shell", 0};
                    sys_execve("/bin/shell", argv, 0);
                    sys_exit(1);
                }
            }
        } else {
            // No children to wait for, sleep briefly
            sys_sleep(100);
        }
    }
}