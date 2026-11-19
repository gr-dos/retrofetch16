; grfake.com - a test fixture, not part of retrofch
;
; A TSR that answers the GR-DOS identification call (INT 21h AX=3347h,
; BX=4752h; gr-dos docs/grver.md) as GR-DOS 0.04, API revision 1, so
; that tests/run.sh can check retrofch's GR-DOS detection and logo under
; DOSBox-X.
;
;   grfake      answer correctly: DX = NOT DX (the nonce complemented)
;   grfake b    answer every call the same way (DX = 1234h), which a
;               caller using two nonces must reject
;
; Built with Open Watcom wasm and wlink (format dos com).

_TEXT   segment byte public 'CODE'
        assume  cs:_TEXT, ds:_TEXT
        org     100h

start:  jmp     install

old21   dd      ?
bad     db      0

handler:
        cmp     ax, 3347h
        jne     chain
        cmp     bx, 4752h
        jne     chain
        mov     ax, 4752h
        mov     bx, 0004h               ; 0.04
        mov     cx, 0001h               ; revision 1
        cmp     byte ptr cs:[bad], 0
        jne     h_bad
        not     dx
        iret
h_bad:  mov     dx, 1234h
        iret
chain:  jmp     dword ptr cs:[old21]

install:
        ; "b" anywhere in the command tail selects the bad answer
        mov     si, 81h
        mov     cl, ds:[80h]
        xor     ch, ch
        jcxz    i_hook
i_scan: lodsb
        or      al, 20h
        cmp     al, 'b'
        jne     i_next
        mov     byte ptr [bad], 1
i_next: loop    i_scan

i_hook: mov     ax, 3521h
        int     21h
        mov     word ptr [old21], bx
        mov     word ptr [old21+2], es
        mov     dx, offset handler
        mov     ax, 2521h
        int     21h
        ; keep the PSP and everything up to install
        mov     dx, offset install
        add     dx, 15
        mov     cl, 4
        shr     dx, cl
        mov     ax, 3100h
        int     21h

_TEXT   ends
        end     start
