# promptOS - build with `make`, boot in QEMU with `make run`
#
# Host needs: gcc (x86_64), ld, make, git, xorriso, qemu-system-x86_64.
# On a non-x86_64 host, use a cross toolchain:
#   make CC=x86_64-elf-gcc LD=x86_64-elf-ld

LIMINE_BRANCH ?= v9.x-binary
QEMU          ?= qemu-system-x86_64

CFLAGS  := -g -O2 -pipe -Wall -Wextra -std=gnu11 -ffreestanding \
           -fno-stack-protector -fno-stack-check -fno-lto -fno-PIC \
           -m64 -march=x86-64 -mno-80387 -mno-mmx -mno-sse -mno-sse2 \
           -mno-red-zone -mcmodel=kernel -fno-tree-loop-distribute-patterns -MMD -MP
LDFLAGS := -m elf_x86_64 -nostdlib -static -z max-page-size=0x1000 \
           -T kernel/linker.ld

SRC := $(shell find kernel -name '*.c')
OBJ := $(SRC:kernel/%.c=build/%.o)

.PHONY: all iso run run-uefi debug clean distclean

all: build/kernel

build/kernel: $(OBJ) kernel/linker.ld
	$(LD) $(OBJ) $(LDFLAGS) -o $@

build/%.o: kernel/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

-include $(OBJ:.o=.d)

# Fetch the prebuilt Limine bootloader (a clone of its -binary branch).
limine:
	git clone https://github.com/limine-bootloader/limine.git --branch=$(LIMINE_BRANCH) --depth=1
	$(MAKE) -C limine

iso: build/promptos.iso

build/promptos.iso: build/kernel limine.conf | limine
	rm -rf build/iso_root
	mkdir -p build/iso_root/boot/limine build/iso_root/EFI/BOOT
	cp build/kernel build/iso_root/boot/kernel
	cp limine.conf limine/limine-bios.sys limine/limine-bios-cd.bin \
	   limine/limine-uefi-cd.bin build/iso_root/boot/limine/
	cp limine/BOOTX64.EFI build/iso_root/EFI/BOOT/
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
	   -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
	   -apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
	   -efi-boot-part --efi-boot-image --protective-msdos-label \
	   build/iso_root -o $@
	./limine/limine bios-install $@

# Boots in a QEMU window (BIOS). The shutdown command closes it.
run: build/promptos.iso
	$(QEMU) -M q35 -m 256M -cdrom $<

# Same, but through UEFI firmware (what most real PCs and laptops use).
# Needs OVMF; override the path if yours differs, e.g. on Fedora:
#   make run-uefi OVMF=/usr/share/edk2/ovmf/OVMF_CODE.fd
OVMF ?= /usr/share/ovmf/OVMF.fd
run-uefi: build/promptos.iso
	$(QEMU) -M q35 -m 256M -cdrom $< -bios $(OVMF)

# Wait for gdb on :1234 (then: gdb build/kernel -ex 'target remote :1234')
debug: build/promptos.iso
	$(QEMU) -M q35 -m 256M -cdrom $< -no-reboot -s -S

clean:
	rm -rf build

distclean: clean
	rm -rf limine
