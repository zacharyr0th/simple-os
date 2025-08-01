#include "../include/elf.h"
#include "../include/process.h"
#include "../include/vmm.h"
#include "../include/pmm.h"
#include "../include/string.h"
#include "../include/terminal.h"

// Validate ELF header
static int elf_validate(Elf64_Ehdr* header) {
    // Check magic
    if (header->e_ident[0] != ELFMAG0 ||
        header->e_ident[1] != ELFMAG1 ||
        header->e_ident[2] != ELFMAG2 ||
        header->e_ident[3] != ELFMAG3) {
        terminal_writestring("ELF: Invalid magic\n");
        return -1;
    }
    
    // Check 64-bit
    if (header->e_ident[4] != ELFCLASS64) {
        terminal_writestring("ELF: Not 64-bit\n");
        return -1;
    }
    
    // Check executable
    if (header->e_type != ET_EXEC) {
        terminal_writestring("ELF: Not executable\n");
        return -1;
    }
    
    // Check x86-64
    if (header->e_machine != EM_X86_64) {
        terminal_writestring("ELF: Wrong architecture\n");
        return -1;
    }
    
    return 0;
}

// Load ELF segments into process address space
int elf_load(process_t* process, void* elf_data, size_t size) {
    (void)size; // TODO: Use size for bounds checking
    
    Elf64_Ehdr* header = (Elf64_Ehdr*)elf_data;
    
    // Validate
    if (elf_validate(header) < 0) {
        return -1;
    }
    
    terminal_writestring("ELF: Loading executable, entry=0x");
    // TODO: Print hex value
    terminal_writestring("\n");
    
    // Get program headers
    Elf64_Phdr* phdrs = (Elf64_Phdr*)((uint8_t*)elf_data + header->e_phoff);
    
    // Process each segment
    for (int i = 0; i < header->e_phnum; i++) {
        Elf64_Phdr* phdr = &phdrs[i];
        
        if (phdr->p_type != PT_LOAD) {
            continue;
        }
        
        terminal_writestring("ELF: Loading segment ");
        // TODO: Print segment number and addresses
        terminal_writestring("\n");
        
        // Calculate required pages
        uint64_t start = phdr->p_vaddr & ~0xFFF;
        uint64_t end = (phdr->p_vaddr + phdr->p_memsz + 0xFFF) & ~0xFFF;
        uint64_t pages = (end - start) / PAGE_SIZE;
        
        // Map pages
        for (uint64_t j = 0; j < pages; j++) {
            uint64_t vaddr = start + j * PAGE_SIZE;
            void* phys_page = pmm_alloc_page();
            
            if (!phys_page) {
                terminal_writestring("ELF: Out of memory\n");
                return -1;
            }
            
            uint64_t paddr = (uint64_t)phys_page;
            
            // Set permissions
            uint64_t flags = PAGE_PRESENT | PAGE_USER;
            if (phdr->p_flags & PF_W) {
                flags |= PAGE_WRITABLE;
            }
            
            if (vmm_map_page(process->page_table, vaddr, paddr, flags) < 0) {
                terminal_writestring("ELF: Failed to map page\n");
                pmm_free_page(phys_page);
                return -1;
            }
            
            process->pages_allocated++;
        }
        
        // Copy data
        if (phdr->p_filesz > 0) {
            uint8_t* src = (uint8_t*)elf_data + phdr->p_offset;
            
            // Copy page by page (since physical pages might not be contiguous)
            for (uint64_t off = 0; off < phdr->p_filesz; ) {
                uint64_t vaddr = phdr->p_vaddr + off;
                uint64_t paddr = vmm_get_physical(process->page_table, vaddr);
                if (paddr == 0) {
                    terminal_writestring("ELF: Failed to get physical address\n");
                    return -1;
                }
                
                uint64_t page_off = vaddr & 0xFFF;
                uint64_t copy_size = PAGE_SIZE - page_off;
                
                if (off + copy_size > phdr->p_filesz) {
                    copy_size = phdr->p_filesz - off;
                }
                
                memcpy((void*)(paddr + page_off), src + off, copy_size);
                off += copy_size;
            }
        }
        
        // Zero BSS section (memsz > filesz)
        if (phdr->p_memsz > phdr->p_filesz) {
            uint64_t bss_start = phdr->p_vaddr + phdr->p_filesz;
            uint64_t bss_size = phdr->p_memsz - phdr->p_filesz;
            
            // Zero it out
            for (uint64_t off = 0; off < bss_size; ) {
                uint64_t vaddr = bss_start + off;
                uint64_t paddr = vmm_get_physical(process->page_table, vaddr);
                if (paddr == 0) {
                    terminal_writestring("ELF: Failed to get physical address for BSS\n");
                    return -1;
                }
                
                uint64_t page_off = vaddr & 0xFFF;
                uint64_t zero_size = PAGE_SIZE - page_off;
                
                if (off + zero_size > bss_size) {
                    zero_size = bss_size - off;
                }
                
                memset((void*)(paddr + page_off), 0, zero_size);
                off += zero_size;
            }
        }
    }
    
    // Update process to have proper user mode context
    // Set entry point for user mode execution
    process->context.rip = header->e_entry;
    process->context.rsp = USER_STACK_TOP - 16; // Start near top of user stack
    process->context.rflags = 0x202; // Interrupts enabled
    
    // Set up user mode segments - we'll need to update this
    // For now, just mark that this is a user process
    process->entry_point = (void (*)(void))header->e_entry;
    
    return 0;
}

// Create process from ELF
process_t* elf_create_process(void* elf_data, size_t size, const char* name) {
    // Create process with a dummy entry point first
    process_t* process = process_create(name, (void (*)(void))0x100000, 1);
    if (!process) {
        return NULL;
    }
    
    // Load ELF
    if (elf_load(process, elf_data, size) < 0) {
        process_destroy(process);
        return NULL;
    }
    
    terminal_writestring("ELF: Process '");
    terminal_writestring(name);
    terminal_writestring("' created\n");
    
    return process;
}