#include <stdint.h>
#include <x86_64/io/io.h>
#include "pic.h"

void pic_remap(uint8_t offset1, uint8_t offset2)
{
	uint8_t mask1 = inb(PIC1_DATA);
	uint8_t mask2 = inb(PIC2_DATA);

	// Begin initialization
	outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);

	outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

	// Vector offsets
	outb(PIC1_DATA, offset1);

	outb(PIC2_DATA, offset2);

	// Tell Master about Slave at IRQ2
	outb(PIC1_DATA, 1 << 2);

	// Tell Slave its cascade identity
	outb(PIC2_DATA, 2);

	// 8086 mode
	outb(PIC1_DATA, ICW4_8086);

	outb(PIC2_DATA, ICW4_8086);

	// Restore masks
	outb(PIC1_DATA, mask1);
	outb(PIC2_DATA, mask2);
}

void pic_unmask(uint8_t irq)
{
	uint16_t port;
	uint8_t mask;

	if (irq < 8) {
		port = PIC1_DATA;
	} else {
		irq -= 8;
		port = PIC2_DATA;
	}

	mask = inb(port);
	mask &= ~(1 << irq);
	outb(port, mask);
}

void pic_eoi(uint8_t irq)
{
	if (irq >= 8)
		outb(PIC2_COMMAND, PIC_EOI);

	outb(PIC1_COMMAND, PIC_EOI);
}
