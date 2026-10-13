#include <stdint.h>
#include <debug/debug.h>
#include <x86_64/idt/idt.h>

#include <info/system.h>

#include "faults.h"

void breakpoint_handler(uint64_t *stack)
{
	(void)stack;
	//idt_dump();
	uint64_t cs = stack[8];
	uint8_t cpl = cs & 0x3;

	if (cpl == 0) {
		idt_complete_self_test();
	}

	debug_write(SYSTEM_NAME, "IDT - subsequent to #BP", "Breakpoint at CPL %u", cpl);
}
