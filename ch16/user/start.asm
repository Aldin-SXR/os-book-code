; spark -- user/start.asm
;
; The first instruction of every program on the disk.
;
; C has no way to say "this is where execution begins and the arguments are
; on the stack rather than pushed by a caller". The kernel builds a stack
; holding argc and argv and jumps here; this turns that into an ordinary C
; call and turns the return into a system call, because there is nowhere for
; main to return to.

bits 32
section .text.start

global _start
extern main
extern exit

_start:
    mov eax, [esp]            ; argc
    mov ecx, [esp + 4]        ; argv

    push ecx
    push eax
    call main
    add esp, 8

    push eax                  ; whatever main returned
    call exit

.stuck:
    jmp .stuck                ; exit does not return
