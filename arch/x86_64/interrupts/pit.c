#include <stdint.h>

#include <x86_64/pic/pic.h>
#include <x86_64/io/io.h>
#include "pit.h"

uint64_t ticks;

void timer_handler()
{
	ticks++;

	pic_eoi(0);
}

void pit_init(uint32_t frequency)
{
	uint16_t divisor = PIT_BASE_FREQUENCY / frequency;

	outb(0x43, 0x36);

	outb(0x40, divisor & 0xFF);

	outb(0x40, divisor >> 8);
}
