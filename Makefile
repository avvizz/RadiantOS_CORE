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
       build/vga.o build/gui.o build/pmm.o build/kmalloc.o build/kconsole.o build/vmm.o build/task.o build/switch.o build/fs.o build/elf.o build/gdt.o build/syscall.o build/syscall_entry.o build/kernel.o

all: userspace/hello.elf build/radiantos.bin

userspace/hello.elf: userspace/hello.c userspace/user.ld userspace/syscalls.h
	$(CC) -m64 -ffreestanding -fno-pie -no-pie -fno-stack-protector -nostdlib \
	    -O2 -Wall -T userspace/user.ld -o userspace/hello.elf userspace/hello.c

# Встраиваем hello.elf в заголовок для FS
kernel/user_hello_blob.h: userspace/hello.elf
	xxd -i userspace/hello.elf | sed 's/userspace_hello_elf/user_hello_elf/g' > $@


build:
	mkdir -p build

build/boot.o: boot/boot.asm | build
	$(NASM) -f elf64 $< -o $@
build/elf.o: kernel/user_hello_blob.h

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
	qemu-system-x86_64 -cdrom build/radiantos.iso -m 256 -usb -device usb-tablet

clean:
	rm -rf build

.PHONY: all iso run clean
