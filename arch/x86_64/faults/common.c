#include <stdint.h>
#include <debug/debug.h>
#include <init/platform.h>

#include <info/system.h>

#include "faults.h"

#define ANSI_FAULT "\x1b[95m"
#define ANSI_RESET "\x1b[0m"

static void fault_halt(void)
{
	for (;;) {
		__asm__ volatile("cli; hlt");
	}
}

void exception_handler(uint64_t *stack)
{
	(void)stack;
	/*
	uint64_t rip = stack[X86_ISR_NOERR_RIP];
	uint64_t cs = stack[X86_ISR_NOERR_CS];
	*/
	debug_write(SYSTEM_NAME, "IDT",
		    "uma interrupção aconteceu, se vira pra descobrir ai kkkkkkkkkkk");
	//fault_halt();
}

void gpf_handler(uint64_t *stack)
{
	(void)stack;
	/*
	uint64_t error = stack[X86_ISR_ERROR];
	uint64_t rip = stack[X86_ISR_RIP];
	uint64_t cs = stack[X86_ISR_CS];
	*/

	uint64_t error = stack[X86_ISR_ERROR];
	uint64_t rip = stack[X86_ISR_RIP];
	uint64_t cs = stack[X86_ISR_CS];
	uint64_t cpl = cs & 0x3;

	debug_write(SYSTEM_NAME, "IDT - subsequent to #GP", "Fatal protection violation");

	debug_write(SYSTEM_NAME, "Fault Analyser", "Error : 0x%02x", error);
	debug_write(SYSTEM_NAME, "Fault Analyser", "RIP : 0x%02x | CPL : 0x%x", rip, cpl);

	panic("Unrecoverable exception");

	fault_halt();
}

void df_handler(uint64_t *stack)
{
	(void)stack;

	fault_halt();
}
