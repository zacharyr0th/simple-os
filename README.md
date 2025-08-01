# SimpleOS: A Unix-like Operating System for x86-64

SimpleOS is a modern Unix-like operating system kernel for x86-64 architecture, featuring a complete shell environment with pipes, signals, virtual terminals, and job control. Perfect for learning OS development or as a foundation for building custom operating systems.

## Key Features

### Core Unix Functionality
- **Pipes & Command Chaining**: `ps | grep shell` - Full pipe support with circular buffers
- **File System**: RAM-based filesystem with VFS layer, supporting files and directories
- **Signal Handling**: Ctrl+C interrupts, kill command, SIGINT/SIGKILL/SIGTERM support
- **Virtual Terminals**: 4 independent terminals (Alt+F1 through Alt+F4)
- **Job Control**: Background processes with `&`, job listing, and foreground/background switching
- **I/O Redirection**: Input/output redirection with `>`, `<`, and `>>` operators
- **Command History**: Navigate previous commands with UP/DOWN arrows
- **Tab Completion**: Smart command completion with TAB key

### System Architecture
- **64-bit Long Mode**: Full x86-64 support with 4-level paging
- **Preemptive Multitasking**: Timer-based scheduler with process priorities
- **System Call Interface**: Linux-style INT 0x80 with 18+ system calls
- **Per-Process Resources**: Isolated file descriptor tables and virtual memory
- **Memory Management**: Page frame allocator, heap allocator with kmalloc/kfree
- **User/Kernel Separation**: Ring 0/3 privilege levels with TSS

## Quick Demo

```bash
# Start the enhanced shell
$ /bin/shell_v2

# Pipe example - filter process list
$ ps | grep shell
2    1    RUN      shell_v2

# File operations
$ echo "Hello SimpleOS" > test.txt
$ cat test.txt
Hello SimpleOS
$ ls
hello.txt
readme.txt
test.txt

# Background jobs
$ stress &
[1] 25
$ jobs
[1]  Running    stress &
$ fg 1

# Virtual terminals
Press Alt+F2    # Switch to terminal 2
Press Alt+F1    # Back to terminal 1

# Tab completion
$ he<TAB>       # Completes to "help"
$ e<TAB>        # Shows: echo exit

# Signal handling
$ stress
Press Ctrl+C    # Interrupt the process
```

## Building SimpleOS

### Prerequisites

1. **Cross-compiler toolchain** (x86_64-elf-gcc):
   ```bash
   # macOS
   brew tap nativeos/i386-elf-toolchain
   brew install x86_64-elf-gcc
   
   # Linux
   # Build from source or use distribution packages
   ```

2. **GRUB and build tools**:
   ```bash
   # macOS
   brew install xorriso
   
   # Linux
   sudo apt-get install grub-pc-bin xorriso
   ```

3. **QEMU for testing**:
   ```bash
   # macOS
   brew install qemu
   
   # Linux
   sudo apt-get install qemu-system-x86
   ```

### Building and Running

```bash
# Build the kernel and create bootable ISO
make

# Run in QEMU
make run

# Clean build artifacts
make clean
```

## Project Structure

```
simple-os/
├── boot/
│   └── grub/               # GRUB bootloader configuration
│       └── grub.cfg
├── include/                # Header files
│   ├── elf.h              # ELF binary format structures
│   ├── exceptions.h       # Exception handlers
│   ├── fs.h               # Filesystem interfaces
│   ├── isr.h              # Interrupt service routines
│   ├── jobs.h             # Job control structures
│   ├── keyboard.h         # Keyboard driver interface
│   ├── kmalloc.h          # Kernel memory allocator
│   ├── panic.h            # Kernel panic handler
│   ├── pipe.h             # Pipe implementation
│   ├── pmm.h              # Physical memory manager
│   ├── ports.h            # I/O port operations
│   ├── process.h          # Process management structures
│   ├── scheduler.h        # Task scheduler interface
│   ├── signal.h           # Signal handling
│   ├── string.h           # String operations
│   ├── syscall.h          # System call definitions
│   ├── timer.h            # Timer/PIT driver
│   ├── tss.h              # Task state segment
│   ├── usermode.h         # User mode support
│   ├── vmm.h              # Virtual memory manager
│   └── vt.h               # Virtual terminal support
├── src/                    # Source code
│   ├── asm_functions.s    # Assembly helper functions
│   ├── boot.s             # Boot assembly code
│   ├── context_switch.s   # Context switching assembly
│   ├── elf.c              # ELF loader implementation
│   ├── exceptions.c       # Exception handlers
│   ├── fs.c               # Filesystem implementation
│   ├── jobs.c             # Job control (kernel side)
│   ├── kernel.c           # Main kernel entry point
│   ├── keyboard.c         # Keyboard driver
│   ├── kmalloc.c          # Memory allocator
│   ├── panic.c            # Panic handler
│   ├── pipe.c             # Pipe IPC mechanism
│   ├── pmm.c              # Physical memory manager
│   ├── ports.c            # Port I/O operations
│   ├── process.c          # Process management
│   ├── programs/          # Built-in programs
│   │   ├── grep.c         # grep utility
│   │   ├── init.c         # Init process
│   │   ├── shell.c        # Basic shell
│   │   ├── shell_v2.c     # Enhanced shell with Unix features
│   │   └── wc.c           # Word count utility
│   ├── scheduler.c        # Task scheduler
│   ├── signal.c           # Signal handling
│   ├── syscall.c          # System call implementations
│   ├── terminal.c         # VGA text terminal
│   ├── terminal.h         # Terminal header
│   ├── timer.c            # PIT timer driver
│   ├── tss.c              # TSS setup
│   ├── usermode.c         # User mode transitions
│   ├── vmm.c              # Virtual memory manager
│   └── vt.c               # Virtual terminals
├── userspace/             # Userspace programs
│   ├── Makefile           # Userspace build rules
│   ├── hello.c            # Sample userspace program
│   ├── hello_binary.h     # Binary header
│   └── link.ld            # Userspace linker script
├── scripts/               # Build and setup scripts
│   └── setup-toolchain.sh # Toolchain setup script
├── Makefile               # Main build configuration
├── Makefile.clang         # Clang build configuration
├── linker.ld              # Kernel linker script
├── LICENSE                # MIT License
└── README.md              # This file
```

## Architecture Details

### Memory Layout
```
0x0000000000000000 - 0x00007FFFFFFFFFFF  User Space (128 TB)
0xFFFF800000000000 - 0xFFFFFFFFFFFFFFFF  Kernel Space (128 TB)
```

### System Calls
- Process: `fork`, `exec`, `exit`, `wait`, `getpid`, `ps`
- I/O: `read`, `write`, `open`, `close`, `pipe`, `dup2`
- File System: `stat`, `mkdir`, `readdir`
- Memory: `sbrk`
- Other: `sleep`, `kill`

### Key Components

#### Process Management
- PCB with context, memory info, and file descriptors
- Round-robin scheduler with priorities
- Fork/exec model for process creation
- Zombie process handling

#### Virtual Memory
- 4-level page tables (PML4, PDPT, PD, PT)
- Per-process address spaces
- Copy-on-write planned for future

#### File System
- VFS layer with pluggable backends
- RAM-based filesystem with inodes
- Block allocation with 512-byte blocks
- Directory support with path resolution

#### Inter-Process Communication
- Pipes with circular buffers
- Signals with proper delivery
- Shared kernel structures for efficiency


## Testing

```bash
# Run specific tests
make test

# Debug with GDB
qemu-system-x86_64 -s -S -cdrom simpleos.iso
# In another terminal:
gdb
(gdb) target remote localhost:1234

# Enable serial output
make run QEMUFLAGS="-serial stdio"
```

## Learning Resources

- [OSDev Wiki](https://wiki.osdev.org/) - Comprehensive OS development resource
- [Intel Software Developer Manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html) - x86-64 architecture reference
- [xv6](https://github.com/mit-pdos/xv6-public) - Educational Unix-like OS from MIT

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
