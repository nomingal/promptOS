# promptOS - Custom UEFI Bootloader & Kernel

QEMU = qemu-system-x86_64

# Arch Linux standard paths for OVMF firmware code & vars
OVMF_CODE ?= /usr/share/edk2/x64/OVMF_CODE.4m.fd
OVMF_VARS ?= /usr/share/edk2/x64/OVMF_VARS.4m.fd

# ==========================================
# 1. KERNEL BUILD SETTINGS
# ==========================================
CFLAGS = -g -O2 -pipe -Wall -Wextra -std=gnu11 -ffreestanding \
         -fno-stack-protector -fno-stack-check -fno-lto -fno-PIC \
         -m64 -march=x86-64 -mno-80387 -mno-mmx -mno-sse -mno-sse2 \
         -mno-red-zone -mcmodel=kernel -fno-tree-loop-distribute-patterns -MMD -MP

LDFLAGS = -m elf_x86_64 -nostdlib -static -z max-page-size=0x1000 \
          -T kernel/linker.ld

# Find all C files EXCEPT efi_main.c (which belongs to the bootloader)
KSRC = $(filter-out kernel/efi_main.c, $(shell find kernel -name '*.c'))
KOBJ = $(KSRC:kernel/%.c=build/%.o)

# ==========================================
# 2. UEFI BOOTLOADER SETTINGS (gnu-efi)
# ==========================================
EFI_INC = /usr/include/efi
EFI_LIB = /usr/lib

EFI_CFLAGS = -fpic -ffreestanding -fno-stack-protector -fno-stack-check \
             -fshort-wchar -mno-red-zone -maccumulate-outgoing-args \
             -I$(EFI_INC) -I$(EFI_INC)/x86_64 -I$(EFI_INC)/protocol

EFI_LDFLAGS = -shared -Bsymbolic -L$(EFI_LIB) -T $(EFI_LIB)/elf_x86_64_efi.lds -entry _start
EFI_LIBS = -lefi -lgnuefi
EFI_CRT = $(EFI_LIB)/crt0-efi-x86_64.o

.PHONY: all run clean

all: build/kernel.elf build/BOOTX64.EFI

# ==========================================
# 3. KERNEL COMPILATION
# ==========================================
build/kernel.elf: $(KOBJ) kernel/linker.ld
	$(LD) $(KOBJ) $(LDFLAGS) -o $@

build/%.o: kernel/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

-include $(KOBJ:.o=.d)

# ==========================================
# 4. UEFI BOOTLOADER COMPILATION
# ==========================================
build/efi_main.o: kernel/efi_main.c
	@mkdir -p $(dir $@)
	$(CC) $(EFI_CFLAGS) -c $< -o $@

# Link as an ELF shared object first using the gnu-efi linker script
build/bootloader.so: build/efi_main.o
	$(LD) $(EFI_LDFLAGS) $(EFI_CRT) $< -o $@ $(EFI_LIBS)

# Convert the ELF shared object into a valid PE32+ UEFI executable (.rodata added!)
build/BOOTX64.EFI: build/bootloader.so
	objcopy -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym -j .rel \
            -j .rela -j .rel.* -j .rela.* -j .reloc \
            -O efi-app-x86_64 $< $@

# ==========================================
# 5. RUNNING IN QEMU
# ==========================================
run: all
	@echo "Setting up virtual FAT drive..."
	@rm -rf build/fat_root
	@mkdir -p build/fat_root/EFI/BOOT
	@cp build/BOOTX64.EFI build/fat_root/EFI/BOOT/
	@cp build/kernel.elf build/fat_root/
	@cp $(OVMF_VARS) build/OVMF_VARS.fd
	$(QEMU) -M q35 -m 256M \
	    -drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
	    -drive if=pflash,format=raw,file=build/OVMF_VARS.fd \
	    -drive file=fat:rw:build/fat_root,format=raw,media=disk

clean:
	rm -rf build

# ==========================================
# 6. UEFI DISK IMAGE GENERATION (.img)
# ==========================================
img: all
	@echo "A criar a imagem de disco UEFI (.img)..."
	@rm -f build/promptos.img
	# 1. Criar uma imagem vazia de 64MB
	@dd if=/dev/zero of=build/promptos.img bs=1M count=64 status=none
	
	# 2. Configurar tabela de partições GPT e partição FAT32 (ESP)
	@parted -s build/promptos.img mklabel gpt
	@parted -s build/promptos.img mkpart ESP fat32 1MiB 100%
	@parted -s build/promptos.img set 1 esp on
	
	# 3. Associar a um loop device, formatar e copiar os ficheiros de boot
	@LOOP_DEV=$$(sudo losetup --find --show --partscan build/promptos.img); \
	sudo mkfs.fat -F32 $${LOOP_DEV}p1; \
	sudo mkdir -p build/img_mount; \
	sudo mount $${LOOP_DEV}p1 build/img_mount; \
	sudo mkdir -p build/img_mount/EFI/BOOT; \
	sudo cp build/BOOTX64.EFI build/img_mount/EFI/BOOT/BOOTX64.EFI; \
	sudo cp build/kernel.elf build/img_mount/; \
	sync; \
	sudo umount build/img_mount; \
	sudo losetup -d $${LOOP_DEV}; \
	rm -rf build/img_mount
	@echo "Imagem de disco criada com sucesso em build/promptos.img!"
