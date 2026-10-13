#include <stdint.h>
#include "idt.h"
#include <x86_64/isr/isr.h>
#include <debug/debug.h>
#include <info/system.h>
#include <x86_64/interrupts/pit.h>
#include <x86_64/pic/pic.h>

static idt_entry_t idt[IDT_ENTRIES];
static idtr_t idtr;

static int self_test_result = 0;

extern void irq0(void);

static inline void lidt(idtr_t *idtr_ptr)
{
	__asm__ volatile("lidt (%0)" : : "r"(idtr_ptr));
}

static void sti(void)
{
	__asm__ volatile("sti");
}

static void cli(void)
{
	__asm__ volatile("cli");
}

void idt_set_gate(uint8_t vector, void *isr, uint8_t flags)
{
	uint64_t addr = (uint64_t)isr;

	idt[vector].offset_low = addr & 0xFFFF;
	idt[vector].selector = 0x08;
	idt[vector].ist = 0;
	idt[vector].type_attr = flags;
	idt[vector].offset_mid = (addr >> 16) & 0xFFFF;
	idt[vector].offset_high = (addr >> 32) & 0xFFFFFFFF;
	idt[vector].zero = 0;
}

int idt_has_gate(uint8_t vector)
{
	return idt[vector].offset_low != 0;
}

static void idt_register_exceptions(void)
{
	for (int i = 0; i < 32; i++) {
		idt_set_gate(i, isr_common, 0xEE);
	}

	idt_set_gate(0x08, isr_df, 0xEE);
	idt_set_gate(0x0D, isr_gpf, 0xEE);
	idt_set_gate(0x0E, isr_pf, 0xEE);
	idt_set_gate(0x03, isr_bp, 0xEE);
}

static void idt_load(void)
{
	idtr.limit = sizeof(idt) - 1;
	idtr.base = (uint64_t)&idt;

	lidt(&idtr);
}

void idt_dump(void)
{
	debug_write(SYSTEM_NAME, "IDT", "Base: 0x%x, Limit: 0x%x", (unsigned long long)idtr.base,
		    idtr.limit);

	for (uint16_t vector = 0; vector < IDT_ENTRIES; vector++) {
		if (!idt_has_gate((uint8_t)vector))
			continue;

		uint64_t offset = ((uint64_t)idt[vector].offset_high << 32) |
			((uint64_t)idt[vector].offset_mid << 16) | idt[vector].offset_low;

		debug_write(SYSTEM_NAME, "IDT",
			    "Vector %u: Handler 0x%x, Selector 0x%x, Flags 0x%x", vector,
			    (unsigned long long)offset, idt[vector].selector,
			    idt[vector].type_attr);
	}
}

int idt_check_self_test(void)
{
	int i = self_test_result;
	self_test_result = 0;
	return i;
}

void idt_complete_self_test(void)
{
	self_test_result = 1;
}

void idt_initialize_devices(void)
{
	pic_remap(0x20, 0x28);
	pic_unmask(0);
	idt_set_gate(0x20, irq0, 0xEE);
	pit_init(1000);
}

void idt_init(void)
{
	cli();

	idt_register_exceptions();

	idt_load();

	idt_initialize_devices();

	sti();
}
