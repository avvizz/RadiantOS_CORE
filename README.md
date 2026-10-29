# RadiantOS

64-bit (x86-64) hobby operating system written in C and C++.  
Boots via GRUB (Multiboot2), enters Long Mode in `boot/boot.asm`.

## Features

- **Graphical mode** via Multiboot2 framebuffer (preferred 1024×768×32)
- Linear framebuffer drawing (pixels, lines, rectangles, windows)
- Built-in 8×16 bitmap font
- Modern graphical terminal inside a window (top bar + taskbar)
- Fallback to classic VGA text mode 80×25 if no framebuffer
- Interrupts (IDT + PIC), PIT timer with live uptime, PS/2 keyboard
- Identity-mapped first 4 GiB (so high framebuffers work)

## Requirements (Debian/Ubuntu/WSL)

```bash
sudo apt install nasm gcc g++ qemu-system-x86 grub-pc-bin xorriso mtools
```

## Build & Run

```bash
make          # build/radiant_os.bin
make run      # build ISO and launch in QEMU
make iso      # only build/radiant_os.iso
```

## Shell commands

| Command        | Description                          |
|----------------|--------------------------------------|
| help           | list commands                        |
| clear / cls    | clear terminal                       |
| uptime         | system uptime                        |
| about          | system info                          |
| echo \<text\>  | print text                           |
| color \<0-15\> | set text color                       |
| box            | demo graphical window                |
| lines          | colorful lines demo                  |
| info           | framebuffer details                  |
| reboot / halt  | stop the system                      |

## Project layout

```
boot/          Multiboot2 header + Long Mode (NASM)
kernel/        kernel (C++) + drivers (C) + gfx
grub/          GRUB config
linker.ld
```

## Notes on graphics

GRUB is asked for a 1024×768×32 framebuffer (tag is optional).  
If the host/QEMU provides it, RadiantOS switches to full graphical UI.  
Otherwise it falls back to VGA text mode automatically.

## Running on Android (Termux)

A real x86-64 kernel cannot run natively on ARM phones.  
You can **emulate** it with QEMU inside Termux:

```bash
# Inside Termux
pkg update && pkg install qemu-system-x86_64
# Copy the built radiant_os.iso to the phone, then:
qemu-system-x86_64 -cdrom radiant_os.iso -m 256M -vga std -display sdl
# or without graphics (text only, faster):
qemu-system-x86_64 -cdrom radiant_os.iso -m 128M -nographic
```

Performance will be limited on phones, but the graphical shell works.

## License

MIT
