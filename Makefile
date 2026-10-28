# AwizzOS build system
CC   ?= gcc
CXX  ?= g++
NASM ?= nasm

CFLAGS   = -m64 -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector \
           -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -Ikernel
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti -fno-threadsafe-statics
LDFLAGS  = -m64 -T linker.ld -nostdlib -no-pie -Wl,-z,max-page-size=0x1000 -Wl,-z,noexecstack

OBJS = build/boot.o build/isr.o build/idt.o build/pic.o build/timer.o \
       build/keyboard.o build/vga.o build/gfx.o build/multiboot.o build/kernel.o

all: build/awizz_os.bin

build:
	mkdir -p build

build/boot.o: boot/boot.asm | build
	$(NASM) -f elf64 $< -o $@
build/%.o: kernel/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/%.o: kernel/%.S | build
	$(CC) $(CFLAGS) -c $< -o $@
build/%.o: kernel/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/awizz_os.bin: $(OBJS) linker.ld
	$(CXX) $(LDFLAGS) $(OBJS) -o $@

iso: build/awizz_os.bin
	mkdir -p build/iso/boot/grub
	cp build/awizz_os.bin build/iso/boot/
	cp grub/grub.cfg build/iso/boot/grub/
	grub-mkrescue -o build/awizz_os.iso build/iso

run: iso
	qemu-system-x86_64 -cdrom build/awizz_os.iso -m 256M

clean:
	rm -rf build

.PHONY: all iso run clean
