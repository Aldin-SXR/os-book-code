; spark -- bootsect.asm
;
; The first 512 bytes of the disk: the only code the firmware loads. The
; strings and the signature at the end have to fit inside that budget too.
;
; The file is 16-bit for most of its length and 32-bit after the switch to
; protected mode, which is one way. It is assembly throughout because there is
; no usable stack at entry, no current C compiler targets 16-bit real mode,
; and several of the instructions here have no expression in C.

bits 16
org 0x7C00                       ; the firmware loads us here, always

KERNEL_SEG    equ 0x1000         ; 0x1000:0000, which is linear 0x10000
KERNEL_TARGET equ 0x100000       ; where the kernel is linked to run
BOOT_MAGIC    equ 0xB007AC05     ; what we leave in eax

%ifndef KERNEL_SECTORS           ; the Makefile measures the kernel and
%define KERNEL_SECTORS 64        ; passes the real number in
%endif

start:
    ; The firmware leaves the segment registers in an unspecified state.
    ; Zero them so that an address means itself, and put the stack just below
    ; the load address, where nothing else is.
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl         ; the firmware leaves the boot disk in dl

    mov si, msg_loading
    call print

    ; Read the kernel from the disk through the BIOS: the last firmware
    ; service spark uses.
    mov si, packet
    mov ah, 0x42                 ; extended read, by LBA
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    mov si, msg_switching
    call print

    cli                          ; from here there is no interrupt table
    in al, 0x92                  ; the A20 gate, the fast way
    or al, 2
    out 0x92, al

    lgdt [gdt_descriptor]        ; hand over the table
    mov eax, cr0
    or eax, 1                    ; CR0.PE: protected mode on
    mov cr0, eax
    jmp CODE_SEG:protected       ; the jump that reloads cs

disk_error:
    mov si, msg_disk
    call print
.hang:
    hlt
    jmp .hang

; Print the NUL-terminated string at ds:si through the BIOS, one character at
; a time. Teletype output advances the cursor and scrolls by itself.
print:
    mov ah, 0x0E
.next:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .next
.done:
    ret

; ---------------------------------------------------------------- 32-bit ---
bits 32
protected:
    mov ax, DATA_SEG             ; the old segment values mean nothing now
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    ; Move the kernel from the address real mode could reach to the address
    ; it is linked for. This is why protected mode has to be entered first:
    ; 0x100000 is not addressable in real mode.
    mov esi, KERNEL_SEG << 4
    mov edi, KERNEL_TARGET
    mov ecx, (KERNEL_SECTORS * 512) / 4
    rep movsd

    mov eax, BOOT_MAGIC          ; who started the kernel
    xor ebx, ebx                 ; no information structure
    jmp KERNEL_TARGET

; ------------------------------------------------------------------ data ---
; The smallest descriptor table that protected mode will accept: the null
; entry the processor requires, one code segment and one data segment, both
; flat over the whole four gigabytes. The kernel replaces it with a table of
; its own once it is running.
align 8
gdt_start:
    dq 0x0000000000000000        ; null descriptor
gdt_code:
    dw 0xFFFF, 0x0000            ; limit 0:15, base 0:15
    db 0x00, 10011010b, 11001111b, 0x00
gdt_data:
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 11001111b, 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1   ; one less than the size
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

; The disk address packet for the extended read. Sector 0 is this file, so
; the kernel begins at sector 1.
align 4
packet:
    db 0x10                      ; size of this packet
    db 0
    dw KERNEL_SECTORS
    dw 0                         ; destination offset
    dw KERNEL_SEG                ; destination segment
    dq 1                         ; first sector to read

boot_drive:   db 0
msg_loading:   db "spark: loading the kernel", 13, 10, 0
msg_switching: db "spark: entering protected mode", 13, 10, 0
msg_disk:      db "spark: could not read the disk", 13, 10, 0

times 510 - ($ - $$) db 0        ; pad out to 510 bytes
dw 0xAA55                        ; and the two bytes the firmware looks for
