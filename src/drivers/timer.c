#include <stdint.h>
#include "../include/isr.h"
#include "../include/ports.h"
#include "../include/terminal.h"
#include "../include/scheduler.h"

// PIT (Programmable Interval Timer) constants
#define PIT_CHANNEL0_DATA 0x40
#define PIT_CHANNEL1_DATA 0x41
#define PIT_CHANNEL2_DATA 0x42
#define PIT_COMMAND 0x43

#define PIT_FREQUENCY 1193180  // Base frequency of PIT

// Timer state
static uint64_t timer_ticks = 0;
static uint32_t timer_frequency = 0;

// Get system uptime in ticks
uint64_t timer_get_ticks(void) {
    return timer_ticks;
}

// Get uptime in milliseconds
uint64_t timer_get_ms(void) {
    return (timer_ticks * 1000) / timer_frequency;
}

// Sleep for specified milliseconds
void sleep_ms(uint32_t ms) {
    uint64_t start = timer_get_ms();
    while (timer_get_ms() - start < ms) {
        asm volatile("hlt");  // Save CPU while waiting
    }
}

// Timer interrupt handler
static void timer_callback(registers_t* regs) {
    (void)regs;  // Unused
    timer_ticks++;
    
    // Trigger scheduler tick
    scheduler_tick();
    
    // Send End of Interrupt to PIC
    outb(0x20, 0x20);
}

// Initialize the timer
void init_timer(uint32_t frequency) {
    timer_frequency = frequency;
    
    // Calculate PIT divisor
    uint32_t divisor = PIT_FREQUENCY / frequency;
    
    // Send the command byte (channel 0, lobyte/hibyte, rate generator)
    outb(PIT_COMMAND, 0x36);
    
    // Send the frequency divisor
    outb(PIT_CHANNEL0_DATA, divisor & 0xFF);         // Low byte
    outb(PIT_CHANNEL0_DATA, (divisor >> 8) & 0xFF);  // High byte
    
    // Register timer callback for IRQ0
    register_interrupt_handler(IRQ0, timer_callback);
    
    terminal_writestring("Timer initialized at ");
    // TODO: Print frequency
    terminal_writestring(" Hz\n");
}