CC      = clang

# FIXME: /usr/src/zfs/include shouldn't be needed!
CFLAGS += -I/usr/include/libspl -I/usr/include/libzfs -I/usr/src/zfs-0.8.3/include

CFLAGS += -D_FILE_OFFSET_BITS=64	\
          -D_LARGEFILE64_SOURCE		\
          -Wall	-g -O2

LDFLAGS = -llz4 -ljson-c -lzpool -lnvpair

all: zdump

zdump: zdump.o
	$(CC) $(LDFLAGS) $^ -o $@

zdump.o: zdump.c
	$(CC) $(CFLAGS) -c $^ -o $@

clean:
	rm -f zdump.o zdump
