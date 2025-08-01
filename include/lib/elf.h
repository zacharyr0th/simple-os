#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include <stddef.h>

// ELF Magic Numbers
#define ELFMAG0 0x7F
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

// ELF Class
#define ELFCLASS64 2

// ELF Data encoding
#define ELFDATA2LSB 1  // Little endian

// ELF Types
#define ET_EXEC 2      // Executable file

// Machine types
#define EM_X86_64 62   // AMD x86-64

// Segment types
#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3

// Segment permissions
#define PF_X 0x1       // Execute
#define PF_W 0x2       // Write
#define PF_R 0x4       // Read

// ELF64 Header
typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) Elf64_Ehdr;

// Program Header
typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) Elf64_Phdr;

// Forward declare process_t to avoid circular includes
struct process;
typedef struct process process_t;

// Function prototypes
int elf_load(process_t* process, void* elf_data, size_t size);
process_t* elf_create_process(void* elf_data, size_t size, const char* name);

#endif // ELF_H