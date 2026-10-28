; AwizzOS boot: Multiboot2 -> Long Mode -> kernel_main
; Requests framebuffer (preferred 1024x768x32)

section .multiboot2
align 8
mb2_start:
    dd 0xE85250D6                                    ; magic Multiboot2
    dd 0                                             ; i386
    dd mb2_end - mb2_start
    dd 0x100000000 - (0xE85250D6 + 0 + (mb2_end - mb2_start))

    ; Framebuffer request tag (type 5), optional
    align 8
    dw 5                                             ; type = framebuffer
    dw 1                                             ; flags = optional
    dd 20                                            ; size
    dd 1024                                          ; width
    dd 768                                           ; height
    dd 32                                            ; depth (bpp)

    ; End tag
    align 8
    dw 0
    dw 0
    dd 8
mb2_end:

section .bss
align 4096
p4:  resb 4096
p3:  resb 4096
; Four P2 tables to identity-map 4 GB with 2 MiB pages
p2_0: resb 4096
p2_1: resb 4096
p2_2: resb 4096
p2_3: resb 4096
stack_bottom: resb 16384
stack_top:

section .text
bits 32
global _start
extern kernel_main

_start:
    mov esp, stack_top
    mov edi, eax                 ; Multiboot2 magic
    ; ebx = Multiboot2 info pointer

    ; Check long mode support
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode

    ; Identity map first 4 GiB (2 MiB pages)
    ; P4[0] -> P3
    mov eax, p3
    or eax, 0b11
    mov [p4], eax

    ; P3[0..3] -> p2_0 .. p2_3
    mov eax, p2_0
    or eax, 0b11
    mov [p3 + 0*8], eax
    mov eax, p2_1
    or eax, 0b11
    mov [p3 + 1*8], eax
    mov eax, p2_2
    or eax, 0b11
    mov [p3 + 2*8], eax
    mov eax, p2_3
    or eax, 0b11
    mov [p3 + 3*8], eax

    ; Fill each P2 with 512 huge pages
    ; Base physical for each: 0, 1GiB, 2GiB, 3GiB
    mov ecx, 0
.map_p2_0:
    mov eax, ecx
    shl eax, 21                  ; * 2MiB
    or eax, 0b10000011           ; present | writable | huge
    mov [p2_0 + ecx*8], eax
    inc ecx
    cmp ecx, 512
    jne .map_p2_0

    mov ecx, 0
.map_p2_1:
    mov eax, ecx
    shl eax, 21
    add eax, 0x40000000          ; +1 GiB
    or eax, 0b10000011
    mov [p2_1 + ecx*8], eax
    inc ecx
    cmp ecx, 512
    jne .map_p2_1

    mov ecx, 0
.map_p2_2:
    mov eax, ecx
    shl eax, 21
    add eax, 0x80000000          ; +2 GiB
    or eax, 0b10000011
    mov [p2_2 + ecx*8], eax
    inc ecx
    cmp ecx, 512
    jne .map_p2_2

    mov ecx, 0
.map_p2_3:
    mov eax, ecx
    shl eax, 21
    add eax, 0xC0000000          ; +3 GiB
    or eax, 0b10000011
    mov [p2_3 + ecx*8], eax
    inc ecx
    cmp ecx, 512
    jne .map_p2_3

    ; Enable PAE, long mode, paging
    mov eax, p4
    mov cr3, eax
    mov eax, cr4
    or eax, 1 << 5               ; PAE
    mov cr4, eax
    mov ecx, 0xC0000080          ; EFER
    rdmsr
    or eax, 1 << 8               ; LME
    wrmsr
    mov eax, cr0
    or eax, 1 << 31              ; PG
    mov cr0, eax

    lgdt [gdt64.pointer]
    jmp 0x08:long_mode

.no_long_mode:
    mov dword [0xB8000], 0x4F524F45
    hlt

bits 64
long_mode:
    xor ax, ax
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov edi, edi                 ; zero-extend magic
    mov esi, ebx                 ; Multiboot info
    call kernel_main
.hang:
    cli
    hlt
    jmp .hang

section .rodata
gdt64:
    dq 0
    dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53)  ; 64-bit code
.pointer:
    dw .pointer - gdt64 - 1
    dq gdt64
