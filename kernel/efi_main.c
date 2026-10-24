/* kernel/efi_main.c */
#include <efi.h>
#include <efilib.h>
#include "boot_info.h"

#define PT_LOAD 1

typedef struct {
    unsigned char e_ident[16];
    UINT16 e_type;
    UINT16 e_machine;
    UINT32 e_version;
    UINT64 e_entry;
    UINT64 e_phoff;
    UINT64 e_shoff;
    UINT32 e_flags;
    UINT16 e_ehsize;
    UINT16 e_phentsize;
    UINT16 e_phnum;
    UINT16 e_shentsize;
    UINT16 e_shnum;
    UINT16 e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    UINT32 p_type;
    UINT32 p_flags;
    UINT64 p_offset;
    UINT64 p_vaddr;
    UINT64 p_paddr;
    UINT64 p_filesz;
    UINT64 p_memsz;
    UINT64 p_align;
} Elf64_Phdr;

static EFI_SYSTEM_TABLE *gST_Global = NULL;

static char uefi_getc(void) {
    if (!gST_Global || !gST_Global->ConIn) return 0;

    EFI_INPUT_KEY key;
    EFI_STATUS status = uefi_call_wrapper(gST_Global->ConIn->ReadKeyStroke, 2, gST_Global->ConIn, &key);
    if (status == EFI_SUCCESS) {
        if (key.UnicodeChar == L'\r') return '\n';
        if (key.UnicodeChar == L'\b') return '\b';
        if (key.UnicodeChar >= 32 && key.UnicodeChar < 127) {
            return (char)key.UnicodeChar;
        }
    }
    return 0;
}

static void freeze_on_error(const CHAR16 *msg) {
    Print(L"\n[Bootloader Error] %s\nSystem Halted.\n", msg);
    while (1) {
        __asm__ volatile("hlt");
    }
}

static void map_page(UINT64 *pml4, UINT64 virt_addr, UINT64 phys_addr) {
    UINT16 pml4_idx = (virt_addr >> 39) & 0x1FF;
    UINT16 pdpt_idx = (virt_addr >> 30) & 0x1FF;
    UINT16 pd_idx   = (virt_addr >> 21) & 0x1FF;
    UINT16 pt_idx   = (virt_addr >> 12) & 0x1FF;

    UINT64 *pdpt;
    if (!(pml4[pml4_idx] & 1)) {
        EFI_PHYSICAL_ADDRESS phys;
        uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiRuntimeServicesData, 1, &phys);
        pdpt = (UINT64 *)phys;
        for (int i = 0; i < 512; i++) pdpt[i] = 0;
        pml4[pml4_idx] = phys | 0x3;
    } else {
        pdpt = (UINT64 *)(pml4[pml4_idx] & ~0xFFFU);
    }

    UINT64 *pd;
    if (!(pdpt[pdpt_idx] & 1)) {
        EFI_PHYSICAL_ADDRESS phys;
        uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiRuntimeServicesData, 1, &phys);
        pd = (UINT64 *)phys;
        for (int i = 0; i < 512; i++) pd[i] = 0;
        pdpt[pdpt_idx] = phys | 0x3;
    } else {
        pd = (UINT64 *)(pdpt[pdpt_idx] & ~0xFFFU);
    }

    UINT64 *pt;
    if (!(pd[pd_idx] & 1)) {
        EFI_PHYSICAL_ADDRESS phys;
        uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiRuntimeServicesData, 1, &phys);
        pt = (UINT64 *)phys;
        for (int i = 0; i < 512; i++) pt[i] = 0;
        pd[pd_idx] = phys | 0x3;
    } else {
        pt = (UINT64 *)(pd[pd_idx] & ~0xFFFU);
    }

    pt[pt_idx] = (phys_addr & ~0xFFFU) | 0x3;
}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    gST_Global = SystemTable;

    uefi_call_wrapper(SystemTable->ConOut->ClearScreen, 1, SystemTable->ConOut);
    Print(L"=== promptOS Bootloader ===\n\n");

    /* 1. Setup GOP */
    EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    EFI_STATUS status = uefi_call_wrapper(BS->LocateProtocol, 3, &gopGuid, NULL, (void **)&gop);
    if (EFI_ERROR(status)) {
        freeze_on_error(L"Failed to locate GOP (Graphics Protocol)");
    }

    static struct promptos_framebuffer fb;
    fb.address          = (uint8_t *)gop->Mode->FrameBufferBase;
    fb.width            = gop->Mode->Info->HorizontalResolution;
    fb.height           = gop->Mode->Info->VerticalResolution;
    fb.pitch            = gop->Mode->Info->PixelsPerScanLine * 4;
    fb.bpp              = 32;
    fb.memory_model     = PROMPTOS_FRAMEBUFFER_RGB;
    fb.red_mask_size    = 8; fb.red_mask_shift   = 16;
    fb.green_mask_size  = 8; fb.green_mask_shift = 8;
    fb.blue_mask_size   = 8; fb.blue_mask_shift  = 0;

    static struct boot_info b_info;
    b_info.framebuffer = &fb;
    b_info.getc_fn = uefi_getc;

    Print(L"[OK] Framebuffer initialized (%dx%d).\n", fb.width, fb.height);

    /* 2. Open Root Drive Volume */
    EFI_GUID sfspGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *sfsp;
    status = uefi_call_wrapper(BS->LocateProtocol, 3, &sfspGuid, NULL, (void **)&sfsp);
    if (EFI_ERROR(status)) {
        freeze_on_error(L"Failed to locate Simple File System");
    }

    EFI_FILE_PROTOCOL *root;
    status = uefi_call_wrapper(sfsp->OpenVolume, 2, sfsp, &root);
    if (EFI_ERROR(status)) {
        freeze_on_error(L"Failed to open root drive volume");
    }

    /* 3. Read kernel.elf */
    EFI_FILE_PROTOCOL *kernel_file;
    status = uefi_call_wrapper(root->Open, 5, root, &kernel_file, L"kernel.elf", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        freeze_on_error(L"Could not find 'kernel.elf' on boot drive root");
    }

    Elf64_Ehdr ehdr;
    UINTN header_size = sizeof(Elf64_Ehdr);
    status = uefi_call_wrapper(kernel_file->Read, 3, kernel_file, &header_size, &ehdr);
    if (EFI_ERROR(status) || ehdr.e_ident[0] != 0x7F || ehdr.e_ident[1] != 'E') {
        freeze_on_error(L"Invalid kernel ELF header");
    }

    Elf64_Phdr phdrs[16];
    UINTN phdrs_size = ehdr.e_phnum * sizeof(Elf64_Phdr);
    uefi_call_wrapper(kernel_file->SetPosition, 2, kernel_file, ehdr.e_phoff);
    uefi_call_wrapper(kernel_file->Read, 3, kernel_file, &phdrs_size, phdrs);

    /* 4. Copy UEFI Page Tables */
    EFI_PHYSICAL_ADDRESS pml4_phys;
    status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiRuntimeServicesData, 1, &pml4_phys);
    if (EFI_ERROR(status)) {
        freeze_on_error(L"Failed to allocate PML4 table");
    }
    UINT64 *new_pml4 = (UINT64 *)pml4_phys;

    UINT64 current_cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(current_cr3));
    UINT64 *old_pml4 = (UINT64 *)(current_cr3 & ~0xFFFU);

    for (int i = 0; i < 512; i++) {
        new_pml4[i] = old_pml4[i];
    }
    new_pml4[511] = 0;

    /* 5. Map Kernel Segments */
    for (UINT16 i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            UINT64 pages = (phdrs[i].p_memsz + 0xFFF) / 0x1000;
            EFI_PHYSICAL_ADDRESS segment_mem;
            status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, pages, &segment_mem);
            if (EFI_ERROR(status)) {
                freeze_on_error(L"Failed to allocate memory for kernel segment");
            }

            if (phdrs[i].p_filesz > 0) {
                uefi_call_wrapper(kernel_file->SetPosition, 2, kernel_file, phdrs[i].p_offset);
                UINTN sz = phdrs[i].p_filesz;
                uefi_call_wrapper(kernel_file->Read, 3, kernel_file, &sz, (void *)segment_mem);
            }

            if (phdrs[i].p_memsz > phdrs[i].p_filesz) {
                UINT8 *bss = (UINT8 *)(segment_mem + phdrs[i].p_filesz);
                for (UINT64 b = 0; b < (phdrs[i].p_memsz - phdrs[i].p_filesz); b++) {
                    bss[b] = 0;
                }
            }

            for (UINT64 p = 0; p < pages; p++) {
                map_page(new_pml4, phdrs[i].p_vaddr + p * 4096, segment_mem + p * 4096);
            }
        }
    }

    Print(L"[OK] Starting promptOS kernel...\n");

    /* 6. Switch CR3 and Jump to Kernel Entry */
    __asm__ volatile("mov %0, %%cr3" : : "r"(pml4_phys));

    void (*kernel_entry)(struct boot_info *) = (void (*)(struct boot_info *))ehdr.e_entry;
    kernel_entry(&b_info);

    while (1) {
        __asm__ volatile("hlt");
    }

    return EFI_SUCCESS;
}