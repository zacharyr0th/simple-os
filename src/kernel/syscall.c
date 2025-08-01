#include <stdint.h>
#include <stddef.h>
#include "../include/isr.h"
#include "../include/terminal.h"
#include "../include/process.h"
#include "../include/scheduler.h"
#include "../include/timer.h"
#include "../include/keyboard.h"
#include "../include/kmalloc.h"
#include "../include/vmm.h"
#include "../include/scheduler.h"
#include "../include/string.h"
#include "../include/pmm.h"
#include "../include/fs.h"
#include "../include/pipe.h"

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

// File descriptors
#define STDIN   0
#define STDOUT  1
#define STDERR  2

// Maximum number of system calls
#define MAX_SYSCALLS 64

// System call function type
typedef uint64_t (*syscall_func_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

// System call table
static syscall_func_t syscall_table[MAX_SYSCALLS];

// Forward declarations
void builtin_hello_main(void);
void shell_main(void);
void shell_v2_main(void);
void init_main(void);
static void string_concat(char* dest, const char* src);
static void int_to_string(uint32_t num, char* buf);

// System call implementations

// sys_exit: Terminate current process
static uint64_t sys_exit(uint64_t status, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    process_t* current = process_get_current();
    if (!current) {
        return 0;
    }
    
    terminal_writestring("\n[SYSCALL] Process ");
    terminal_writestring(current->name);
    terminal_writestring(" exiting with status: ");
    // TODO: Print status
    terminal_writestring("\n");
    
    // Set exit status and become zombie
    current->exit_status = (int)status;
    current->state = PROCESS_STATE_ZOMBIE;
    
    // If we have a parent, we become a zombie
    // If no parent (PID 0), we can be cleaned up immediately
    if (current->parent_pid == 0) {
        process_exit((int)status);
    } else {
        // Become zombie and schedule next process
        schedule();
    }
    
    // Should never reach here
    return 0;
}

// sys_write: Write to file descriptor
static uint64_t sys_write(uint64_t fd, uint64_t buf_ptr, uint64_t count, uint64_t arg4, uint64_t arg5) {
    (void)arg4; (void)arg5;
    
    // Validate parameters
    if (buf_ptr == 0 || count == 0) {
        return 0;
    }
    
    const char* buf = (const char*)buf_ptr;
    
    // Handle stdout and stderr
    if (fd == STDOUT || fd == STDERR) {
        for (size_t i = 0; i < count; i++) {
            terminal_putchar(buf[i]);
        }
        return count;
    }
    
    // Handle file descriptors
    fd_entry_t* fd_table = get_fd_table();
    if (fd_table && fd < MAX_FDS) {
        if (fd_table[fd].is_pipe && fd_table[fd].pipe) {
            // Write to pipe
            return pipe_write(fd_table[fd].pipe, buf, count);
        } else if (fd_table[fd].node) {
            // Write to file
            fs_node_t* node = fd_table[fd].node;
            int written = fs_write(node, fd_table[fd].offset, count, (uint8_t*)buf);
            if (written > 0) {
                fd_table[fd].offset += written;
            }
            return written;
        }
    }
    
    // Unsupported fd
    return -1;
}

// sys_read: Read from file descriptor
static uint64_t sys_read(uint64_t fd, uint64_t buf_ptr, uint64_t count, uint64_t arg4, uint64_t arg5) {
    (void)arg4; (void)arg5;
    
    // Validate parameters
    if (buf_ptr == 0 || count == 0) {
        return 0;
    }
    
    char* buf = (char*)buf_ptr;
    
    // Handle stdin
    if (fd == STDIN) {
        size_t read = 0;
        
        // Block until we have input
        while (read < count) {
            if (keyboard_has_char()) {
                char c = keyboard_getchar();
                buf[read++] = c;
                
                // Don't echo here - keyboard driver already does it
                
                // Stop on newline
                if (c == '\n') {
                    break;
                }
            } else {
                // Yield CPU while waiting for input
                asm volatile("hlt");
            }
        }
        
        return read;
    }
    
    // Handle file descriptors
    fd_entry_t* fd_table = get_fd_table();
    if (fd_table && fd < MAX_FDS) {
        if (fd_table[fd].is_pipe && fd_table[fd].pipe) {
            // Read from pipe
            return pipe_read(fd_table[fd].pipe, buf, count);
        } else if (fd_table[fd].node) {
            // Read from file
            fs_node_t* node = fd_table[fd].node;
            int bytes_read = fs_read(node, fd_table[fd].offset, count, (uint8_t*)buf);
            if (bytes_read > 0) {
                fd_table[fd].offset += bytes_read;
            }
            return bytes_read;
        }
    }
    
    // Unsupported fd
    return -1;
}

// sys_getpid: Get current process ID
static uint64_t sys_getpid(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    return process_get_pid();
}

// sys_sleep: Sleep for milliseconds
static uint64_t sys_sleep(uint64_t ms, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    sleep_ms((uint32_t)ms);
    return 0;
}

// sys_sbrk: Extend data segment
static uint64_t sys_sbrk(uint64_t increment, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    process_t* current = process_get_current();
    if (!current) {
        return -1;
    }
    
    uint64_t old_heap = current->heap_current;
    
    // If increment is 0, just return current break
    if (increment == 0) {
        return old_heap;
    }
    
    uint64_t new_heap = current->heap_current + (int64_t)increment;
    
    // Check limits
    if (new_heap > current->heap_max) {
        return -1;  // ENOMEM
    }
    
    if (new_heap < current->heap_start) {
        return -1;  // Can't go below heap start
    }
    
    if ((int64_t)increment > 0) {
        // Growing heap - allocate new pages as needed
        uint64_t old_page = current->heap_current & ~(PAGE_SIZE - 1);
        uint64_t new_page = (new_heap + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        
        for (uint64_t page = old_page; page < new_page; page += PAGE_SIZE) {
            if (page >= current->heap_current) {
                // Allocate new page
                if (vmm_alloc_user_pages(current, page, 1) < 0) {
                    return -1;  // Failed to allocate
                }
            }
        }
    } else {
        // Shrinking heap - could free pages here
        // For now, just update the pointer
    }
    
    current->heap_current = new_heap;
    return old_heap;
}

// sys_fork: Create a child process
static uint64_t sys_fork(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    process_t* parent = process_get_current();
    if (!parent) {
        return -1;
    }
    
    terminal_writestring("[FORK] Starting fork from PID ");
    // TODO: Print parent PID
    terminal_writestring("\n");
    
    // Create child process structure
    process_t* child = allocate_process_struct();
    if (!child) {
        terminal_writestring("[FORK] Failed to allocate child process\n");
        return -1;  // EAGAIN - no resources
    }
    
    // Copy process name with [child] suffix
    strncpy(child->name, parent->name, 24);
    string_concat(child->name, "[child]");
    
    // Clone address space
    child->page_table = vmm_clone_address_space(parent->page_table);
    if (!child->page_table) {
        terminal_writestring("[FORK] Failed to clone address space\n");
        free_process_struct(child);
        return -1;
    }
    
    // Copy process context (registers, etc)
    child->context = parent->context;
    
    // NOTE: Return values are set by the syscall handler in the register context
    // The child will get 0, parent will get child PID when they run
    
    // Copy other process state
    child->parent_pid = parent->pid;
    child->state = PROCESS_STATE_READY;
    child->priority = parent->priority;
    child->ticks_remaining = DEFAULT_QUANTUM;
    child->ticks_total = 0;
    
    // Copy memory layout info
    child->heap_start = parent->heap_start;
    child->heap_current = parent->heap_current;
    child->heap_max = parent->heap_max;
    child->stack_bottom = parent->stack_bottom;
    child->stack_top = parent->stack_top;
    child->pages_allocated = parent->pages_allocated;
    child->page_faults = 0;
    
    // Copy file descriptor table
    if (parent->fd_table && child->fd_table) {
        fd_entry_t* parent_fds = (fd_entry_t*)parent->fd_table;
        fd_entry_t* child_fds = (fd_entry_t*)child->fd_table;
        
        for (int i = 0; i < MAX_FDS; i++) {
            child_fds[i] = parent_fds[i];
            // TODO: Increment reference counts for files/pipes
        }
    }
    
    // Add to ready queue
    ready_queue_push(child);
    
    terminal_writestring("[FORK] Created child PID ");
    // TODO: Print child PID
    terminal_writestring("\n");
    
    return child->pid;  // Return child PID to parent
}

// sys_wait: Wait for child process to exit
static uint64_t sys_wait(uint64_t status_ptr, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    process_t* parent = process_get_current();
    if (!parent) {
        return -1;
    }
    
    terminal_writestring("[WAIT] Process waiting for child\n");
    
    while (1) {
        // Look for zombie children
        process_t* child = find_zombie_child(parent->pid);
        if (child) {
            // Found a zombie child
            if (status_ptr) {
                int* status = (int*)status_ptr;
                *status = child->exit_status;
            }
            
            uint32_t child_pid = child->pid;
            
            terminal_writestring("[WAIT] Reaping child PID ");
            // TODO: Print child PID
            terminal_writestring("\n");
            
            // Clean up child
            free_process_struct(child);
            
            return child_pid;
        }
        
        // No zombie children, block and wait
        parent->state = PROCESS_STATE_WAITING;
        schedule();  // Switch to another process
        
        // When we get here, we've been woken up - check again
    }
}

// sys_execve: Execute a new program
static uint64_t sys_execve(uint64_t path_ptr, uint64_t argv_ptr, uint64_t envp_ptr, uint64_t arg4, uint64_t arg5) {
    (void)argv_ptr; (void)envp_ptr; (void)arg4; (void)arg5;
    
    const char* path = (const char*)path_ptr;
    process_t* current = process_get_current();
    
    if (!current || !path) {
        return -1;
    }
    
    terminal_writestring("[EXEC] Executing: ");
    terminal_writestring(path);
    terminal_writestring("\n");
    
    // Built-in program table
    struct builtin_program {
        const char* path;
        void (*entry)(void);
        const char* name;
    } builtins[] = {
        {"/bin/hello", builtin_hello_main, "hello"},
        {"/bin/shell", shell_main, "shell"},
        {"/bin/init", init_main, "init"},
        {"/bin/shell_v2", shell_v2_main, "shell_v2"},  // Enhanced shell
        {0, 0, 0}  // NULL terminator
    };
    
    // Check built-in programs
    for (int i = 0; builtins[i].path; i++) {
        if (strcmp(path, builtins[i].path) == 0) {
            terminal_writestring("[EXEC] Loading built-in program: ");
            terminal_writestring(builtins[i].name);
            terminal_writestring("\n");
            
            // Clear current address space (except kernel mappings)
            vmm_clear_user_space(current->page_table);
            
            // Set up new process state
            current->context.rip = (uint64_t)builtins[i].entry;
            current->context.rsp = USER_STACK_TOP - 16;
            current->context.rflags = 0x202;
            
            // Update process name
            strncpy(current->name, builtins[i].name, 31);
            current->name[31] = '\0';
            
            return 0;  // execve doesn't return on success
        }
    }
    
    terminal_writestring("[EXEC] Program not found: ");
    terminal_writestring(path);
    terminal_writestring("\n");
    return -1;  // ENOENT
}

// sys_ps: List processes
static uint64_t sys_ps(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    terminal_writestring("PID  PPID  STATE     NAME\n");
    terminal_writestring("---  ----  --------  ----------\n");
    
    // Iterate through process table
    extern process_t* process_table[];
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* p = process_table[i];
        if (p) {
            const char* state_str = "UNKNOWN";
            switch (p->state) {
                case PROCESS_STATE_READY:      state_str = "READY"; break;
                case PROCESS_STATE_RUNNING:    state_str = "RUN"; break;
                case PROCESS_STATE_BLOCKED:    state_str = "BLOCK"; break;
                case PROCESS_STATE_WAITING:    state_str = "WAIT"; break;
                case PROCESS_STATE_ZOMBIE:     state_str = "ZOMBIE"; break;
                case PROCESS_STATE_TERMINATED: state_str = "TERM"; break;
            }
            
            // Print PID (simple - just show first digit for now)
            char pid_str[16];
            int_to_string(p->pid, pid_str);
            terminal_writestring(pid_str);
            terminal_writestring("    ");
            
            // Print PPID
            char ppid_str[16];
            int_to_string(p->parent_pid, ppid_str);
            terminal_writestring(ppid_str);
            terminal_writestring("    ");
            
            // Print STATE
            terminal_writestring(state_str);
            
            // Pad to align NAME column
            int state_len = 0;
            while (state_str[state_len]) state_len++;
            for (int j = state_len; j < 8; j++) {
                terminal_writestring(" ");
            }
            terminal_writestring("  ");
            
            // Print NAME
            terminal_writestring(p->name);
            terminal_writestring("\n");
        }
    }
    
    return 0;
}

// Helper function to convert int to string for ps command
static void int_to_string(uint32_t num, char* buf) {
    if (num == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    
    uint32_t temp = num;
    
    // Count digits
    int digits = 0;
    while (temp > 0) {
        digits++;
        temp /= 10;
    }
    
    // Fill buffer
    buf[digits] = '\0';
    for (int j = digits - 1; j >= 0; j--) {
        buf[j] = '0' + (num % 10);
        num /= 10;
    }
}

// Helper function for string concatenation (string.h doesn't have this)
static void string_concat(char* dest, const char* src) {
    while (*dest) dest++;  // Find end of dest
    while (*src) {
        *dest++ = *src++;  // Copy src to end of dest
    }
    *dest = '\0';
}

// Built-in program for testing exec
void builtin_hello_main(void) {
    terminal_writestring("[BUILTIN] Hello program started!\n");
    
    // Use system calls to show we're running in user space
    asm volatile(
        "mov $2, %%rax\n"      // SYS_WRITE
        "mov $1, %%rdi\n"      // stdout
        "lea msg(%%rip), %%rsi\n"
        "mov $25, %%rdx\n"     // length
        "int $0x80\n"
        "jmp done\n"
        "msg: .ascii \"Hello from exec program!\\n\"\n"
        "done:"
        : : : "rax", "rdi", "rsi", "rdx"
    );
    
    // Exit cleanly
    asm volatile(
        "mov $1, %%rax\n"      // SYS_EXIT
        "mov $42, %%rdi\n"     // exit code
        "int $0x80"
        : : : "rax", "rdi"
    );
}

// Simple inline shell implementation
void shell_main(void) {
    terminal_writestring("\n=== SimpleOS Shell ===\n");
    terminal_writestring("Type 'help' for commands\n\n");
    
    while (1) {
        terminal_writestring("$ ");
        
        // For now, just provide a simple demo
        // In real implementation, we'd read from keyboard and process commands
        terminal_writestring("Shell running! Press 'F' to test fork/exec instead.\n");
        terminal_writestring("This is a demo shell. Real shell needs keyboard input integration.\n");
        
        // Simple loop for demo
        for (volatile int i = 0; i < 1000000; i++) {
            // Busy wait
        }
        
        // Exit after demo
        asm volatile(
            "mov $1, %%rax\n"      // SYS_EXIT
            "mov $0, %%rdi\n"      // exit code
            "int $0x80"
            : : : "rax", "rdi"
        );
    }
}

// Simple inline init implementation  
void init_main(void) {
    terminal_writestring("[init] Starting SimpleOS init process...\n");
    
    // Fork to create shell
    int64_t shell_pid;
    asm volatile(
        "mov $7, %%rax\n"      // SYS_FORK
        "int $0x80\n"
        "mov %%rax, %0"
        : "=r"(shell_pid) : : "rax"
    );
    
    if (shell_pid == 0) {
        // Child: exec shell
        const char* path = "/bin/shell";
        asm volatile(
            "mov $9, %%rax\n"      // SYS_EXECVE
            "mov %0, %%rdi\n"      // path
            "mov $0, %%rsi\n"      // argv (NULL)
            "mov $0, %%rdx\n"      // envp (NULL)
            "int $0x80"
            : : "r"(path) : "rax", "rdi", "rsi", "rdx"
        );
        
        // If exec fails
        terminal_writestring("[init] Failed to exec shell!\n");
        asm volatile(
            "mov $1, %%rax\n"      // SYS_EXIT
            "mov $1, %%rdi\n"      // exit code
            "int $0x80"
            : : : "rax", "rdi"
        );
    } else if (shell_pid > 0) {
        terminal_writestring("[init] Shell started, entering reaper loop\n");
        
        // Parent: reap children
        while (1) {
            int status;
            int64_t pid;
            asm volatile(
                "mov $8, %%rax\n"      // SYS_WAIT
                "mov %1, %%rdi\n"      // status pointer
                "int $0x80\n"
                "mov %%rax, %0"
                : "=r"(pid) : "r"(&status) : "rax", "rdi"
            );
            
            if (pid > 0) {
                terminal_writestring("[init] Reaped child\n");
                
                // If shell died, restart it
                if (pid == shell_pid) {
                    terminal_writestring("[init] Shell died! Restarting...\n");
                    // Fork again
                    asm volatile(
                        "mov $7, %%rax\n"      // SYS_FORK
                        "int $0x80\n"
                        "mov %%rax, %0"
                        : "=r"(shell_pid) : : "rax"
                    );
                    
                    if (shell_pid == 0) {
                        const char* path = "/bin/shell";
                        asm volatile(
                            "mov $9, %%rax\n"      // SYS_EXECVE
                            "mov %0, %%rdi\n"      // path
                            "mov $0, %%rsi\n"      // argv
                            "mov $0, %%rdx\n"      // envp
                            "int $0x80"
                            : : "r"(path) : "rax", "rdi", "rsi", "rdx"
                        );
                        
                        asm volatile(
                            "mov $1, %%rax\n"      // SYS_EXIT
                            "mov $1, %%rdi\n"      // exit code
                            "int $0x80"
                            : : : "rax", "rdi"
                        );
                    }
                }
            }
        }
    } else {
        terminal_writestring("[init] Failed to fork shell!\n");
        asm volatile(
            "mov $1, %%rax\n"      // SYS_EXIT
            "mov $1, %%rdi\n"      // exit code
            "int $0x80"
            : : : "rax", "rdi"
        );
    }
}

// File descriptor table (per-process)
#define MAX_FDS 16
typedef struct {
    fs_node_t* node;
    pipe_t* pipe;
    uint32_t offset;
    int flags;
    int is_pipe;
} fd_entry_t;

// Get current process's fd table
static fd_entry_t* get_fd_table(void) {
    process_t* current = process_get_current();
    if (!current || !current->fd_table) {
        return NULL;
    }
    return (fd_entry_t*)current->fd_table;
}

// Initialize fd table for a process
void init_process_fd_table(process_t* proc) {
    if (!proc) return;
    
    // Allocate fd table
    proc->fd_table = kmalloc(sizeof(fd_entry_t) * MAX_FDS);
    if (!proc->fd_table) return;
    
    fd_entry_t* fds = (fd_entry_t*)proc->fd_table;
    
    // Clear all entries
    for (int i = 0; i < MAX_FDS; i++) {
        fds[i].node = NULL;
        fds[i].pipe = NULL;
        fds[i].offset = 0;
        fds[i].flags = 0;
        fds[i].is_pipe = 0;
    }
}

// sys_open: Open a file
static uint64_t sys_open(uint64_t path_ptr, uint64_t flags, uint64_t mode, uint64_t arg4, uint64_t arg5) {
    (void)flags; (void)mode; (void)arg4; (void)arg5;
    
    const char* path = (const char*)path_ptr;
    if (!path) return -1;
    
    fd_entry_t* fd_table = get_fd_table();
    if (!fd_table) return -1;
    
    // Find free fd
    int fd = -1;
    for (int i = 3; i < MAX_FDS; i++) {  // Start from 3 (after stdin/stdout/stderr)
        if (!fd_table[i].node && !fd_table[i].pipe) {
            fd = i;
            break;
        }
    }
    
    if (fd == -1) return -1;  // No free fds
    
    // Simple path parsing - just look in root for now
    fs_node_t* root = fs_root();
    if (!root) return -1;
    
    // Skip leading slash
    if (path[0] == '/') path++;
    
    fs_node_t* node = fs_finddir(root, (char*)path);
    if (!node) {
        // Create file if it doesn't exist
        node = ramfs_create_file(root, path);
        if (!node) return -1;
    }
    
    fd_table[fd].node = node;
    fd_table[fd].offset = 0;
    fd_table[fd].flags = flags;
    fd_table[fd].is_pipe = 0;
    
    return fd;
}

// sys_close: Close a file descriptor
static uint64_t sys_close(uint64_t fd, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    fd_entry_t* fd_table = get_fd_table();
    if (!fd_table || fd >= MAX_FDS) {
        return -1;
    }
    
    fd_table[fd].node = NULL;
    fd_table[fd].pipe = NULL;
    fd_table[fd].offset = 0;
    fd_table[fd].flags = 0;
    fd_table[fd].is_pipe = 0;
    
    return 0;
}

// sys_stat: Get file information
static uint64_t sys_stat(uint64_t path_ptr, uint64_t stat_ptr, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;
    
    const char* path = (const char*)path_ptr;
    if (!path || !stat_ptr) return -1;
    
    // Simple stat structure
    struct stat {
        uint32_t size;
        uint32_t type;
    } *st = (struct stat*)stat_ptr;
    
    fs_node_t* root = fs_root();
    if (!root) return -1;
    
    // Skip leading slash
    if (path[0] == '/') path++;
    
    fs_node_t* node = fs_finddir(root, (char*)path);
    if (!node) return -1;
    
    st->size = node->size;
    st->type = node->type;
    
    return 0;
}

// sys_mkdir: Create directory
static uint64_t sys_mkdir(uint64_t path_ptr, uint64_t mode, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)mode; (void)arg3; (void)arg4; (void)arg5;
    
    const char* path = (const char*)path_ptr;
    if (!path) return -1;
    
    fs_node_t* root = fs_root();
    if (!root) return -1;
    
    // Skip leading slash
    if (path[0] == '/') path++;
    
    fs_node_t* dir = ramfs_create_dir(root, path);
    if (!dir) return -1;
    
    return 0;
}

// sys_readdir: Read directory entries
static uint64_t sys_readdir(uint64_t fd, uint64_t dirent_ptr, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;
    
    fd_entry_t* fd_table = get_fd_table();
    if (!fd_table || fd >= MAX_FDS || !fd_table[fd].node) {
        return -1;
    }
    
    fs_node_t* node = fd_table[fd].node;
    if (node->type != FS_TYPE_DIR) {
        return -1;
    }
    
    // Simple dirent structure
    struct dirent {
        char name[32];
        uint32_t type;
    } *de = (struct dirent*)dirent_ptr;
    
    fs_dirent_t* entry = fs_readdir(node, fd_table[fd].offset);
    if (!entry) return 0;  // No more entries
    
    strncpy(de->name, entry->name, 31);
    de->name[31] = '\0';
    de->type = FS_TYPE_FILE;  // Simplified
    
    fd_table[fd].offset++;
    return 1;  // One entry read
}

// sys_kill: Send signal to process
static uint64_t sys_kill(uint64_t pid, uint64_t sig, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;
    
    extern void signal_send(int pid, int sig);
    signal_send((int)pid, (int)sig);
    
    return 0;
}

// sys_pipe: Create a pipe
static uint64_t sys_pipe(uint64_t pipefd_ptr, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    int* pipefd = (int*)pipefd_ptr;
    if (!pipefd) return -1;
    
    fd_entry_t* fd_table = get_fd_table();
    if (!fd_table) return -1;
    
    // Create pipe
    pipe_t* pipe = pipe_create();
    if (!pipe) return -1;
    
    // Find two free fds
    int read_fd = -1, write_fd = -1;
    for (int i = 3; i < MAX_FDS; i++) {
        if (!fd_table[i].node && !fd_table[i].pipe) {
            if (read_fd == -1) {
                read_fd = i;
            } else {
                write_fd = i;
                break;
            }
        }
    }
    
    if (read_fd == -1 || write_fd == -1) {
        // No free fds
        pipe_destroy(pipe);
        return -1;
    }
    
    // Set up read end
    fd_table[read_fd].pipe = pipe;
    fd_table[read_fd].is_pipe = 1;
    fd_table[read_fd].flags = 0;  // Read only
    
    // Set up write end
    fd_table[write_fd].pipe = pipe;
    fd_table[write_fd].is_pipe = 1;
    fd_table[write_fd].flags = 1;  // Write only
    
    pipefd[0] = read_fd;
    pipefd[1] = write_fd;
    
    return 0;
}

// sys_dup2: Duplicate file descriptor
static uint64_t sys_dup2(uint64_t oldfd, uint64_t newfd, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;
    
    fd_entry_t* fd_table = get_fd_table();
    if (!fd_table) return -1;
    
    // Validate file descriptors
    if (oldfd >= MAX_FDS || newfd >= MAX_FDS) {
        return -1;
    }
    
    // Check if oldfd is valid
    if (!fd_table[oldfd].node && !fd_table[oldfd].pipe) {
        return -1;
    }
    
    // If oldfd == newfd, do nothing
    if (oldfd == newfd) {
        return newfd;
    }
    
    // Close newfd if it's open
    if (fd_table[newfd].node || fd_table[newfd].pipe) {
        if (fd_table[newfd].node) {
            fs_close(fd_table[newfd].node);
        }
        fd_table[newfd].node = NULL;
        fd_table[newfd].pipe = NULL;
    }
    
    // Copy oldfd to newfd
    fd_table[newfd] = fd_table[oldfd];
    
    // Increment reference count for pipes
    if (fd_table[newfd].pipe) {
        // TODO: Add reference counting to pipes
    }
    
    return newfd;
}

// System call handler (called from INT 0x80)
void syscall_handler(registers_t* regs) {
    // We're now in kernel mode with kernel stack from TSS
    
    // System call number in RAX
    uint64_t syscall_num = regs->rax;
    
    // Arguments in RDI, RSI, RDX, R10, R8 (Linux x86_64 ABI)
    // Note: RCX is clobbered by SYSCALL instruction, so R10 is used instead
    uint64_t arg1 = regs->rdi;
    uint64_t arg2 = regs->rsi;
    uint64_t arg3 = regs->rdx;
    uint64_t arg4 = regs->r10;
    uint64_t arg5 = regs->r8;
    
    // Validate syscall number
    if (syscall_num >= MAX_SYSCALLS || syscall_table[syscall_num] == NULL) {
        regs->rax = -1;  // ENOSYS
        return;
    }
    
    // Call the system call
    uint64_t result = syscall_table[syscall_num](arg1, arg2, arg3, arg4, arg5);
    
    // Return value in RAX
    regs->rax = result;
}

// Initialize system call interface
void init_syscalls(void) {
    // Clear syscall table
    for (int i = 0; i < MAX_SYSCALLS; i++) {
        syscall_table[i] = NULL;
    }
    
    // Register system calls
    syscall_table[SYS_EXIT] = sys_exit;
    syscall_table[SYS_WRITE] = sys_write;
    syscall_table[SYS_READ] = sys_read;
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_SLEEP] = sys_sleep;
    syscall_table[SYS_SBRK] = sys_sbrk;
    syscall_table[SYS_FORK] = sys_fork;
    syscall_table[SYS_WAIT] = sys_wait;
    syscall_table[SYS_EXECVE] = sys_execve;
    syscall_table[SYS_PS] = sys_ps;
    syscall_table[SYS_OPEN] = sys_open;
    syscall_table[SYS_CLOSE] = sys_close;
    syscall_table[SYS_STAT] = sys_stat;
    syscall_table[SYS_MKDIR] = sys_mkdir;
    syscall_table[SYS_READDIR] = sys_readdir;
    syscall_table[SYS_KILL] = sys_kill;
    syscall_table[SYS_PIPE] = sys_pipe;
    syscall_table[SYS_DUP2] = sys_dup2;
    
    // Register INT 0x80 handler
    register_interrupt_handler(0x80, syscall_handler);
    
    terminal_writestring("System call interface initialized (INT 0x80)\n");
}