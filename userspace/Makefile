CC = gcc
LD = ld
CFLAGS = -ffreestanding -nostdlib -fno-builtin -fno-stack-protector -m64
LDFLAGS = -T link.ld -nostdlib

all: hello.elf

hello.o: hello.c
	$(CC) $(CFLAGS) -c hello.c -o hello.o

hello.elf: hello.o
	$(LD) $(LDFLAGS) hello.o -o hello.elf

clean:
	rm -f *.o *.elf *.h

# Convert to C array for embedding
hello.h: hello.elf
	xxd -i hello.elf > hello.h