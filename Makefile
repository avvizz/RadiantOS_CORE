CC   ?= gcc
CXX  ?= g++
NASM ?= nasm

CFLAGS   = -m64 -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector \
           -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -fno-tree-loop-distribute-patterns
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti -fno-threadsafe-statics
LDFLAGS  = -m64 -T linker.ld -nostdlib -no-pie -Wl,-z,max-page-size=0x1000 -Wl,-z,noexecstack
USERFLAGS = -m64 -ffreestanding -fno-pie -no-pie -fno-stack-protector -nostdlib -O2 -Wall

OBJS = build/boot.o build/isr.o build/idt.o build/pic.o build/timer.o \
       build/keyboard.o build/mouse.o build/string.o build/fb.o \
       build/vga.o build/gui.o build/pmm.o build/kmalloc.o build/kconsole.o \
       build/vmm.o build/task.o build/switch.o build/fs.o build/elf.o \
       build/gdt.o build/syscall.o build/syscall_entry.o build/proc.o \
       build/initrd.o build/kernel.o

all: userspace/hello.elf userspace/shell.elf build/initrd.img build/radiantos.bin

userspace/hello.elf: userspace/hello.c userspace/user.ld userspace/syscalls.h
	$(CC) $(USERFLAGS) -T userspace/user.ld -o $@ userspace/hello.c

userspace/shell.elf: userspace/shell.c userspace/user.ld userspace/syscalls.h
	$(CC) $(USERFLAGS) -T userspace/user.ld -o $@ userspace/shell.c

build/initrd.img: userspace/hello.elf userspace/shell.elf tools/mkinitrd.py | build
	mkdir -p build/initrd_staging
	cp userspace/hello.elf userspace/shell.elf build/initrd_staging/
	echo "RadiantOS initrd" > build/initrd_staging/readme.txt
	echo "Hello from initrd file" > build/initrd_staging/hello.txt
	python3 tools/mkinitrd.py build/initrd.img \
	    build/initrd_staging/hello.elf \
	    build/initrd_staging/shell.elf \
	    build/initrd_staging/readme.txt \
	    build/initrd_staging/hello.txt

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

iso: build/radiantos.bin build/initrd.img
	mkdir -p build/iso/boot/grub
	cp build/radiantos.bin build/iso/boot/
	cp build/initrd.img build/iso/boot/
	cp grub/grub.cfg build/iso/boot/grub/
	grub-mkrescue -o build/radiantos.iso build/iso

run: iso
	qemu-system-x86_64 -cdrom build/radiantos.iso -m 256 -usb -device usb-tablet

clean:
	rm -rf build userspace/*.elf kernel/user_hello_blob.h

.PHONY: all iso run clean
