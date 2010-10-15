//#define FSYS_NTFS
//#define DEBUG_NTFS 1
#define WITHOUT_LIBC_STUBS 1

//#define STAGE2
#define GRUB_UTIL
#include "../stage2/shared.h"

//#include "../stage2/fsys_ntfs.c"

#include "win32.h"

/* these are needed to link the fsys_ntfs.c file */
void print_a_completion(char* p) {
}

#ifdef FSYS_FAT
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
#endif

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

#define bpb_sector_offset 0x18
#define bpb_head_offset   0x1A
#define bpb_hidden_offset 0x1C
#define bpb_sig_offset    0x42


int
grub_seek (int offset)
{
  if (offset > filemax || offset < 0)
    return -1;

  filepos = offset;
  return offset;
}

int devread(int sector, int byte_offset, int byte_len, char* buf) {
  ntfs_s64 offset;
  offset = ((ntfs_s64)sector) * SECTOR_SIZE + byte_offset;
  if (fd < 0) return -1;
#if 0
  offset = win32_lseek(fd, offset, SEEK_SET);
  if (offset == -1) {
	printf("lseek failed?\n");
  }
#endif
  int rvl = 0;
  if (buf) {
#if 0
	printf("devread %ld:%ld:%ld\n", sector, byte_offset, byte_len);
	rvl = win32_read(fd, buf, byte_len);
#else
	//printf("devread %ld:%ld:%ld\n", sector, byte_offset, byte_len);
	rvl = win32_readsector(fd, buf, sector, byte_offset, byte_len);
#endif
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
  //filepos = win32_filepos(fd);
#if 0
  if (sector == 0) {
	printf("BPB info: hidden sector %ld, sector per track %d, heads %d, id %x\n", 
		*(long *)&buf[bpb_hidden_offset], 
		*(short *)&buf[bpb_sector_offset], 
		*(short *)&buf[bpb_head_offset],
		(int)*(char *)&buf[bpb_sig_offset]);
  }
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
  //printf ("[%d,%d,%d]\n", sector_offset, last_sector, sector_count);
  ptr_blocklist += sprintf(ptr_blocklist, "%d+%d", 
			sector_offset + last_sector, sector_count);
}

/* Print which sector is read when loading a file.  */
static void
disk_read_print_func (int sector, int offset, int length)
{
  int cnt = length/SECTOR_SIZE + (length%SECTOR_SIZE ? 1 : 0);

  //printf ("[%d,%d,%d]\n", sector, offset, length);
  if (last_sector == -1) {
	last_sector = sector;
	sector_count = cnt;	
  } else if (last_sector + sector_count != sector) {
    //printf ("sector group [%d,%d]\n", last_sector, sector_count);
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

#if 0
char*
orig_ntfs_blocklist(char* device, char* path) {

  if (path[1] == ':') { path += 2; };

  grub_scratch_mem = VirtualAlloc(NULL, 0x100000, MEM_COMMIT, PAGE_READWRITE);
  if (grub_scratch_mem == NULL) {
	printf("VirtualAlloc failed\n");
	return NULL;
  }
  fd = win32_open(device, 0);
  filepos = win32_filepos(fd);
  if (!ntfs_mount() && fat_mount()) {
	printf("mount failed\n");
  } else {
	// some function
	char writable_path[1024];
	sprintf(writable_path, path);
	if (!ntfs_dir(writable_path) && !fat_dir(writable_path)) {
	  printf("dir failed %s\n", writable_path);
	} else {
	  RUNL *runl = &cmft->runl;
	  char* ptr = global_blocklist;
	  rewind_run_list(runl);
	  while (get_next_run(runl)) {
		printf("sector %d len %d vcn=0x%x ecn=0x%x\n", 
			   runl->cnum*8+63, runl->clen*8, runl->vcn, runl->evcn);
		if (ptr != global_blocklist) {
		  *ptr++=',';
		}
		ptr += sprintf(ptr, "%d+%d", runl->cnum*8+63, runl->clen*8);
	  }
	}
  }
  win32_close(fd);

  VirtualFree(grub_scratch_mem, 0, MEM_RELEASE);
  return global_blocklist;
}
#endif

char*
ntfs_blocklist(char* device, char* path) {

  char device_name[64];

  if (is9xME()) return global_blocklist;
  strcpy(device_name, device);
  if (path[1] == ':') { 
	device_name[0] = path[0], device_name[1]=path[1], device_name[2] = '\0';
	path += 2; 
  }

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

  //fd = win32_open(device_name, 0);
  fd = win32_open(device, 0);
  sector_offset = win32_bias_sector(fd);
  //printf("sector offset win32_open %ld\n", sector_offset);
  filepos = 0;
  if (!ntfs_mount()) {
	//printf("NTFS mount failed\n");
  } else {
	// some function
	char writable_path[1024];
	sprintf(writable_path, path);
	if (!ntfs_dir(writable_path)) {
	  printf("dir failed %s, %d\n", writable_path, errnum);
	} else {
	  int rcnt;
	  //printf("filepos %ld\n", filepos);
	  filepos = 0;
	  disk_read_hook = disk_read_print_func ;
	  rcnt = ntfs_read(dummy,0x100000);
    	  if (last_sector != -1) {
		//printf ("sector group [%d,%d]\n", last_sector, sector_count);
		print_blocklist();
	  }
	  //printf("return from ntfs_read %d\n", rcnt);
	  disk_read_hook = NULL;
	  }
  }
  win32_close(fd);

  VirtualFree(grub_scratch_mem, 0, MEM_RELEASE);
  VirtualFree(dummy, 0, MEM_RELEASE);
  return global_blocklist;
}

char*
fat_blocklist(char* device, char* path) {

  char device_name[64];

  strcpy(device_name, device);
  if (path[1] == ':') { 
	device_name[0] = path[0], device_name[1]=path[1], device_name[2] = '\0';
	path += 2; 
  }

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
  //fd = win32_open(device_name, 0);
  sector_offset = win32_bias_sector(fd);
  //printf("sector offset win32_open %ld\n", sector_offset);
  filepos = 0;
  if (!fat_mount()) {
	//printf("FAT mount failed\n");
  } else {
	// some function
	char writable_path[1024];
	sprintf(writable_path, path);
	filepos = 0;
	if (!fat_dir(writable_path)) {
	  printf("dir failed %s, %d\n", writable_path, errnum);
	} else {
	  int rcnt;
	  //printf("filepos %ld\n", filepos);
	  filepos = 0;
	  disk_read_hook = disk_read_print_func ;
	  rcnt = fat_read(dummy,0x100000);
    	  if (last_sector != -1) {
		//printf ("sector group [%d,%d]\n", last_sector, sector_count);
		print_blocklist();
	  }
	  //printf("return from ntfs_read %d\n", rcnt);
	  disk_read_hook = NULL;
	  }
  }
  win32_close(fd);

  VirtualFree(grub_scratch_mem, 0, MEM_RELEASE);
  VirtualFree(dummy, 0, MEM_RELEASE);
  return global_blocklist;
}
