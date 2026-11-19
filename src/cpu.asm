; cpu.asm - CPU, FPU and XMS probes for retrofch (Open Watcom wasm)
;
; Small model, __cdecl: arguments on the stack, result in AX (DX:AX for
; 32 bits), caller pops.  Every routine saves BX, SI, DI, BP and ES.
;
; The file is assembled for the 8086.  Instructions of later CPUs are
; enabled with .286p/.386/.586 only around the code that runs after the
; probe has shown the CPU has them, and the caller (probe.c) calls the
; 386 and CPUID routines only when cpu_probe says they are safe.

        .8086
        .8087

_TEXT   segment byte public 'CODE'
        assume  cs:_TEXT

        public  _cpu_probe, _cpu_queue, _fpu_probe
        public  _cpu_cpuid, _cpu_rdtsc, _xms_probe

; ------------------------------------------------------------------
; unsigned cpu_probe(void)
;
; Low byte, the class:
;   0  8086/8088 (or NEC V20/V30): FLAGS bits 12-15 stuck at 1 and
;      shift counts not masked
;   1  80186/80188: FLAGS bits 12-15 stuck at 1, shift counts masked
;   2  80286: FLAGS bits 12-14 stuck at 0 in real mode
;   3  80386: bits 12-14 settable, EFLAGS.AC (bit 18) not
;   4  80486 without CPUID: AC settable, EFLAGS.ID (bit 21) not
;   5  CPUID available: ID settable
; Flags:
;   100h  NEC V-series: AAD ignores its immediate operand (class 0)
;   200h  MSW.PE set: virtual 8086 mode (class 3 and up)

_cpu_probe proc near
        push    bp
        mov     bp, sp
        push    bx
        push    si
        push    di
        pushf

        ; Clear FLAGS bits 12-15; the 8086 and 80186 read them back as 1.
        pushf
        pop     ax
        and     ax, 0fffh
        push    ax
        popf
        pushf
        pop     ax
        and     ax, 0f000h
        cmp     ax, 0f000h
        jne     p_286up

        ; 8086 or 80186: the 80186 masks shift counts to 5 bits.
        mov     ax, 0ffffh
        mov     cl, 33
        shl     ax, cl
        jnz     p_186

        ; 8086 or NEC V20/V30: the NEC parts always use base 10 in AAD.
        ; AAD 16 on 0102h gives 12h on Intel, 0Ch on NEC.
        mov     ax, 0102h
        db      0d5h, 10h
        cmp     al, 0ch
        mov     ax, 0
        jne     p_done
        mov     ax, 100h
        jmp     p_done

p_186:  mov     ax, 1
        jmp     p_done

        ; 286 or later.  SMSW is a 286 instruction, and unprivileged.
p_286up:
        .286p
        smsw    ax
        .8086
        test    al, 1
        jz      p_real
        ; Protected mode under DOS is virtual 8086 mode: a 386 or later.
        call    probe486
        or      ax, 200h
        jmp     p_done

        ; Real mode: the 80286 cannot set FLAGS bits 12-14 (IOPL, NT).
p_real: pushf
        pop     ax
        or      ax, 7000h
        push    ax
        popf
        pushf
        pop     ax
        test    ax, 7000h
        mov     ax, 2
        jz      p_done
        call    probe486

p_done: popf
        pop     di
        pop     si
        pop     bx
        pop     bp
        ret
_cpu_probe endp

; probe486: AX = 3, 4 or 5 on a CPU known to be a 386 or later.
; With interrupts off and SP dword aligned, toggles EFLAGS.AC and then
; EFLAGS.ID, restoring EFLAGS each time.  (An alignment-check fault
; needs AC set, CR0.AM set and CPL 3, as in virtual 8086 mode; with SP
; aligned and interrupts off nothing unaligned runs while AC is set.)

        .386
probe486 proc near
        pushf
        cli
        mov     dx, sp
        and     sp, 0fffch
        pushfd
        pop     eax
        mov     ecx, eax
        xor     eax, 40000h
        push    eax
        popfd
        pushfd
        pop     eax
        push    ecx
        popfd
        xor     eax, ecx
        test    eax, 40000h
        mov     ax, 3
        jz      p4_done
        pushfd
        pop     eax
        mov     ecx, eax
        xor     eax, 200000h
        push    eax
        popfd
        pushfd
        pop     eax
        push    ecx
        popfd
        xor     eax, ecx
        test    eax, 200000h
        mov     ax, 4
        jz      p4_done
        mov     ax, 5
p4_done:
        mov     sp, dx
        popf
        ret
probe486 endp
        .8086

; ------------------------------------------------------------------
; unsigned cpu_queue(void)
;
; The prefetch queue length of an 8086-class or 80186-class CPU, by
; self-modifying code: 6 (8086, V30, 80186), 4 (8088, V20, 80188), or
; 0 if even the very next instruction's change is seen, which a real
; 8086-family CPU never does (an emulator without a prefetch queue).
; Only for classes 0 and 1: later CPUs differ (the 386 and later detect
; or flush on such writes).

_cpu_queue proc near
        push    bp
        mov     bp, sp
        push    bx
        push    si
        push    di
        push    es
        pushf

        push    cs
        pop     es

        ; Test 1: overwrite the INC DX right after REP STOSB with a NOP.
        xor     dx, dx
        mov     di, offset q1_inc
        mov     al, 90h
        mov     cx, 1
        cld
        cli
        rep     stosb
q1_inc: inc     dx
        sti
        mov     byte ptr cs:[q1_inc], 42h       ; INC DX again
        or      dx, dx
        mov     ax, 0
        jz      q_done

        ; Test 2: write STI backwards over the three bytes ending at q2_end;
        ; the INC DX is the fifth byte after REP STOSB.  A 4-byte queue has
        ; not fetched it yet and runs the STI; a 6-byte queue runs INC DX.
        xor     dx, dx
        mov     di, offset q2_end
        mov     al, 0fbh
        mov     cx, 3
        std
        cli
        rep     stosb
        cld
        nop
        nop
        nop
q2_inc: inc     dx
q2_nop: nop
q2_end: sti
        mov     byte ptr cs:[q2_inc], 42h       ; INC DX
        mov     byte ptr cs:[q2_nop], 90h       ; NOP
        or      dx, dx
        mov     ax, 4
        jz      q_done
        mov     ax, 6

q_done: popf
        pop     es
        pop     di
        pop     si
        pop     bx
        pop     bp
        ret
_cpu_queue endp

; ------------------------------------------------------------------
; unsigned fpu_probe(void)
;
; 0 no FPU; 1 an FPU with projective infinity (8087, 80287);
; 2 an FPU with affine infinity only (80387 and later, integrated).
; FNINIT/FNSTSW/FNSTCW have no WAIT, so they are harmless without an
; FPU; a delay loop after each gives an 8087 time to store.

_fpu_probe proc near
        push    bp
        mov     bp, sp
        sub     sp, 2
        push    bx
        push    si
        push    di

        fninit
        mov     word ptr [bp-2], 5a5ah
        fnstsw  word ptr [bp-2]
        mov     cx, 64
f_w1:   loop    f_w1
        cmp     byte ptr [bp-2], 0
        jne     f_none
        fnstcw  word ptr [bp-2]
        mov     cx, 64
f_w2:   loop    f_w2
        mov     ax, [bp-2]
        and     ax, 103fh
        cmp     ax, 3fh
        jne     f_none

        ; Present.  After FNINIT the 8087 and 80287 compare -inf equal to
        ; +inf (projective); the 80387 and later do not.
        fld1
        fldz
        fdivp   st(1), st               ; +inf
        fld     st                      ; +inf, +inf
        fchs                            ; -inf, +inf
        fcompp
        fstsw   word ptr [bp-2]
        fwait
        mov     ax, [bp-2]
        sahf
        mov     ax, 1
        jz      f_init
        mov     ax, 2
f_init: fninit
        jmp     f_done

f_none: xor     ax, ax
f_done: pop     di
        pop     si
        pop     bx
        mov     sp, bp
        pop     bp
        ret
_fpu_probe endp

; ------------------------------------------------------------------
; void cpu_cpuid(unsigned long leaf, unsigned long *regs)
;
; regs[0..3] = EAX, EBX, ECX, EDX of CPUID leaf (subleaf 0).
; Only when cpu_probe returned class 5.

        .586
_cpu_cpuid proc near
        push    bp
        mov     bp, sp
        push    bx
        push    si
        push    di
        mov     eax, [bp+4]
        xor     ecx, ecx
        cpuid
        mov     si, [bp+8]
        mov     [si], eax
        mov     [si+4], ebx
        mov     [si+8], ecx
        mov     [si+12], edx
        pop     di
        pop     si
        pop     bx
        pop     bp
        ret
_cpu_cpuid endp

; void cpu_rdtsc(unsigned long *tsc): tsc[0] low, tsc[1] high.
; Only when CPUID leaf 1 reports a time-stamp counter.

_cpu_rdtsc proc near
        push    bp
        mov     bp, sp
        push    bx
        rdtsc
        mov     bx, [bp+4]
        mov     [bx], eax
        mov     [bx+4], edx
        pop     bx
        pop     bp
        ret
_cpu_rdtsc endp
        .8086

; ------------------------------------------------------------------
; int xms_probe(unsigned long *out, int big)
;
; 0 if no XMS driver (INT 2Fh AX=4300h).  Otherwise 1, with
;   out[0]  version (XMS function 00h AX, BCD, e.g. 0300h)
;   out[1]  largest free block, KB
;   out[2]  total free, KB
; using function 08h, or 88h (32-bit counts, XMS 3.0 on a 386) when big
; is nonzero and the version is 3.00 or later.  A driver that reports
; an error (BL nonzero, such as A0h: all memory allocated) gives 0 KB.

_xms_probe proc near
        push    bp
        mov     bp, sp
        sub     sp, 4                   ; [bp-4] far entry point
        push    bx
        push    si
        push    di
        push    es

        mov     ax, 4300h
        int     2fh
        cmp     al, 80h
        je      x_have
        xor     ax, ax
        jmp     x_done
x_have: mov     ax, 4310h
        int     2fh
        mov     [bp-4], bx
        mov     [bp-2], es

        mov     si, [bp+4]
        mov     ah, 0
        call    dword ptr [bp-4]
        mov     [si], ax
        mov     word ptr [si+2], 0

        cmp     word ptr [bp+6], 0
        je      x_small
        cmp     ax, 0300h
        jb      x_small
        .386
        mov     ah, 88h
        xor     bl, bl
        call    dword ptr [bp-4]
        or      bl, bl
        jz      x_big
        xor     eax, eax
        xor     edx, edx
x_big:  mov     si, [bp+4]
        mov     [si+4], eax
        mov     [si+8], edx
        jmp     x_ok
        .8086

x_small:
        mov     ah, 8
        xor     bl, bl
        call    dword ptr [bp-4]
        or      bl, bl
        jz      x_sm2
        xor     ax, ax
        xor     dx, dx
x_sm2:  mov     si, [bp+4]
        mov     [si+4], ax
        mov     word ptr [si+6], 0
        mov     [si+8], dx
        mov     word ptr [si+10], 0

x_ok:   mov     ax, 1
x_done: pop     es
        pop     di
        pop     si
        pop     bx
        mov     sp, bp
        pop     bp
        ret
_xms_probe endp

_TEXT   ends
        end
