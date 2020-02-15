all: ../kernel.bin ../bundle.bin

kernel.elf: *.c *.h
	gcc *.c -g -m32 -o kernel.elf -nostdlib -fno-plt -fno-pic -Wl,-N,-Ttext=0x2000,-Tdata=0x10000 -static -lgcc

kernel.bin: kernel.elf
	objcopy -O binary --only-section .text --only-section .rodata --only-section .data --only-section .bss kernel.elf kernel.bin

setup.bin: setup.asm
	yasm setup.asm -o setup.bin

../kernel.bin: build_bin.py kernel.bin setup.bin kernel.elf
	python3 build_bin.py

userspace/test:
	cd userspace; make test-cpp

../bundle.bin: bundle.asm userspace/test
	yasm bundle.asm -o ../bundle.bin

qemu: all
	cd ../qemu; make run

qemu-debug: all
	cd ../qemu; make debug

qemu-kvm: all
	cd ../qemu; make kvm

bochs: all
	cd ../bochs; make run

clean:
	rm -f kernel.elf kernel.bin setup.bin ../kernel.bin ../bundle.bin
	cd userspace; make clean
