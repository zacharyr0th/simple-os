#include "../include/../syscall.h"
#include "../../include/string.h"
#include "../../include/terminal.h"

#define MAX_CMD_LEN 256
#define MAX_ARGS 16

// System call wrappers (since we're calling them from user space)
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

static int sys_fork(void) {
    int ret;
    asm volatile(
        "mov $7, %%rax\n"      // SYS_FORK
        "int $0x80\n"
        "mov %%rax, %0"
        : "=r"(ret)
        :
        : "rax"
    );
    return ret;
}

static int sys_wait(int* status) {
    int ret;
    asm volatile(
        "mov $8, %%rax\n"      // SYS_WAIT
        "mov %1, %%rdi\n"      // status pointer
        "int $0x80\n"
        "mov %%rax, %0"
        : "=r"(ret)
        : "r"(status)
        : "rax", "rdi"
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

static int sys_execve(const char* path, char* const argv[], char* const envp[]) {
    int ret;
    asm volatile(
        "mov $9, %%rax\n"      // SYS_EXECVE
        "mov %1, %%rdi\n"      // path
        "mov %2, %%rsi\n"      // argv
        "mov %3, %%rdx\n"      // envp
        "int $0x80\n"
        "mov %%rax, %0"
        : "=r"(ret)
        : "r"(path), "r"(argv), "r"(envp)
        : "rax", "rdi", "rsi", "rdx"
    );
    return ret;
}

static void sys_sleep(int ms) {
    asm volatile(
        "mov $5, %%rax\n"      // SYS_SLEEP
        "mov %0, %%rdi\n"      // milliseconds
        "int $0x80"
        :
        : "r"(ms)
        : "rax", "rdi"
    );
}

static int sys_getpid(void) {
    int ret;
    asm volatile(
        "mov $4, %%rax\n"      // SYS_GETPID
        "int $0x80\n"
        "mov %%rax, %0"
        : "=r"(ret)
        :
        : "rax"
    );
    return ret;
}

// Simple string length function
static int str_len(const char* s) {
    int len = 0;  
    while (s[len]) len++;
    return len;
}

// Simple string compare function
static int str_cmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

// Parse command line into argv array
static int parse_command(char* cmd, char* argv[]) {
    int argc = 0;
    char* token = cmd;
    int in_token = 0;
    
    for (int i = 0; cmd[i] && argc < MAX_ARGS - 1; i++) {
        if (cmd[i] == ' ' || cmd[i] == '\t') {
            if (in_token) {
                cmd[i] = '\0';
                in_token = 0;
            }
        } else {
            if (!in_token) {
                argv[argc++] = &cmd[i];
                in_token = 1;
            }
        }
    }
    argv[argc] = 0;  // NULL terminator
    return argc;
}

// Simple number to string conversion
static void int_to_str(int num, char* buf) {
    if (num == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    
    int i = 0;
    int temp = num;
    
    // Handle negative numbers
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

void shell_main(void) {
    char cmd[MAX_CMD_LEN];
    char* argv[MAX_ARGS];
    
    sys_write(1, "\n=== SimpleOS Shell ===\n", 24);
    sys_write(1, "Type 'help' for commands\n\n", 26);
    
    while (1) {
        // Print prompt
        sys_write(1, "$ ", 2);
        
        // Read command
        int len = sys_read(0, cmd, MAX_CMD_LEN - 1);
        if (len <= 0) continue;
        
        cmd[len - 1] = '\0';  // Remove newline
        if (len == 1) continue;  // Empty line
        
        // Check for pipe
        char* pipe_pos = str_chr(cmd, '|');
        if (pipe_pos) {
            // Simple pipe handling
            *pipe_pos = '\0';
            char* cmd1 = cmd;
            char* cmd2 = pipe_pos + 1;
            
            // Skip whitespace
            while (*cmd2 == ' ' || *cmd2 == '\t') cmd2++;
            
            sys_write(1, "Pipes supported in shell_v2. Use 'S' to start enhanced shell.\n", 61);
            continue;
        }
        
        // Parse command
        int argc = parse_command(cmd, argv);
        if (argc == 0) continue;
        
        // Built-in commands
        if (str_cmp(argv[0], "help") == 0) {
            sys_write(1, "Commands:\n", 10);
            sys_write(1, "  help    - Show this help\n", 27);
            sys_write(1, "  ps      - List processes\n", 27);
            sys_write(1, "  echo    - Print arguments\n", 28);
            sys_write(1, "  fork    - Test fork\n", 22);
            sys_write(1, "  stress  - Stress test\n", 24);
            sys_write(1, "  ls      - List files\n", 23);
            sys_write(1, "  cat     - Show file contents\n", 31);
            sys_write(1, "  kill    - Kill a process\n", 27);
            sys_write(1, "  wc      - Count lines/words/chars\n", 36);
            sys_write(1, "  grep    - Search for pattern\n", 31);
            sys_write(1, "  clear   - Clear screen\n", 25);
            sys_write(1, "  exit    - Exit shell\n", 23);
            sys_write(1, "\nPipes: cmd1 | cmd2 (e.g. ps | grep shell)\n", 43);
        }
        else if (str_cmp(argv[0], "ps") == 0) {
            // Call the real sys_ps system call
            asm volatile(
                "mov $10, %%rax\n"     // SYS_PS
                "int $0x80"
                : : : "rax"
            );
        }
        else if (str_cmp(argv[0], "echo") == 0) {
            for (int i = 1; i < argc; i++) {
                sys_write(1, argv[i], str_len(argv[i]));
                if (i < argc - 1) sys_write(1, " ", 1);
            }
            sys_write(1, "\n", 1);
        }
        else if (str_cmp(argv[0], "fork") == 0) {
            int pid = sys_fork();
            if (pid == 0) {
                sys_write(1, "Child process running!\n", 23);
                sys_sleep(2000);
                sys_write(1, "Child exiting\n", 14);
                sys_exit(0);
            } else if (pid > 0) {
                char buf[64];
                sys_write(1, "Created child PID ", 18);
                int_to_str(pid, buf);
                sys_write(1, buf, str_len(buf));
                sys_write(1, "\n", 1);
                
                int status;
                sys_wait(&status);
                sys_write(1, "Child finished\n", 15);
            } else {
                sys_write(1, "Fork failed!\n", 13);
            }
        }
        else if (str_cmp(argv[0], "stress") == 0) {
            sys_write(1, "Starting stress test...\n", 24);
            
            for (int i = 0; i < 3; i++) {
                int pid = sys_fork();
                if (pid == 0) {
                    // Child: do some work
                    char buf[64];
                    sys_write(1, "Worker ", 7);
                    int_to_str(sys_getpid(), buf);
                    sys_write(1, buf, str_len(buf));
                    sys_write(1, " running\n", 9);
                    
                    sys_sleep(1000 + i * 500);
                    sys_exit(i);
                }
            }
            
            // Parent: wait for all children  
            for (int i = 0; i < 3; i++) {
                int status;
                int pid = sys_wait(&status);
                char buf[64];
                sys_write(1, "Child ", 6);
                int_to_str(pid, buf);
                sys_write(1, buf, str_len(buf));
                sys_write(1, " exited\n", 8);
            }
            
            sys_write(1, "Stress test complete!\n", 22);
        }
        else if (str_cmp(argv[0], "clear") == 0) {
            sys_write(1, "\033[2J\033[H", 7);  // ANSI clear screen
        }
        else if (str_cmp(argv[0], "ls") == 0) {
            // Open root directory
            int fd;
            asm volatile(
                "mov $11, %%rax\n"     // SYS_OPEN
                "lea root_path(%%rip), %%rdi\n"
                "mov $0, %%rsi\n"      // flags
                "mov $0, %%rdx\n"      // mode
                "int $0x80\n"
                "mov %%rax, %0\n"
                "jmp ls_done\n"
                "root_path: .asciz \"/\"\n"
                "ls_done:"
                : "=r"(fd) : : "rax", "rdi", "rsi", "rdx"
            );
            
            if (fd >= 0) {
                // Read directory entries
                struct {
                    char name[32];
                    uint32_t type;
                } dirent;
                
                while (1) {
                    int ret;
                    asm volatile(
                        "mov $15, %%rax\n"     // SYS_READDIR
                        "mov %1, %%rdi\n"      // fd
                        "mov %2, %%rsi\n"      // dirent buffer
                        "int $0x80\n"
                        "mov %%rax, %0"
                        : "=r"(ret)
                        : "r"(fd), "r"(&dirent)
                        : "rax", "rdi", "rsi"
                    );
                    
                    if (ret <= 0) break;
                    
                    sys_write(1, dirent.name, str_len(dirent.name));
                    sys_write(1, "\n", 1);
                }
                
                // Close directory
                asm volatile(
                    "mov $12, %%rax\n"     // SYS_CLOSE
                    "mov %0, %%rdi\n"      // fd
                    "int $0x80"
                    : : "r"(fd) : "rax", "rdi"
                );
            } else {
                sys_write(1, "Failed to open directory\n", 25);
            }
        }
        else if (str_cmp(argv[0], "cat") == 0) {
            if (argc < 2) {
                sys_write(1, "Usage: cat <filename>\n", 22);
            } else {
                // Open file
                int fd;
                asm volatile(
                    "mov $11, %%rax\n"     // SYS_OPEN
                    "mov %1, %%rdi\n"      // filename
                    "mov $0, %%rsi\n"      // flags (read)
                    "mov $0, %%rdx\n"      // mode
                    "int $0x80\n"
                    "mov %%rax, %0"
                    : "=r"(fd)
                    : "r"(argv[1])
                    : "rax", "rdi", "rsi", "rdx"
                );
                
                if (fd >= 0) {
                    // Read and display file
                    char buffer[256];
                    while (1) {
                        int bytes;
                        asm volatile(
                            "mov $3, %%rax\n"      // SYS_READ
                            "mov %1, %%rdi\n"      // fd
                            "mov %2, %%rsi\n"      // buffer
                            "mov $256, %%rdx\n"    // count
                            "int $0x80\n"
                            "mov %%rax, %0"
                            : "=r"(bytes)
                            : "r"(fd), "r"(buffer)
                            : "rax", "rdi", "rsi", "rdx"
                        );
                        
                        if (bytes <= 0) break;
                        sys_write(1, buffer, bytes);
                    }
                    
                    // Close file
                    asm volatile(
                        "mov $12, %%rax\n"     // SYS_CLOSE
                        "mov %0, %%rdi\n"      // fd
                        "int $0x80"
                        : : "r"(fd) : "rax", "rdi"
                    );
                } else {
                    sys_write(1, "File not found: ", 16);
                    sys_write(1, argv[1], str_len(argv[1]));
                    sys_write(1, "\n", 1);
                }
            }
        }
        else if (str_cmp(argv[0], "kill") == 0) {
            if (argc < 2) {
                sys_write(1, "Usage: kill <pid>\n", 18);
            } else {
                // Simple atoi for pid
                int pid = 0;
                for (int i = 0; argv[1][i]; i++) {
                    if (argv[1][i] >= '0' && argv[1][i] <= '9') {
                        pid = pid * 10 + (argv[1][i] - '0');
                    } else {
                        sys_write(1, "Invalid PID\n", 12);
                        pid = -1;
                        break;
                    }
                }
                
                if (pid > 0) {
                    // Send SIGKILL
                    asm volatile(
                        "mov $16, %%rax\n"     // SYS_KILL (need to add this)
                        "mov %0, %%rdi\n"      // pid
                        "mov $9, %%rsi\n"      // SIGKILL
                        "int $0x80"
                        : : "r"(pid) : "rax", "rdi", "rsi"
                    );
                    sys_write(1, "Sent SIGKILL to process ", 24);
                    sys_write(1, argv[1], str_len(argv[1]));
                    sys_write(1, "\n", 1);
                }
            }
        }
        else if (str_cmp(argv[0], "wc") == 0) {
            // Word count - read from stdin
            int lines = 0, words = 0, chars = 0;
            int in_word = 0;
            char buffer[256];
            
            while (1) {
                int bytes = sys_read(0, buffer, sizeof(buffer));
                if (bytes <= 0) break;
                
                for (int i = 0; i < bytes; i++) {
                    chars++;
                    if (buffer[i] == '\n') lines++;
                    
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
            
            if (in_word) words++;
            
            char buf[16];
            sys_write(1, "  ", 2);
            int_to_str(lines, buf);
            sys_write(1, buf, str_len(buf));
            sys_write(1, "  ", 2);
            int_to_str(words, buf);
            sys_write(1, buf, str_len(buf));
            sys_write(1, "  ", 2);
            int_to_str(chars, buf);
            sys_write(1, buf, str_len(buf));
            sys_write(1, "\n", 1);
        }
        else if (str_cmp(argv[0], "grep") == 0) {
            if (argc < 2) {
                sys_write(1, "Usage: grep <pattern>\n", 22);
            } else {
                char line[256];
                int line_pos = 0;
                
                while (1) {
                    char c;
                    int bytes = sys_read(0, &c, 1);
                    if (bytes <= 0) {
                        if (line_pos > 0) {
                            line[line_pos] = '\0';
                            if (str_chr(line, argv[1][0])) {  // Simple search
                                sys_write(1, line, str_len(line));
                                sys_write(1, "\n", 1);
                            }
                        }
                        break;
                    }
                    
                    if (c == '\n') {
                        line[line_pos] = '\0';
                        if (str_chr(line, argv[1][0])) {  // Simple search
                            sys_write(1, line, str_len(line));
                            sys_write(1, "\n", 1);
                        }
                        line_pos = 0;
                    } else if (line_pos < 255) {
                        line[line_pos++] = c;
                    }
                }
            }
        }
        else if (str_cmp(argv[0], "exit") == 0) {
            sys_write(1, "Goodbye!\n", 9);
            sys_exit(0);
        }
        else {
            // Try to execute external program
            int pid = sys_fork();
            if (pid == 0) {
                // Child: try to exec
                sys_execve(argv[0], argv, 0);
                // If we get here, exec failed
                sys_write(1, "Command not found: ", 19);
                sys_write(1, argv[0], str_len(argv[0]));
                sys_write(1, "\n", 1);
                sys_exit(1);
            } else if (pid > 0) {
                // Parent: wait for child
                int status;
                sys_wait(&status);
            }
        }
    }
}