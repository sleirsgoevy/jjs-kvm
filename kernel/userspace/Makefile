all: test

test.o: test.asm
	yasm test.asm -f elf32 -o test.o

test: test.o
	ld -m elf_i386 test.o -o test -nostdlib -static

test-c: test.c
	gcc test.c -g -m32 -static -o test

test-cpp: test.cpp
	g++ test.cpp -g -m32 -static -o test

clean:
	rm -f test.o test
