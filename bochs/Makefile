all: hdd.img

hdd.img:
	cd ../qemu; make rmboot.img
	cp ../qemu/rmboot.img hdd.img
	dd if=/dev/null of=hdd.img bs=1048576 seek=1

run: hdd.img
	rm -f hdd.img.lock
	bochs

clean:
	rm -f hdd.img
