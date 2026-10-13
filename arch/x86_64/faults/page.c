#include <stdint.h>
#include <stddef.h>

#include "debug/debug.h"
#include <info/system.h>
#include <init/platform.h>
#include "faults.h"

uint64_t read_cr2(void)
{
	uint64_t value;

	__asm__ volatile("mov %%cr2, %0" : "=r"(value));

	return value;
}

uint64_t read_cr3(void)
{
	uint64_t value;

	__asm__ volatile("mov %%cr3, %0" : "=r"(value));

	return value;
}

void backtrace(void)
{
	uint64_t *rbp;

	__asm__ volatile("mov %%rbp, %0" : "=r"(rbp));

	debug_write(SYSTEM_NAME, "Backtrace", "Call stack:");

	for (int i = 0; i < 16 && rbp; i++) {
		uint64_t rip = rbp[1];

		debug_write(SYSTEM_NAME, "Backtrace", "#%d: 0x%016lx", i, rip);

		rbp = (uint64_t *)rbp[0];
	}
}

void pf_handler(uint64_t *stack)
{
	(void)stack;

	uint64_t error = stack[X86_ISR_ERROR];
	uint64_t rip = stack[X86_ISR_RIP];
	uint64_t cs = stack[X86_ISR_CS];
	uint64_t cr2 = read_cr2();
	uint64_t cr3 = read_cr3();
	uint64_t cpl = cs & 0x3;

	debug_write(SYSTEM_NAME, "IDT - subsequent to #PF", "Fatal memory access violation");

	debug_write(SYSTEM_NAME, "Fault Analyser", "Error : 0x%02x | CR2 : 0x%02x | CR3 : 0x%05x",
		    error, cr2, cr3);
	debug_write(SYSTEM_NAME, "Fault Analyser", "RIP : 0x%02x | CPL : 0x%x", rip, cpl);

	char *cause;

	if (cr2 == 0) {
		cause = "possible NULL dereference";
	} else if (cr2 < 0x1000) {
		cause = "uncertain low memory access";
	} else {
		cause = "uncertain";
	}

	debug_write(SYSTEM_NAME, "Fault Analyser", "Cause : %s", cause);

	backtrace();

	debug_write(SYSTEM_NAME, "Fault Analyser", "Page : %s",
		    (error & (1 << 0)) ? "present" : "not present");

	debug_write(SYSTEM_NAME, "Fault Analyser", "Access : %s",
		    (error & (1 << 1)) ? "write" : "read");

	debug_write(SYSTEM_NAME, "Fault Analyser", "Mode : %s",
		    (error & (1 << 2)) ? "userspace" : "kernel");

	if (error & (1 << 3)) {
		debug_write(SYSTEM_NAME, "Fault Analyser",
			    "Reason: reserved page-table bit violation");
	}

	if (error & (1 << 4)) {
		debug_write(SYSTEM_NAME, "Fault Analyser", "Access : instruction fetch");
	}

	if (error & (1 << 5)) {
		debug_write(SYSTEM_NAME, "Fault Analyser", "Reason : protection-key violation");
	}

	if (error & (1 << 6)) {
		debug_write(SYSTEM_NAME, "Fault Analyser", "Reason : shadow-stack access");
	}

	if (error & (1 << 7)) {
		debug_write(SYSTEM_NAME, "Fault Analyser", "Reason : HLAT-related violation");
	}

	if (error & (1 << 15)) {
		debug_write(SYSTEM_NAME, "Fault Analyser", "Reason : SGX violation");
	}

	panic("Unrecoverable exception");

	for (;;) {
		__asm__ volatile("cli; hlt");
	}
}
