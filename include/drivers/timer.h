#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

// Initialize timer with given frequency (in Hz)
void init_timer(uint32_t frequency);

// Get system uptime in timer ticks
uint64_t timer_get_ticks(void);

// Get system uptime in milliseconds
uint64_t timer_get_ms(void);

// Sleep for specified milliseconds
void sleep_ms(uint32_t ms);

#endif // TIMER_H