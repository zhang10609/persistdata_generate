INCLUDEDIR = .

#CFLAGS := -I$(INCLUDEDIR) -DMKPDIMAGE_TRACE

CFLAGS := -DMKPDIMAGE_TRACE

all: mkpdimg

mkpdimg: mkpdimg.o
	$(CC) mkpdimg.o -o $@

mkpdimg.o: mkpdimg.c Makefile

clean:
	-rm -f *.o mkpdimg
