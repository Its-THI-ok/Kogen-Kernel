CC      := clang
LD      := ld.lld
NASM    := nasm

BUILD    := build
ISO_ROOT := $(BUILD)/iso
TARGET   := $(BUILD)/kernel.elf
ISO      := $(BUILD)/sine.iso

LIMINE := limine

SRC_DIRS := kernel libk arch boot drivers

CFLAGS := \
	-std=c23 \
	-ffreestanding \
	-fno-stack-protector \
	-fno-pic \
	-fno-pie \
	-m64 \
	-mno-red-zone \
	-Wall \
	-g \
	-Wextra \
	-Iinclude \
	-Ikernel \
	-Idrivers \
	-Ilibk \
	-Iarch \
	-mcmodel=kernel \
	-mgeneral-regs-only \
	-MMD -MP

LDFLAGS := \
	-T linker.ld \
	-nostdlib \
	-z max-page-size=0x1000

C_SOURCES := $(shell find $(SRC_DIRS) -type f -name '*.c' 2>/dev/null)
ASM_SOURCES := $(shell find $(SRC_DIRS) -type f -name '*.asm' 2>/dev/null)

C_OBJECTS := $(patsubst %.c,$(BUILD)/%.o,$(C_SOURCES))
ASM_OBJECTS := $(patsubst %.asm,$(BUILD)/%.o,$(ASM_SOURCES))

OBJECTS := $(C_OBJECTS) $(ASM_OBJECTS)
DEPS := $(C_OBJECTS:.o=.d)

.PHONY: all iso run debug clean reinstall-limine

################################################################################
# Default
################################################################################

all: $(TARGET)

################################################################################
# C
################################################################################

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@printf "  [CC] %s\n" "$<"
	@$(CC) $(CFLAGS) -c $< -o $@

################################################################################
# ASM
################################################################################

$(BUILD)/%.o: %.asm
	@mkdir -p $(dir $@)
	@printf "  [AS] %s\n" "$<"
	@$(NASM) -f elf64 $< -o $@

################################################################################
# Link
################################################################################

$(TARGET): $(OBJECTS) linker.ld
	@mkdir -p $(BUILD)
	@printf "  [LD] kernel.elf\n"
	@$(LD) $(LDFLAGS) $^ -o $@

################################################################################
# ISO
################################################################################

iso: $(ISO)

$(ISO): $(TARGET) limine.conf
	@printf "  [ISO] Preparing\n"

	@rm -rf $(ISO_ROOT)

	@mkdir -p $(ISO_ROOT)/boot
	@mkdir -p $(ISO_ROOT)/boot/limine
	@mkdir -p $(ISO_ROOT)/EFI/BOOT

	@cp $(TARGET) $(ISO_ROOT)/boot/kernel.elf
	@cp limine.conf $(ISO_ROOT)/boot/

	@cp $(LIMINE)/limine-bios.sys $(ISO_ROOT)/boot/limine/
	@cp $(LIMINE)/limine-bios-cd.bin $(ISO_ROOT)/boot/limine/
	@cp $(LIMINE)/limine-uefi-cd.bin $(ISO_ROOT)/boot/limine/

	@cp $(LIMINE)/BOOTX64.EFI $(ISO_ROOT)/EFI/BOOT/
	@cp $(LIMINE)/BOOTIA32.EFI $(ISO_ROOT)/EFI/BOOT/

	@printf "  [ISO] Building\n"

	@xorriso \
		-as mkisofs \
		-quiet \
		-b boot/limine/limine-bios-cd.bin \
		-no-emul-boot \
		-boot-load-size 4 \
		-boot-info-table \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part \
		--efi-boot-image \
		--protective-msdos-label \
		$(ISO_ROOT) \
		-o $(ISO)

	@printf "  [LIMINE] Installing\n"
	@$(LIMINE)/limine bios-install $(ISO)

	@printf "  [DONE] %s\n" "$(ISO)"

################################################################################
# Run
################################################################################

run: $(ISO)
	@qemu-system-x86_64 \
		-cdrom $(ISO) \
		-serial stdio

################################################################################
# Serial-only
################################################################################

ser: $(ISO)
	@qemu-system-x86_64 \
		-cdrom $(ISO) \
		-display none \
		-monitor none \
		-serial stdio

################################################################################
# GDB
################################################################################

gdb: $(ISO)
	@qemu-system-x86_64 \
		-cdrom $(ISO) \
		-display none \
		-monitor none \
		-serial stdio \
		-s -S

################################################################################
# Debug
################################################################################

debug: $(ISO)
	@qemu-system-x86_64 \
		-cdrom $(ISO) \
		-serial stdio \
		-d int

################################################################################
# Reinstall Limine
################################################################################

reinstall-limine:
	@printf "  [LIMINE] Reinstalling\n"
	@$(LIMINE)/limine bios-install $(ISO)
	@printf "  [DONE] Limine reinstalled\n"

################################################################################
# Clean
################################################################################

clean:
	@rm -rf $(BUILD)

-include $(DEPS)
