all: kernel.elf rmboot.img

kernel.o: ../kernel.bin
	objcopy -I binary ../kernel.bin -O elf32-i386 -B i386 kernel.o

kernel.elf: kernel.o
	ld -m elf_i386 -nostdlib -static -Ttext=0x108000 kernel.o -o kernel.elf

rmboot.img: rmboot.asm
	yasm rmboot.asm -o rmboot.img

run: all
	fuser -k `which /usr/bin/qemu-system-i386` || true
	qemu-system-i386 -m 4096 -hda rmboot.img -device loader,file=kernel.elf -device loader,file=../bundle.bin,addr=0x1000000,force-raw=on -debugcon file:/dev/stdout -s

debug: all
	fuser -k `which /usr/bin/qemu-system-i386` || true
	qemu-system-i386 -m 4096 -hda rmboot.img -device loader,file=kernel.elf -device loader,file=../bundle.bin,addr=0x1000000,force-raw=on -debugcon file:/dev/stdout -s -S &
	gdb -x gdbinit ../kernel/kernel.elf

kvm: all
	fuser -k `which /usr/bin/qemu-system-i386` || true
	qemu-system-i386 -m 4096 -hda rmboot.img -device loader,file=kernel.elf -device loader,file=../bundle.bin,addr=0x1000000,force-raw=on -debugcon file:/dev/stdout -s -enable-kvm

clean:
	rm -f rmboot.img kernel.o kernel.elf
