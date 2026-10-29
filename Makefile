# Сборка 64-битной RadiantOS. Хватает обычного gcc на Linux x86-64.
# С кросс-компилятором:  make CC=x86_64-elf-gcc CXX=x86_64-elf-g++
CC   ?= gcc
CXX  ?= g++
NASM ?= nasm

CFLAGS   = -m64 -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector \
           -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -fno-tree-loop-distribute-patterns
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti -fno-threadsafe-statics
LDFLAGS  = -m64 -T linker.ld -nostdlib -no-pie -Wl,-z,max-page-size=0x1000 -Wl,-z,noexecstack

OBJS = build/boot.o build/isr.o build/idt.o build/pic.o build/timer.o \
       build/keyboard.o build/mouse.o build/string.o build/fb.o \
       build/vga.o build/gui.o build/pmm.o build/kmalloc.o build/kernel.o

all: build/radiantos.bin

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

build/radiantos.bin: $(OBJS) linker.ld
	$(CXX) $(LDFLAGS) $(OBJS) -o $@

iso: build/radiantos.bin
	mkdir -p build/iso/boot/grub
	cp build/radiantos.bin build/iso/boot/
	cp grub/grub.cfg build/iso/boot/grub/
	grub-mkrescue -o build/radiantos.iso build/iso

run: iso
	qemu-system-x86_64 -cdrom build/radiantos.iso -m 256

clean:
	rm -rf build

.PHONY: all iso run clean
