all: test

test.o: test.asm
	yasm test.asm -f elf32 -o test.o

test: test.o
	ld -m elf_i386 test.o -o test -nostdlib -static

test-c: test.c
	gcc $(CFLAGS) test.c -g -m32 -static -o test

test-cpp: test.cpp
	g++ $(CXXFLAGS) test.cpp -g -m32 -static -o test

test-dyn: test.cpp
	g++ $(CXXFLAGS) test.cpp -g -m32 -o test -fPIE -fPIC

clean:
	rm -f test.o test
