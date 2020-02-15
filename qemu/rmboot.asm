section .text
org 0x7c00
use16

start:
mov ax, cs
mov ds, ax
mov es, ax
mov ss, ax
mov sp, 0x8000
enable_a20: ; from osdev.org
        cli
        call    a20wait
        mov     al,0xad
        out     0x64,al
        call    a20wait
        mov     al,0xd0
        out     0x64,al
        call    a20wait2
        in      al,0x60
        push    eax
        call    a20wait
        mov     al,0xd1
        out     0x64,al
        call    a20wait
        pop     eax
        or      al,2
        out     0x60,al
        call    a20wait
        mov     al,0xae
        out     0x64,al
        call    a20wait
        jmp setup_32
a20wait:
        in      al,0x64
        test    al,2
        jnz     a20wait
        ret
a20wait2:
        in      al,0x64
        test    al,1
        jz      a20wait2
        ret
setup_32:
lgdt [gdtr]
mov eax, cr0
or al, 1
mov cr0, eax
jmp 8:entry32

use32

entry32:
mov ax, 16
mov ds, ax
mov es, ax
mov ss, ax
mov esp, 0x107c00

mov esi, 0x7c00
mov edi, 0x107c00
mov ecx, 512
rep movsb
db 0xe9
dd 0x100000

mov edi, 0x1000
mov esi, 0x108000
mov ecx, 0xff000
rep movsb

push dword 0x1000
ret

gdt:
db 0, 0, 0, 0, 0, 0, 0, 0
db 0xff, 0xff, 0, 0, 0, 0x9a, 0xcf, 0
db 0xff, 0xff, 0, 0, 0, 0x92, 0xcf, 0

gdtr:
dw 23
dd gdt

end:
times 510-end+start db 0
db 0x55, 0xaa
