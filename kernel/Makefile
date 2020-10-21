all: ../kernel.bin ../bundle.bin

kernel.elf: *.c *.h
	gcc $(CFLAGS) *.c *.S -g -m32 -o kernel.elf -ffreestanding -no-pie -nostdlib -fno-plt -fno-pic -Wl,-N,-Ttext=0x2000,-Tdata=0x10000 -static -lgcc

kernel.bin: kernel.elf
	objcopy -O binary --only-section .text --only-section .rodata --only-section .data --only-section .bss kernel.elf kernel.bin

release:
	make clean
	make CFLAGS=-DNDEBUG\ -O3 ../kernel.bin

dist: release
	cd ../qemu; make dist

setup.bin: setup.asm
	yasm setup.asm -o setup.bin

../kernel.bin: build_bin.py kernel.bin setup.bin kernel.elf
	python3 build_bin.py

../bundle.bin: bundle.asm
	cd userspace; make test-dyn
	yasm bundle.asm -o ../bundle.bin

qemu: all
	cd ../qemu; make run

qemu-debug: all
	cd ../qemu; make debug

qemu-kvm: all
	cd ../qemu; make kvm

qemu-kvm-debug: all
	cd ../qemu; make kvm-debug

bochs: all
	cd ../bochs; make run

clean:
	rm -f kernel.elf kernel.bin setup.bin ../kernel.bin ../bundle.bin
	cd userspace; make clean
