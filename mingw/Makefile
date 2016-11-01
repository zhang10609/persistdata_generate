CFLAGS := -DMKPDIMAGE_TRACE

CC = gcc
RM = del

all: mkpdimg

mkpdimg: mkpdimg.o
	$(CC) mkpdimg.o -o $@

mkpdimg.o: mkpdimg.c Makefile

clean:
	$(RM) *.o mkpdimg.exe