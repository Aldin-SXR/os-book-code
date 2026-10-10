; spark -- switch.asm
;
; The context switch. It is in assembly because it is the one operation in the
; kernel that C has no way to express: it changes the stack pointer out from
; under the running function, so that the instruction after the call returns
; into a different function than the one that called it.
;
; No C compiler will emit that, and no C compiler could be told to. A function
; in C returns to its caller; this one returns to somebody else's caller.
;
; It relies on the ordinary calling convention rather than fighting it. cdecl
; says a function may destroy eax, ecx and edx, so the caller has already
; saved anything it cared about in those. The other four have to survive, so
; those four are what this pushes, and they are the whole of the "context"
; that a voluntary switch has to preserve. The interrupted-at-any-instruction
; case, where every register has to be saved, is Chapter 6's isr.asm and is
; how Chapter 13 will do it.

bits 32

section .text
global proc_switch

; void proc_switch(uint32_t *save_to, uint32_t load_from);
;
;   save_to    where to write the outgoing stack pointer
;   load_from  the incoming stack pointer
proc_switch:
    ; The four registers cdecl says a callee must preserve. Everything else
    ; that matters is already on this stack, put there by the C compiler.
    push ebp
    push ebx
    push esi
    push edi

    ; Four pushes moved esp by 16, so the return address is at esp+16 and the
    ; two arguments are above it.
    mov eax, [esp + 20]    ; save_to
    mov [eax], esp         ; where the outgoing one resumes
    mov esp, [esp + 24]    ; load_from: the other stack now

    ; The same four, belonging to whoever was last switched away from.
    pop edi
    pop esi
    pop ebx
    pop ebp

    ; And this is the strange one. The return address under those four
    ; belongs to the incoming process, so this ret does not go back to the
    ; caller: it goes to wherever that process last called proc_switch from,
    ; or, for a process that has never run, to its entry point.
    ret
