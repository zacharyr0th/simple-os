#include "../../include/string.h"

// Simple grep implementation
// Reads from stdin and prints lines containing pattern

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

// String functions
static int str_len(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

static char* str_str(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    
    for (; *haystack; haystack++) {
        const char* h = haystack;
        const char* n = needle;
        
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        
        if (!*n) return (char*)haystack;
    }
    
    return 0;
}

void grep_main(int argc, char* argv[]) {
    if (argc < 2) {
        sys_write(2, "Usage: grep <pattern>\n", 22);
        sys_exit(1);
    }
    
    const char* pattern = argv[1];
    char line[256];
    int line_pos = 0;
    
    // Read from stdin line by line
    while (1) {
        char c;
        int bytes = sys_read(0, &c, 1);
        
        if (bytes <= 0) {
            // EOF or error
            if (line_pos > 0) {
                // Process last line
                line[line_pos] = '\0';
                if (str_str(line, pattern)) {
                    sys_write(1, line, str_len(line));
                    sys_write(1, "\n", 1);
                }
            }
            break;
        }
        
        if (c == '\n') {
            // End of line
            line[line_pos] = '\0';
            if (str_str(line, pattern)) {
                sys_write(1, line, str_len(line));
                sys_write(1, "\n", 1);
            }
            line_pos = 0;
        } else if (line_pos < 255) {
            line[line_pos++] = c;
        }
    }
    
    sys_exit(0);
}