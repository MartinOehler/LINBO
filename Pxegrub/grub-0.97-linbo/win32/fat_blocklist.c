#define FSYS_FAT
#define DEBUG_FAT 1
#define WITHOUT_LIBC_STUBS 1

#define GRUB_UTIL
#include "../stage2/shared.h"

#include "../stage2/fsys_fat.c"

int
substring (const char *s1, const char *s2)
{
  while (*s1 == *s2)
    {
      /* The strings match exactly. */
      if (! *(s1++))
	return 0;
      s2 ++;
    }

  /* S1 is a substring of S2. */
  if (*s1 == 0)
    return -1;

  /* S1 isn't a substring. */
  return 1;
}


#if 0
#include "win32.h"

/* these are needed to link the fsys_ntfs.c file */
void print_a_completion(char* p) {
}

int fd;
int filepos;
int filemax = 1000000;
unsigned long current_drive;
int current_slice;
int print_possibilities = 0;
grub_error_t errnum;

void (*disk_read_hook) (int, int, int) = NULL;
void (*disk_read_func) (int, int, int) = NULL;


#include <stdio.h>

int devread(int sector, int byte_offset, int byte_len, char* buf) {
  ntfs_s64 offset;
  offset = ((ntfs_s64)sector) * SECTOR_SIZE + byte_offset;
  if (fd < 0) return -1;
  offset = win32_lseek(fd, offset, SEEK_SET);
  if (offset == -1) {
	printf("lseek failed?\n");
  }
  int rvl = 0;
  if (buf) {
	rvl = win32_read(fd, buf, byte_len);
  }
  if (disk_read_hook && disk_read_func) {
	int v;
	disk_read_hook(sector, byte_offset, byte_len);
#ifdef DEBUG_NTFS 
	printf("stage2 version %d.%d\n", 
		buf[SECTOR_SIZE + STAGE2_VER_MAJ_OFFS],
		buf[SECTOR_SIZE + STAGE2_VER_MAJ_OFFS + 1]);
#endif
  }
#ifdef FSYS_FAT
#else
  filepos = win32_filepos(fd);
#endif
  return rvl;
}

#include <windows.h>

char* grub_scratch_mem;
char* dummy;
char global_blocklist[1024] = "";
char* ptr_blocklist = global_blocklist;
unsigned long sector_offset = 0;
int last_sector = -1;
int sector_count = 0;

static void print_blocklist()
{
  if (ptr_blocklist != global_blocklist) {
	*ptr_blocklist++=',';
  }
  ptr_blocklist += sprintf(ptr_blocklist, "%d+%d", 
			sector_offset + last_sector, sector_count);
}

/* Print which sector is read when loading a file.  */
static void
disk_read_print_func (int sector, int offset, int length)
{
  int cnt = length/SECTOR_SIZE + (length%SECTOR_SIZE ? 1 : 0);

  printf ("[%d,%d,%d]\n", sector, offset, length);
  if (last_sector == -1) {
	last_sector = sector;
	sector_count = cnt;	
  } else if (last_sector + sector_count != sector) {
    printf ("sector group [%d,%d]\n", last_sector, sector_count);
    print_blocklist();	
	last_sector = sector;
	sector_count = cnt;	
  } else {
	sector_count += cnt;	
  }
#if 0
  if (ptr_blocklist != global_blocklist) {
	*ptr_blocklist++=',';
  }
  ptr_blocklist += sprintf(ptr_blocklist, "%d+%d", sector_offset + sector, length/SECTOR_SIZE + 1);
#endif 
}

char*
fat_blocklist(char* device, char* path) {

  if (path[1] == ':') { path += 2; };

  grub_scratch_mem = VirtualAlloc(NULL, 0x100000, MEM_COMMIT, PAGE_READWRITE);
  if (grub_scratch_mem == NULL) {
	printf("VirtualAlloc(grub_scratch_mem) failed\n");
	return NULL;
  }

  dummy = VirtualAlloc(NULL, 0x100000, MEM_COMMIT, PAGE_READWRITE);
  if (dummy == NULL) {
  	VirtualFree(grub_scratch_mem, 0, MEM_RELEASE);
	printf("VirtualAlloc(dummy) failed\n");
	return NULL;
  }

  fd = win32_open(device, 0);
  sector_offset = win32_bias_sector(fd);
  printf("sector offset win32_open %ld\n", sector_offset);
  filepos = 0;
  if (!fat_mount()) {
	printf("FAT mount failed\n");
  } else {
	// some function
	char writable_path[1024];
	sprintf(writable_path, path);
	filepos = 0;
	if (!fat_dir(writable_path)) {
	  printf("dir failed %s, %d\n", writable_path, errnum);
	} else {
	  int rcnt;
	  printf("filepos %ld\n", filepos);
	  filepos = 0;
	  disk_read_hook = disk_read_print_func ;
	  rcnt = fat_read(dummy,0x100000);
    	  if (last_sector != -1) {
		printf ("sector group [%d,%d]\n", last_sector, sector_count);
		print_blocklist();
	  }
	  printf("return from ntfs_read %d\n", rcnt);
	  disk_read_hook = NULL;
	  }
  }
  win32_close(fd);

  VirtualFree(grub_scratch_mem, 0, MEM_RELEASE);
  VirtualFree(dummy, 0, MEM_RELEASE);
  return global_blocklist;
}
#endif
