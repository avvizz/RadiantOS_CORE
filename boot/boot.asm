; RadiantOS: Multiboot2 -> Long Mode -> kernel_main (C++)
; GRUB передаёт управление в 32-битном режиме, поэтому сначала идёт 32-битный код.

section .multiboot2
align 8
mb2_start:
    dd 0xE85250D6                                    ; magic Multiboot2
    dd 0                                             ; архитектура i386
    dd mb2_end - mb2_start                           ; длина заголовка
    dd 0x100000000 - (0xE85250D6 + 0 + (mb2_end - mb2_start))
    ; тег: просим графический режим 1024x768x32 (необязательный)
    dw 5, 1
    dd 20
    dd 1024, 768, 32
    dd 0                                             ; выравнивание
    ; конечный тег
    dw 0, 0
    dd 8
mb2_end:

section .bss
align 4096
p4: resb 4096
p3: resb 4096
p2: resb 4096 * 4        ; 4 таблицы по 512 записей = 4 ГБ (нужно для framebuffer)
stack_bottom: resb 16384
stack_top:

section .text
bits 32
global _start
extern kernel_main

_start:
    mov esp, stack_top
    mov edi, eax         ; magic от загрузчика
    mov esi, ebx         ; указатель на multiboot info (cpuid ниже затирает ebx!)

    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode

    ; p4[0] -> p3
    mov eax, p3
    or eax, 0b111          ; present | write | user
    mov [p4], eax
    ; p3[0..3] -> p2 (4 таблицы подряд)
    mov eax, p2
    or eax, 0b111          ; present | write | user
    mov [p3], eax
    add eax, 0x1000
    mov [p3 + 8], eax
    add eax, 0x1000
    mov [p3 + 16], eax
    add eax, 0x1000
    mov [p3 + 24], eax
    ; 2048 страниц по 2 МБ: identity mapping первых 4 ГБ
    xor ecx, ecx
.map_p2:
    mov eax, 0x200000
    mul ecx
    or eax, 0b10000111   ; present | write | user | huge
    mov [p2 + ecx * 8], eax
    inc ecx
    cmp ecx, 2048
    jne .map_p2

    mov eax, p4
    mov cr3, eax
    mov eax, cr4
    or eax, 1 << 5       ; PAE
    mov cr4, eax
    mov ecx, 0xC0000080  ; EFER
    rdmsr
    or eax, 1 << 8       ; LME
    wrmsr
    mov eax, cr0
    or eax, 1 << 31      ; paging
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
    mov edi, edi         ; обнуляем верхние 32 бита
    mov esi, esi
    call kernel_main
.hang:
    cli
    hlt
    jmp .hang

section .rodata
gdt64:
    dq 0
    dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53)
.pointer:
    dw .pointer - gdt64 - 1
    dq gdt64
