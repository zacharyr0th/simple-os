// This header file defines functions for input and output operations on I/O ports.
// It provides low-level access to hardware ports, allowing reading from and writing to specific port addresses.

#ifndef PORTS_H
#define PORTS_H

#include <stdint.h>

uint8_t inb(uint16_t port);
void outb(uint16_t port, uint8_t data);

#endif 