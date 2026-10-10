; spark -- hello.asm
;
; The smallest boot sector that works: print one line and halt. It is not
; part of the kernel and nothing links against it.
;
; Every constraint here comes from the firmware. The sector is 512 bytes, it
; must end with the signature 0x55 0xAA, and it is loaded at 0x7C00.

bits 16
org 0x7C00

start:
    ; The firmware leaves the segment registers in an unspecified state.
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7C00               ; the stack grows downwards
    sti

    mov si, message
    mov ah, 0x0E                 ; BIOS teletype output
.next:
    lodsb                        ; al = [ds:si], si = si + 1
    test al, al
    jz .done
    int 0x10
    jmp .next

.done:
    hlt                          ; nothing left to do
    jmp .done

message: db "hello from sector zero", 13, 10, 0

times 510 - ($ - $$) db 0        ; pad to 510 bytes
dw 0xAA55                        ; what the firmware looks for
