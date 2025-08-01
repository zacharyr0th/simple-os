# This Makefile automates the build process for SimpleOS.
# It defines compilation rules for C and assembly files, linking, and cleaning up built files.

# SimpleOS/Makefile

# Compiler and assembler
CC = x86_64-elf-gcc
AS = x86_64-elf-as
# Compiler flags
CFLAGS = -ffreestanding -c -m64 -O2 -Wall -Wextra -I./include -I./include/arch/x86_64 -I./include/kernel -I./include/mm -I./include/drivers -I./include/fs -I./include/ipc -I./include/lib -I./include/boot

# Linker flags
LDFLAGS = -nostdlib -T linker.ld -Wl,--no-warn-rwx-segments -Wl,--no-warn-execstack -Wl,--verbose

# Source files organized by subsystem
KERNEL_SRC = src/kernel/kernel.c src/kernel/scheduler.c src/kernel/process.c \
             src/kernel/syscall.c src/kernel/panic.c

MM_SRC = src/mm/kmalloc.c src/mm/pmm.c src/mm/vmm.c

DRIVER_SRC = src/drivers/terminal.c src/drivers/keyboard.c src/drivers/ports.c \
             src/drivers/timer.c src/drivers/vt.c

FS_SRC = src/fs/fs.c

IPC_SRC = src/ipc/pipe.c src/ipc/signal.c

LIB_SRC = src/lib/elf.c

BOOT_SRC = src/boot/exceptions.c

ARCH_SRC = src/arch/x86_64/tss.c src/arch/x86_64/usermode.c

PROG_SRC = src/programs/shell.c src/programs/shell_v2.c

# Assembly sources
ASM_SRC = src/arch/x86_64/asm_functions.s src/arch/x86_64/boot.s \
          src/arch/x86_64/context_switch.s

# All C sources
SRC = $(KERNEL_SRC) $(MM_SRC) $(DRIVER_SRC) $(FS_SRC) $(IPC_SRC) $(LIB_SRC) \
      $(BOOT_SRC) $(ARCH_SRC) $(PROG_SRC)

# Object files - put them in build directory
OBJ_DIR = build
OBJ = $(SRC:src/%.c=$(OBJ_DIR)/%.o) $(ASM_SRC:src/%.s=$(OBJ_DIR)/%.o)

# Output file
OUTPUT = kernel.bin
ISO = simpleos.iso

# Default target
all: $(ISO)

# Create build directories
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)
	mkdir -p $(OBJ_DIR)/kernel
	mkdir -p $(OBJ_DIR)/mm
	mkdir -p $(OBJ_DIR)/drivers
	mkdir -p $(OBJ_DIR)/fs
	mkdir -p $(OBJ_DIR)/ipc
	mkdir -p $(OBJ_DIR)/lib
	mkdir -p $(OBJ_DIR)/boot
	mkdir -p $(OBJ_DIR)/arch/x86_64
	mkdir -p $(OBJ_DIR)/programs

# Link object files to create the kernel binary
$(OUTPUT): $(OBJ_DIR) $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

# Compile C files
$(OBJ_DIR)/%.o: src/%.c
	$(CC) $(CFLAGS) $< -o $@

# Assemble assembly files
$(OBJ_DIR)/%.o: src/%.s
	$(AS) $< -o $@

# Create bootable ISO
$(ISO): $(OUTPUT)
	mkdir -p iso/boot/grub
	cp $(OUTPUT) iso/boot/
	cp boot/grub/grub.cfg iso/boot/grub/
	grub-mkrescue -o $(ISO) iso/
	rm -rf iso/

# Run in QEMU
run: $(ISO)
	qemu-system-x86_64 -cdrom $(ISO) -m 512M

# Clean up built files
clean:
	rm -rf $(OBJ_DIR)
	rm -f $(OUTPUT) $(ISO)
	rm -rf iso/

# Phony targets
.PHONY: all clean run