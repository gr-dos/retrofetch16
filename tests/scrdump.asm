; scrdump.com - a test fixture, not part of retrofch
;
; Writes the 80x25 color text screen (B800:0000, 4000 bytes: character,
; attribute, ...) to SCREEN.BIN in the current directory, so that
; tests/run.sh can check what retrofch's BIOS output put on the screen,
; colors included.  Exit status 1 if the file cannot be written.
;
; Built with Open Watcom wasm and wlink (format dos com).

_TEXT   segment byte public 'CODE'
        assume  cs:_TEXT, ds:_TEXT
        org     100h

start:  mov     ah, 3ch                 ; create
        xor     cx, cx
        mov     dx, offset fname
        int     21h
        jc      fail
        mov     bx, ax
        push    ds
        mov     ax, 0b800h
        mov     ds, ax
        xor     dx, dx
        mov     cx, 4000
        mov     ah, 40h                 ; write
        int     21h
        pop     ds
        jc      fail
        cmp     ax, 4000
        jne     fail
        mov     ah, 3eh                 ; close
        int     21h
        mov     ax, 4c00h
        int     21h
fail:   mov     ax, 4c01h
        int     21h

fname   db      'SCREEN.BIN', 0

_TEXT   ends
        end     start
