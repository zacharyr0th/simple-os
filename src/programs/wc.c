#include "../../include/string.h"

// Simple wc (word count) implementation
// Counts lines, words, and characters from stdin

// System call wrappers
static int sys_write(int fd, const char* buf, int len) {
    int ret;
    asm volatile(
        "mov $2, %%rax\n"      // SYS_WRITE
        "mov %1, %%rdi\n"      // fd
        "mov %2, %%rsi\n"      // buffer
        "mov %3, %%rdx\n"      // length
        "int $0x80\n"
        "mov %%rax, %0"
        : "=r"(ret)
        : "r"(fd), "r"(buf), "r"(len)
        : "rax", "rdi", "rsi", "rdx"
    );
    return ret;
}

static int sys_read(int fd, char* buf, int len) {
    int ret;
    asm volatile(
        "mov $3, %%rax\n"      // SYS_READ
        "mov %1, %%rdi\n"      // fd
        "mov %2, %%rsi\n"      // buffer
        "mov %3, %%rdx\n"      // length
        "int $0x80\n"
        "mov %%rax, %0"
        : "=r"(ret)
        : "r"(fd), "r"(buf), "r"(len)
        : "rax", "rdi", "rsi", "rdx"
    );
    return ret;
}

static void sys_exit(int code) {
    asm volatile(
        "mov $1, %%rax\n"      // SYS_EXIT
        "mov %0, %%rdi\n"      // exit code
        "int $0x80"
        :
        : "r"(code)
        : "rax", "rdi"
    );
}

// Number to string conversion
static void int_to_str(int num, char* buf) {
    if (num == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    
    int i = 0;
    int temp = num;
    
    if (num < 0) {
        buf[i++] = '-';
        num = -num;
    }
    
    // Count digits
    int digits = 0;
    temp = num;
    while (temp > 0) {
        digits++;
        temp /= 10;
    }
    
    // Fill buffer
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

void wc_main(void) {
    int lines = 0;
    int words = 0;
    int chars = 0;
    int in_word = 0;
    
    char buffer[256];
    
    // Read from stdin
    while (1) {
        int bytes = sys_read(0, buffer, sizeof(buffer));
        if (bytes <= 0) break;
        
        for (int i = 0; i < bytes; i++) {
            chars++;
            
            if (buffer[i] == '\n') {
                lines++;
            }
            
            // Count words
            if (buffer[i] == ' ' || buffer[i] == '\t' || buffer[i] == '\n') {
                if (in_word) {
                    words++;
                    in_word = 0;
                }
            } else {
                in_word = 1;
            }
        }
    }
    
    // Count last word if needed
    if (in_word) {
        words++;
    }
    
    // Output results
    char num_buf[32];
    
    // Lines
    int_to_str(lines, num_buf);
    sys_write(1, "  ", 2);
    sys_write(1, num_buf, str_len(num_buf));
    
    // Words
    int_to_str(words, num_buf);
    sys_write(1, "  ", 2);
    sys_write(1, num_buf, str_len(num_buf));
    
    // Characters
    int_to_str(chars, num_buf);
    sys_write(1, "  ", 2);
    sys_write(1, num_buf, str_len(num_buf));
    
    sys_write(1, "\n", 1);
    
    sys_exit(0);
}