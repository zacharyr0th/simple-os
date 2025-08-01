#ifndef TSS_H
#define TSS_H

#include <stdint.h>

// Task State Segment structure for x86_64
typedef struct __attribute__((packed)) {
    uint32_t reserved0;
    uint64_t rsp0;      // Stack pointer for ring 0 (kernel)
    uint64_t rsp1;      // Stack pointer for ring 1 (unused)
    uint64_t rsp2;      // Stack pointer for ring 2 (unused)
    uint64_t reserved1;
    uint64_t ist[7];    // Interrupt stack table (optional)
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base; // I/O permission bitmap offset
} tss_t;

// TSS functions
void tss_init(void);
void tss_set_kernel_stack(uint64_t stack);
uint64_t tss_get_kernel_stack(void);

// Global TSS instance
extern tss_t tss;

#endif // TSS_H