# AwizzOS v0.3

64-bit (x86-64) hobby operating system written in C and C++.
Boots via GRUB (Multiboot2), enters Long Mode in `boot/boot.asm`.

## Features

- **Graphical mode** via Multiboot2 framebuffer (preferred 1024×768×32)
- Linear framebuffer drawing (pixels, lines, rectangles, windows)
- Built-in 8×16 bitmap font
- Graphical terminal + interactive shell
- Fallback to classic VGA text mode 80×25 if no framebuffer
- Interrupts (IDT + PIC), PIT timer with live uptime, PS/2 keyboard
- Identity-mapped first 4 GiB (so high framebuffers work)

## Requirements (Debian/Ubuntu/WSL)
```
sudo apt install nasm gcc g++ qemu-system-x86 grub-pc-bin xorriso mtools
```

## Build & Run
```
make          # build/awizz_os.bin
make run      # build ISO and launch in QEMU
make iso      # only build/awizz_os.iso
```

## Shell commands
| Command        | Description                          |
|----------------|--------------------------------------|
| help           | list commands                        |
| clear / cls    | clear terminal                       |
| uptime         | system uptime                        |
| about          | system info                          |
| echo <text>    | print text                           |
| color <0-15>   | set text color                       |
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
If the host/QEMU provides it, AwizzOS switches to full graphical UI.
Otherwise it falls back to VGA text mode automatically.

## License
MIT
