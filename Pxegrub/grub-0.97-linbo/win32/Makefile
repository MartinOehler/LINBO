CFLAGS = -I. -I../stage1 -mno-cygwin 
LDFLAGS = -s -mno-cygwin 

all: w32grub.exe

%.o:%.c
	$(CC) $(CFLAGS) -c $^ -o $@

w32grub.exe: install.o stages.o win32.o \
		fat_blocklist.o ntfs_blocklist.o blocklist.o
	$(CC) $(LDFLAGS) $^ -o $@


clean:
	rm -f w32grub.exe *~ *.o *.stackdump


