/*
 * win32.c: a stdio-like disk I/O implementation for low-level disk access on Win32
 *          can access an NTFS-volume while mounted!
 */

#include <windows.h>
#include <winioctl.h>

#include <stdio.h>
#include <ctype.h>

#include "win32.h"
//#define DEBUG
#define FORCE_ALIGNED_READ

typedef struct win32_fd {
  HANDLE handle;
  LARGE_INTEGER part_start;
  LARGE_INTEGER part_end;
  LARGE_INTEGER current_pos;
  int logical;
  int drive;
  int mode;
} win32_fd;

win32_fd win32_fds[64] = { 
  {(HANDLE)42}, // fd==0 breaks some code in attr.c, so skip it
  {0}};

#ifdef DEBUG
static __inline__ void Dprintf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	//vfprintf(stdout, fmt, ap);
	vprintf(fmt, ap);
	va_end(ap);
}
#else
static void Dprintf(const char *fmt, ...) {}
#endif

#define perror(msg) win32_perror(__FILE__,__LINE__,__FUNCTION__,msg)

int win32_perror(char* file, int line, char* func, char* msg) {
	char buffer[1024] = "";
	DWORD err = GetLastError();
	if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0, buffer, sizeof(buffer), NULL) <= 0) {
	  sprintf(buffer, "HRESULT 0x%lx", err);
	}
	//fprintf(stderr, "%s(%d):%s\t%s %s\n", file, line, func, buffer, msg);
	printf("%s(%d):%s\t%s %s\n", file, line, func, buffer, msg);
	return 0;
}

int logical_to_physical(char *name, int *drive, int *part)
{
	PARTITION_INFORMATION partition;
	DWORD numread=sizeof(partition);
	char buffer[10240];
	int  found = 0;
	char filename[256];
	

	if (*name == '(') {
		int num;
		num = sscanf(name,"(hd%d,%d)", drive,part);
		if (num == 1) *part = -1;
		else if (num == 0) return 0;
		return 1;
	}

	sprintf(filename, "\\\\.\\%.2s", name);
	HANDLE handle = CreateFile(filename,
							   GENERIC_READ, // no access, just get info
							   FILE_SHARE_READ|FILE_SHARE_WRITE, 
							   NULL, 
							   OPEN_EXISTING, 
							   FILE_ATTRIBUTE_SYSTEM,
							   NULL);
	if (handle == INVALID_HANDLE_VALUE) {
	  char msg[1024];
	  sprintf(msg,"CreateFile(%s) failed", name);
	  perror(msg);
	  return found;
	}

	  
	BOOL rvl = DeviceIoControl(handle,
							 IOCTL_DISK_GET_PARTITION_INFO,
							 NULL, 0,
							 &partition, sizeof(partition),
							 &numread,
							 NULL);
	if (!rvl) {
	  perror("ioctl failed");
	  CloseHandle(handle);
	  }
	else {
	  int  i = 0;

	  CloseHandle(handle); // close the logical one

	  do {
	  	sprintf(filename, "\\\\.\\PhysicalDrive%d", i++);
		HANDLE handle = CreateFile(filename,
						   	   GENERIC_READ, 
							   FILE_SHARE_READ|FILE_SHARE_WRITE, 
							   NULL, 
							   OPEN_EXISTING, 
							   FILE_ATTRIBUTE_SYSTEM,
							   NULL);

		if (handle != INVALID_HANDLE_VALUE) {
	  		DWORD numread=sizeof(buffer);
	  		DRIVE_LAYOUT_INFORMATION* drive_layout;
			int j=0;
	  
	  		BOOL rvl = DeviceIoControl(handle,
								 IOCTL_DISK_GET_DRIVE_LAYOUT,
								 NULL, 0,
								 &buffer, sizeof(buffer),
								 &numread,
								 NULL);
	  		if (!rvl) {
	  			perror("ioctl failed");
	  			CloseHandle(handle);
				break;
			}
			drive_layout = (DRIVE_LAYOUT_INFORMATION*)buffer;
			for (j=0; j < drive_layout->PartitionCount; j++) {
				if (drive_layout->PartitionEntry[j].StartingOffset.QuadPart ==
					partition.StartingOffset.QuadPart &&
					drive_layout->PartitionEntry[j].PartitionLength.QuadPart ==
					partition.PartitionLength.QuadPart) {
					*drive = i-1;
					*part = j;
					found = 1;
					break;
				}
					
			}
		    CloseHandle(handle);
		} else{
		  Dprintf("Can't open %s", filename);
		}
	  } while (!found);	
	}
	if (found) Dprintf("file/device %s is on (hd%d,%d)\n", name, *drive,*part);
	return found;
}

/*
 * open a file
 * if name is in format "(hd[0-9],[0-9])" then open a partition
 * if name is in format "(hd[0-9])" then open a volume
 * else open a file 
 */

int win32_open(const char* name, int mode) {
  int drive = 0;
  int part = 0;
  int fd = 1;
  int logical_drive = 0;
  int floppy = 0;

  // paranoia check
  while (win32_fds[fd].handle != 0) {
	fd++;
	if (fd>sizeof(win32_fds)/sizeof(win32_fds[0])) {
	  perror("too many open files");
	  return -1;
	}
  }

  win32_fds[fd].logical = 0;
  win32_fds[fd].mode = mode;
  win32_fds[fd].drive = 0;
  // parse name
  int numparams;
  numparams = sscanf(name,"(hd%d,%d)",&drive,&part);
  if (name[1] == ':') {
    logical_drive = 1;
	floppy = (drive = toupper(name[0]) - 'A') < 2;
	if (!floppy) drive-=2;
  }

  if (numparams >= 1 || logical_drive) {
	if (is9xME()) return win32_open9x(fd, drive+ (floppy ? 1 :3));
	if (logical_drive) {
	  Dprintf("win32_open(%s) -> drive %s", name,name);
	} else if (numparams == 2) {
	  Dprintf("win32_open(%s) -> drive %d, part %d\n", name, drive, part);
	} else {
	  Dprintf("win32_open(%s) -> drive %d\n", name, drive);
	}

	char filename[256];
	if (logical_drive) sprintf(filename,"\\\\.\\%s",name);
	else sprintf(filename, "\\\\.\\PhysicalDrive%d", drive);
	
	HANDLE handle = CreateFile(filename,
							   GENERIC_READ|((mode != 0) ? GENERIC_WRITE : 0), 
							   FILE_SHARE_READ|FILE_SHARE_WRITE, 
							   NULL, 
							   OPEN_EXISTING, 
							   FILE_ATTRIBUTE_SYSTEM,
							   NULL);
	if (handle == INVALID_HANDLE_VALUE) {
	  char msg[1024];
	  sprintf(msg,"CreateFile(%s) failed", filename);
	  perror(msg);
	  return -1;
	}
	win32_fds[fd].logical = 0;

	if (logical_drive) {
		if (!floppy) {
			PARTITION_INFORMATION partition;
			DWORD numread=sizeof(partition);
			char buffer[10240];

	  
			BOOL rvl = DeviceIoControl(handle,
							 IOCTL_DISK_GET_PARTITION_INFO,
							 NULL, 0,
							 &partition, sizeof(partition),
							 &numread,
							 NULL);
			if (!rvl) {
	  			perror("ioctl failed");
	  		}
			else {
	  			win32_fds[fd].handle = handle;
	  			win32_fds[fd].part_start = partition.StartingOffset;
	  			win32_fds[fd].part_end.QuadPart = 
					partition.StartingOffset.QuadPart +
					partition.PartitionLength.QuadPart;
	  			win32_fds[fd].current_pos.QuadPart = 0;
				win32_fds[fd].logical = 1;
			}
	  		Dprintf("win32_open(%s) -> getting partition information", name);
		} else {
	  		win32_fds[fd].handle = handle;
	  		win32_fds[fd].part_start.QuadPart = 0;
	  		win32_fds[fd].part_end.QuadPart = 1440*1024; 
	  		win32_fds[fd].current_pos.QuadPart = 0;
			win32_fds[fd].logical = 1;
		}

	} else if (numparams == 1) {
	  win32_fds[fd].handle = handle;
	  win32_fds[fd].part_start.QuadPart = 0;
	  win32_fds[fd].part_end.QuadPart = -1;
	  win32_fds[fd].current_pos.QuadPart = 0;
	} else {
	  char buffer[10240];
	  DWORD numread;
	  
	  BOOL rvl = DeviceIoControl(handle,
								 IOCTL_DISK_GET_DRIVE_LAYOUT,
								 NULL, 0,
								 &buffer, sizeof(buffer),
								 &numread,
								 NULL);
	  if (!rvl) {
		perror("ioctl failed");
		return -1;
	  }
	  
	  DRIVE_LAYOUT_INFORMATION* drive_layout = (DRIVE_LAYOUT_INFORMATION*)buffer;
	  if (part >= drive_layout->PartitionCount) {
		printf("partition %d not found on drive %d", part, drive);
		return -1;
	  }
	  
	  win32_fds[fd].handle = handle;
	  win32_fds[fd].part_start = drive_layout->PartitionEntry[part].StartingOffset;
	  win32_fds[fd].part_end.QuadPart = 
		drive_layout->PartitionEntry[0].StartingOffset.QuadPart +
		drive_layout->PartitionEntry[0].PartitionLength.QuadPart;
	  win32_fds[fd].current_pos.QuadPart = 0;
	}
  } else {
	Dprintf("win32_open(%s) -> file\n", name);
	HANDLE handle = CreateFile(name, GENERIC_READ|((mode != 0) ? GENERIC_WRITE : 0), FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	BY_HANDLE_FILE_INFORMATION info;
	BOOL rvl = GetFileInformationByHandle(handle, &info);
	if (!rvl) {
	  perror("ioctl failed");
	  return -1;
	}
	win32_fds[fd].handle = handle;
	  win32_fds[fd].part_start.QuadPart = 0;
	  win32_fds[fd].part_end.QuadPart = (((ntfs_s64)info.nFileSizeHigh)<<32)+((ntfs_s64)info.nFileSizeLow);
	  win32_fds[fd].current_pos.QuadPart = 0;	
	  
  }
  Dprintf("win32_open(%s) -> offset 0x%llx\n", name, win32_fds[fd].part_start);
  
  return fd;
}

#define MY_LSEEK
#ifdef MY_LSEEK 
ntfs_s64 win32_lseek(const int fd, ntfs_s64 pos, int mode)
{
  LARGE_INTEGER offset;
  int disp;
  HANDLE hf = win32_fds[fd].handle;

  if (mode == SEEK_SET) {
    disp=FILE_BEGIN;
	offset.QuadPart = pos;
    offset.QuadPart += win32_fds[fd].logical ? 0 : win32_fds[fd].part_start.QuadPart;
  } else if (mode == SEEK_CUR) {
	disp=FILE_CURRENT;
	offset.QuadPart = pos;
  } else if (mode == SEEK_END) {
	// end of partition != end of disk
	disp=FILE_BEGIN;
	offset.QuadPart = win32_fds[fd].part_end.QuadPart + pos 
			  - win32_fds[fd].logical ? 0 : win32_fds[fd].part_start.QuadPart;
  }

  Dprintf("win32_seek SetFilePointer(%lx:%lx)\n", offset.HighPart, offset.LowPart);
  offset.LowPart = SetFilePointer(hf, offset.LowPart, &offset.HighPart, disp);
  if (offset.LowPart == INVALID_SET_FILE_POINTER && 
	GetLastError() != NO_ERROR) {
	perror("SetFilePointer failed");
	offset.QuadPart = -1;
  }
  Dprintf("win32_seek final(%llx)\n", offset.QuadPart);
  win32_fds[fd].current_pos = offset;
  return offset.QuadPart;
}
#else

ntfs_s64 win32_lseek(const int fd, ntfs_s64 pos, int mode) {

  LARGE_INTEGER offset;

  int disp;

  offset.QuadPart = pos;
  Dprintf("win32_seek(0x%llx,%d, logical %d)\n", offset.QuadPart,mode, win32_fds[fd].logical);
  if (mode==SEEK_SET) {
    disp=FILE_BEGIN;
	offset.QuadPart = pos;
    offset.QuadPart += (win32_fds[fd].logical != 0) ? 0 : win32_fds[fd].part_start.QuadPart;
  } else if (mode==SEEK_CUR) {
	disp=FILE_CURRENT;
	offset.QuadPart = pos;
  } else if (mode==SEEK_END) {
	// end of partition != end of disk
	disp=FILE_BEGIN;
	offset.QuadPart = win32_fds[fd].part_end.QuadPart + pos 
			  - win32_fds[fd].logical ? 0 : win32_fds[fd].part_start.QuadPart;
  } else {
	printf("win32_seek() wrong mode %d\n", mode);
	return -1;
  }

  Dprintf("win32_seek SetFilePointer(%llx)\n", offset.QuadPart);
  BOOL rvl = SetFilePointerEx(win32_fds[fd].handle, offset, &win32_fds[fd].current_pos, disp);

  if (!rvl) {
	perror("SetFilePointer failed");
	return -1;
  }
  Dprintf("win32_seek final pos(%lld)\n", win32_fds[fd].current_pos);
  return pos;
}
#endif
ntfs_s64 win32_read(const int fd, const void *b, ntfs_s64 count) {
  LARGE_INTEGER base, offset, numtoread, l_count;
  l_count.QuadPart = count;
  offset.QuadPart = win32_fds[fd].current_pos.QuadPart & 0x1FF;
  base.QuadPart = win32_fds[fd].current_pos.QuadPart - offset.QuadPart;
  numtoread.QuadPart = ((count+offset.QuadPart-1) | 0x1FF) + 1;

  Dprintf("win32_read(fd=%d,b=%p,count=0x%llx)->(%llx+%llx:%llx)\n", fd, b, count, base, offset,numtoread);

  DWORD numread = 0;
  BOOL rvl = -1;

#ifndef FORCE_ALIGNED_READ
  if (
	  ((((long)b) & ((ntfs_s64)0x1FF)) == 0) 
		&& ((count & ((ntfs_s64)0x1FF))==0)
	  && ((win32_fds[fd].current_pos.QuadPart & 0x1FF) == 0)
	  ) {
	Dprintf("normal read\n");
	rvl = ReadFile(win32_fds[fd].handle, (LPVOID)b, count, &numread, (LPOVERLAPPED)NULL);
  } else {
	Dprintf("aligned read\n");
#endif
	BYTE* alignedbuffer = (BYTE*)VirtualAlloc(NULL, count, MEM_COMMIT, PAGE_READWRITE);
	Dprintf("set SetFilePointerEx(%llx)\n", base.QuadPart);
#if 0
	rvl = SetFilePointerEx(win32_fds[fd].handle, base, NULL, FILE_BEGIN);
#else
	int ol = win32_fds[fd].logical;
	win32_fds[fd].logical = 1;
	rvl = (win32_lseek(fd, base.QuadPart, SEEK_SET) != -1);
	win32_fds[fd].logical = ol;
#endif

	rvl = ReadFile(win32_fds[fd].handle, (LPVOID)alignedbuffer, numtoread.QuadPart, &numread, (LPOVERLAPPED)NULL);

	LARGE_INTEGER new_pos;
	ntfs_s64 npos = (ntfs_s64) win32_fds[fd].current_pos.QuadPart + count;
    new_pos.QuadPart = npos;
	Dprintf("reset SetFilePointerEx(%llx)\n", new_pos.QuadPart);
    //if (rvl) rvl = (win32_lseek(fd,count,SEEK_CUR) != -1);
	//rvl = SetFilePointerEx(win32_fds[fd].handle, new_pos, &win32_fds[fd].current_pos, FILE_BEGIN);
    Dprintf("win32_seek final pos(%lld)\n", win32_fds[fd].current_pos);
	if (!rvl) {
	  // printf("SetFilePointerEx failed");
	}

	memcpy((void*)b,alignedbuffer+offset.QuadPart,count);
	VirtualFree(alignedbuffer, 0, MEM_RELEASE);
#ifndef FORCE_ALIGNED_READ
  }
#endif
  if (!rvl) {
	perror("ReadFile failed");
	return -1;
  }

#if 0
  int c = 0;
  for (c=0; c<count; c++) {
	if (strncmp(b+c,"[boot loader]",strlen("[boot loader]"))==0) {
	  printf("FOUND at offset %llx->%llx,%d\n", win32_fds[fd].current_pos,win32_fds[fd].current_pos.QuadPart-win32_fds[fd].part_start.QuadPart,c);
	}
  }
#endif

  if (numread > count) {
	return count;
  } else {
	return numread;
  }
}

ntfs_s64 win32_write(const int fd, const void *b, ntfs_s64 count) {
  LARGE_INTEGER base, offset, numtoread, l_count;
  l_count.QuadPart = count;
  offset.QuadPart = win32_fds[fd].current_pos.QuadPart & 0x1FF;
  base.QuadPart = win32_fds[fd].current_pos.QuadPart - offset.QuadPart;
  numtoread.QuadPart = ((count+offset.QuadPart-1) | 0x1FF) + 1;

  Dprintf("win32_write(fd=%d,b=%p,count=0x%llx)->(%llx+%llx:%llx)\n", fd, b, count, base, offset,numtoread);

  DWORD numread = 0;
  BOOL rvl = -1;
  int  data_written = 0;

#ifndef FORCE_ALIGNED_READ
  if (
	  ((((long)b) & ((ntfs_s64)0x1FF)) == 0) 
		&& ((count & ((ntfs_s64)0x1FF))==0)
	  && ((win32_fds[fd].current_pos.QuadPart & 0x1FF) == 0)
	  ) {
	Dprintf("normal write\n");
	rvl = WriteFile(win32_fds[fd].handle, (LPVOID)b, count, &numread, (LPOVERLAPPED)NULL);
  } else {
	Dprintf("aligned write\n");
#endif
	BYTE* alignedbuffer = (BYTE*)VirtualAlloc(NULL, count, MEM_COMMIT, PAGE_READWRITE);
	memcpy(alignedbuffer+offset.QuadPart,(void*)b,count);
	Dprintf("set SetFilePointerEx(%llx)\n", base.QuadPart);
#if 0
	rvl = SetFilePointerEx(win32_fds[fd].handle, base, NULL, FILE_BEGIN);
#else
	int ol = win32_fds[fd].logical;
	win32_fds[fd].logical = 1;
	rvl = (win32_lseek(fd, base.QuadPart, SEEK_SET) != -1);
	win32_fds[fd].logical = ol;
#endif

	if (rvl) rvl = WriteFile(win32_fds[fd].handle, (LPVOID)alignedbuffer, numtoread.QuadPart, &numread, (LPOVERLAPPED)NULL);
	else printf("fail to seek to %lld\n", base.QuadPart);
	if (!rvl) {
		perror("WriteFile failed");
		}
	else data_written = 1; 
	LARGE_INTEGER new_pos;
	ntfs_s64 npos = (ntfs_s64) win32_fds[fd].current_pos.QuadPart + count;
    new_pos.QuadPart = npos;
	Dprintf("reset SetFilePointerEx(%llx)\n", new_pos.QuadPart);
    //if (rvl) rvl = (win32_lseek(fd,count,SEEK_CUR) != -1);
	//rvl = SetFilePointerEx(win32_fds[fd].handle, new_pos, &win32_fds[fd].current_pos, FILE_BEGIN);
    Dprintf("win32_seek final pos(%lld)\n", win32_fds[fd].current_pos);
	if (!rvl) {
	  printf("SetFilePointer failed");
	  rvl = 0;
	}

	VirtualFree(alignedbuffer, 0, MEM_RELEASE);
#ifndef FORCE_ALIGNED_READ
  }
#endif
  if (!data_written) {
	perror("WriteFile failed");
	return -1;
  }

#if 0
  int c = 0;
  for (c=0; c<count; c++) {
	if (strncmp(b+c,"[boot loader]",strlen("[boot loader]"))==0) {
	  printf("FOUND at offset %llx->%llx,%d\n", win32_fds[fd].current_pos,win32_fds[fd].current_pos.QuadPart-win32_fds[fd].part_start.QuadPart,c);
	}
  }
#endif

  if (numread > count) {
	return count;
  } else {
	return numread;
  }
}
int win32_close(int fd) {
  
  Dprintf("win32_close(%d)\n", fd);

  BOOL rvl = CloseHandle(win32_fds[fd].handle);
  win32_fds[fd].handle = 0;

  if (!rvl) {
	perror("CloseHandle failed");
	return -1;
  }

  return 0;
}

ntfs_s64 win32_bias(int fd) {
  return win32_fds[fd].part_start.QuadPart;
}

unsigned long win32_bias_sector(int fd) {
  return (win32_fds[fd].part_start.QuadPart/512);
}

ntfs_s64 win32_filepos(int fd) {
  return win32_fds[fd].current_pos.QuadPart;
}

#define VWIN32_DIOC_DOS_DRIVEINFO (6)
#define VWIN32_DIOC_DOS_INT13   (4)
#define VWIN32_DIOC_DOS_INT25   (2)
#define VWIN32_DIOC_DOS_INT26   (3)
#define VWIN32_DIOC_DOS_IOCTL	(1)

typedef struct _DIOC_REGISTERS
{
    DWORD reg_EBX;
    DWORD reg_EDX;
    DWORD reg_ECX;
    DWORD reg_EAX;
    DWORD reg_EDI;
    DWORD reg_ESI;
    DWORD reg_Flags;
} DIOC_REGISTERS, *PDIOC_REGISTERS;

#pragma pack(1)
struct media
{
    WORD infolevel;
    DWORD serialnumber;
    char vollable[11];
    char filesystype[8];
};

struct DISKIO
{
    DWORD startsector;
    WORD  sectorsnum;
    DWORD buff;
};

struct boot
{
	BYTE jump[3] ;
	char OEMname[8] ;
	WORD bps ;
	BYTE spc ;
	WORD reservedsec ;
	BYTE fatcopies ;
	WORD maxdirentries ;
	WORD totalsec ;
	BYTE mediadesc ;
	WORD secperfat ;
	WORD secpertrack ;
	WORD noofsides ;
	DWORD hidden ;
	DWORD hugesec ;
    BYTE drivenumber ;
    BYTE reserved ;
    BYTE bootsignature ;
    DWORD volumeid ;
    char volumelabel[11] ;
    char filesystype[8] ;
    BYTE unused[450] ;
} ;

struct boot32 
{
    BYTE jump[3] ;
	char bsOemName[8] ;
    WORD BytesPerSector ;
    BYTE SectorsPerCluster ;
    WORD ReservedSectors ;
    BYTE NumberOfFATs ;
    WORD RootEntries ;
    WORD TotalSectors ;
    BYTE MediaDescriptor ;
    WORD SectorsPerFAT ;
    WORD SectorsPerTrack ;
    WORD Heads ;
    WORD HiddenSectors ;
    WORD HiddenSectorsHigh ;
    WORD BigTotalSectors ;
    WORD BigTotalSectorsHigh ;
    WORD BigSectorsPerFat ;
    WORD BigSectorsPerFatHi ;
    WORD ExtFlags ;
    WORD FS_Version ;
    WORD RootDirStrtClus ;
    WORD RootDirStrtClusHi ;
    WORD FSInfoSec ;
    WORD BkUpBootSec ;
    WORD Reserved[6] ;
    BYTE bsDriveNumber ;		  
    BYTE bsReserved ;
    BYTE bsBootSignature ;
    DWORD bsVolumeID ;	
    char bsVolumeLabel[11] ;
    char bsFileSysType[8] ;
	BYTE unused [422] ;
} ;

struct 	bigfatbootfsinfo
{
	DWORD	FSInf_Sig ;
	DWORD	FSInf_free_clus_cnt ;
	DWORD	FSInf_next_free_clus ;
	DWORD	FSInf_resvd[3] ;
} ;

struct  partitiontable
{
	BYTE boot_flag;	
	BYTE head_start;
	WORD cylinder_sector_start;
	BYTE fs_type;
	BYTE head_end;
	WORD cylinder_sector_end;
	DWORD sect_start;
	DWORD sect_total;
};

#pragma pack()

int win32_disk95hasInt13ext(HANDLE hDevice, int drive)
{
   DIOC_REGISTERS reg;
   DWORD byteCnt; 
   int fResult;

   reg.reg_EAX = 0x4100;
   reg.reg_EBX = 0x55aa;
   reg.reg_EDX = drive;
   reg.reg_Flags=0x0001;

   fResult = DeviceIoControl(hDevice, VWIN32_DIOC_DOS_INT13, &reg,
		sizeof(reg), &reg, sizeof(reg), &byteCnt, 0);

   Dprintf("check int13 extension result flags:%lx, bx:%lx\n", 
		reg.reg_Flags, reg.reg_EBX);
   if (fResult && (reg.reg_Flags & 0x0001) == 0 && reg.reg_EBX == 0xaa55) {
	return 1;
	}
   else return 0;
}

int win32_get95disk_info(HANDLE hDevice, int drive, WORD* cylinder, 
WORD *sector, WORD* head)
{
   DIOC_REGISTERS reg;
   DWORD byteCnt; 
   int fResult;

   reg.reg_EAX = 0x0800;
   reg.reg_EBX = 0x0;
   reg.reg_ECX = 0x0;
   reg.reg_EDX = drive;
   reg.reg_Flags=0x0001;

   fResult = DeviceIoControl(hDevice, VWIN32_DIOC_DOS_INT13, &reg,
		sizeof(reg), &reg, sizeof(reg), &byteCnt, 0);

   Dprintf("return of get disk info cx:%lx dx:%lx ax:%lx flag:%lx\n",
		reg.reg_ECX,reg.reg_EDX,reg.reg_EAX, reg.reg_Flags);

   if (fResult && (reg.reg_Flags & 0x0001) == 0) {
	*sector = reg.reg_ECX & 0x3f;
	*cylinder = reg.reg_ECX >> 6;
	*head = reg.reg_EDX >> 8;
	return 0;
	}
   else return 1;
}

int is9xME()
{
	OSVERSIONINFO ovinfo;
	ovinfo.dwOSVersionInfoSize=sizeof (OSVERSIONINFO);
	GetVersionEx(&ovinfo);
	if(ovinfo.dwPlatformId==VER_PLATFORM_WIN32_NT)
		return 0;
	else {
		return 1;
	}
}

int getmediaid (HANDLE hDevice, int drive_1, struct media *m )
{
	DIOC_REGISTERS r = {0};
	DWORD cb ;
	
	r.reg_EAX = 0x440d ;    
	r.reg_EBX = drive_1 ;
	r.reg_ECX = 0x0866 ;
	r.reg_EDX = ( DWORD ) m ;
	r.reg_Flags = 1 ;

	DeviceIoControl ( hDevice, VWIN32_DIOC_DOS_IOCTL,
						&r, sizeof ( r ), &r, sizeof ( r ), &cb, 0 ) ;
}

void readabsolutesectors (  HANDLE hDevice, int drive_0, int startsect,
					  int numsect, void * buffer )
{
	DIOC_REGISTERS r ;
	struct DISKIO di ;
	DWORD cb;
	int fResult;

	r.reg_EAX = drive_0 ;    
	r.reg_EBX = ( DWORD ) &di ;
	r.reg_ECX = -1 ;
	r.reg_Flags = 1 ;
	di.startsector = startsect ;
	di.sectorsnum = numsect ;
	di.buff = ( DWORD ) buffer ;
	
	fResult = DeviceIoControl ( hDevice, VWIN32_DIOC_DOS_INT25,
						&r, sizeof ( r ), &r, sizeof ( r ), &cb, 0 ) ;

	if (!fResult || (r.reg_Flags & 0x0001)) return ;
	else {
	Dprintf("return %s\n",buffer);
	}
}

int writeabsolutesectors (  HANDLE hDevice, int drive_0, int startsect,
					  int numsect, void * buffer )
{
	DIOC_REGISTERS r ;
	struct DISKIO di ;
	DWORD cb;
	int fResult;

	r.reg_EAX = drive_0 ;    
	r.reg_EBX = ( DWORD ) &di ;
	r.reg_ECX = -1 ;
	r.reg_Flags = 1 ;
	di.startsector = startsect ;
	di.sectorsnum = numsect ;
	di.buff = ( DWORD ) buffer ;
	
	fResult = DeviceIoControl ( hDevice, VWIN32_DIOC_DOS_INT26,
						&r, sizeof ( r ), &r, sizeof ( r ), &cb, 0 ) ;

	if (!fResult || (r.reg_Flags & 0x0001)) {
		Dprintf("writeabsolutesector(%d) %d,%d failed %d\n",
				drive_0, startsect,numsect,r.reg_Flags);
		return -1;
	} else {
		Dprintf("writeabsolutesector(%d) %d,%d done\n",
			drive_0, startsect,numsect);
	}
}

#define sector_offset 0x18
#define head_offset 0x1A

int readabsolutesectors32 ( HANDLE hDevice, int drive_1, int startsect,
								   int numsect, char *buffer )
{
	DIOC_REGISTERS r ;
	struct DISKIO di ;
	DWORD cb ;
	int fResult;

	Dprintf("readabsolutesectors32(%lx:%d) : sector %d,%d\n", 
		hDevice, drive_1,startsect, numsect);

	r.reg_EAX = 0x7305 ;  
	r.reg_EBX = ( DWORD ) &di ;
	r.reg_ECX = -1 ;  
	r.reg_EDX = drive_1 ;  
	r.reg_ESI = 0 ;       // 0:read, 1:write
	r.reg_Flags = 0x0001 ;       
	di.startsector = startsect ;
	di.sectorsnum = numsect ;
	di.buff = ( DWORD ) buffer ;

	fResult = DeviceIoControl( hDevice, VWIN32_DIOC_DOS_DRIVEINFO,
					  &r, sizeof( r ), &r, sizeof( r ), &cb, 0 ); 
	if (!fResult || (r.reg_Flags & 0x0001)) {
		Dprintf("readabsolutesectors32(%lx:%d) : sector %d,%d failed %i\n", 
		hDevice, drive_1,startsect, numsect, r.reg_Flags);
		return -1;
		}
	else {
    if (startsect == 0) {
	Dprintf("drive %d,%d: sector per track %d, heads %d\n", drive_1,startsect,
		*(WORD *)&buffer[sector_offset], *(WORD *)&buffer[head_offset]);
    }
	return 0;
	}
}

int writeabsolutesectors32 ( HANDLE hDevice, int drive_1, int startsect,
								   int numsect, char *buffer )
{
	DIOC_REGISTERS r ;
	struct DISKIO di ;
	DWORD cb ;
	int fResult;

	Dprintf("writeabsolutesectors32(%lx:%d) : sector %d,%d\n", 
		hDevice, drive_1,startsect, numsect);

	r.reg_EAX = 0x7305 ;  
	r.reg_EBX = ( DWORD ) &di ;
	r.reg_ECX = -1 ;  
	r.reg_EDX = drive_1 ;  
	r.reg_ESI = 1 ;       // 0:read, 1:write
	r.reg_Flags = 0x0001 ;       
	di.startsector = startsect ;
	di.sectorsnum = numsect ;
	di.buff = ( DWORD ) buffer ;

	fResult = DeviceIoControl( hDevice, VWIN32_DIOC_DOS_DRIVEINFO,
					  &r, sizeof( r ), &r, sizeof( r ), &cb, 0 ); 
	if (!fResult || (r.reg_Flags & 0x0001)) {
		Dprintf("writeabsolutesectors32(%lx:%d) : sector %d,%d failed\n", 
			hDevice, drive_1,startsect, numsect);
		return -1;
		}
	else {
    if (startsect == 0) {
	Dprintf("drive %d,%d: sector per track %d, heads %d\n", drive_1,startsect,
		*(WORD *)&buffer[sector_offset], *(WORD *)&buffer[head_offset]);
    }
	return 0;
	}
}

#define bpb_sector_offset 0x18
#define bpb_head_offset   0x1A
#define bpb_hidden_offset 0x1C
#define bpb_sig_offset    0x42
#define bpb_sectcnt_offset 0x13
#define bpb_bigsectcnt_offset 0x20
#define SECTOR_SIZE 512
int load_bootinfo(HANDLE hDevice, int fd, int drive, char *buf)
{
	struct boot32 *p32;
	struct boot *p16;
	DWORD hidden=0, total=0;
	char buffer[512];

	p32 = (struct boot32 *)buf;
	p16 = (struct boot *)buf;

	if (strncmp(p32->bsFileSysType,"FAT32",5) == 0) {
		hidden = *(DWORD *)&buf[bpb_hidden_offset];
		total = *(WORD *)&buf[bpb_sectcnt_offset];
		if (total == 0) total = *(DWORD *)&buf[bpb_bigsectcnt_offset];
	} else if (strncmp(p16->filesystype,"FAT",3) == 0) {
		hidden = *(DWORD *)&buf[bpb_hidden_offset];
		total = *(WORD *)&buf[bpb_sectcnt_offset];
		if (total == 0) total = *(DWORD *)&buf[bpb_bigsectcnt_offset];
	} else if (strncmp(&buffer[3],"NTFS",4) == 0) {
		hidden = *(DWORD *)&buf[bpb_hidden_offset];
		total = *(DWORD *)&buf[bpb_bigsectcnt_offset];
	} else if (drive > 2) {
		Dprintf("invalid boot sector \n");
		return -1;
		}
  	win32_fds[fd].part_start.QuadPart = hidden; 
  	win32_fds[fd].part_start.QuadPart*= SECTOR_SIZE; 
  	win32_fds[fd].part_end.QuadPart = hidden + total;
  	win32_fds[fd].part_end.QuadPart*= SECTOR_SIZE;
	Dprintf("partition info: %ld:%ld\n", hidden, total);
	return 0;
}

int win32_open9x(int fd, int drive)
{
 	HANDLE 	hDevice;   
	DIOC_REGISTERS 	reg;
	BOOL 	fResult;
	DWORD 	byteCnt; 
	char	buffer[512];
	    	
	hDevice = CreateFile("\\\\.\\vwin32", 0, 0, NULL, 0,
		FILE_FLAG_DELETE_ON_CLOSE, NULL);

	if (hDevice != INVALID_HANDLE_VALUE) {
		if (readabsolutesectors32(hDevice, drive, 0, 1, buffer) == 0 &&
			load_bootinfo(hDevice, fd, drive, buffer) == 0) {
			win32_fds[fd].handle = hDevice;
  			win32_fds[fd].drive = drive;
  			win32_fds[fd].logical = 1;
	  		win32_fds[fd].current_pos.QuadPart = 0;	
		} else {
			CloseHandle(hDevice); return -1;
		}
	} else return -1;

	return fd;
}

char readbuf_9xme[32*1024];

int win32_readsector(int fd, char *buf, int sector, int byte_offset, int byte_cnt)
{
	int rvl = 0;
	int t_cnt = byte_offset + byte_cnt;
	int s_cnt;

	s_cnt = t_cnt/SECTOR_SIZE + ((t_cnt%SECTOR_SIZE) ? 1 : 0);
	Dprintf("win32_readsector(%d) %ld:%ld:%ld\n", fd, sector, byte_offset, byte_cnt);
	Dprintf("win32_readsector(%d) to read %ld:%ld:%ld\n", fd, sector, s_cnt, t_cnt);
	if (is9xME()) {
		rvl = readabsolutesectors32(win32_fds[fd].handle, 
								win32_fds[fd].drive, sector, s_cnt, readbuf_9xme);
		if (rvl == 0) {
			Dprintf("readabsolutsector32 return %ld:%ld:%ld\n", sector, byte_offset, byte_cnt);
			memmove(buf,readbuf_9xme+byte_offset,byte_cnt);
			rvl = 1;
		} else rvl = 0;
	} else {
		LARGE_INTEGER offset;

		offset.QuadPart = (ntfs_s64) sector*SECTOR_SIZE + byte_offset;
		if (win32_lseek(fd, offset.QuadPart, SEEK_SET) != -1) {
			if (win32_read(fd, buf, byte_cnt) != -1) rvl = 1;
		} else {
			perror("lseek failed");
		}
	}
	return rvl;
}
 
int win32_writesector(int fd, char *buf, int sector, int byte_offset, int byte_cnt)
{
	int rvl = 0;
	int t_cnt = byte_offset + byte_cnt;
	int s_cnt;

	s_cnt = t_cnt/SECTOR_SIZE + ((t_cnt%SECTOR_SIZE) ? 1 : 0);
	Dprintf("win32_writesector(%d) %ld:%ld:%ld\n", fd, sector, byte_offset, byte_cnt);
	Dprintf("win32_writesector(%d) to write %ld:%ld:%ld\n", fd, sector, s_cnt, t_cnt);
	if (is9xME()) {
		rvl = readabsolutesectors32(win32_fds[fd].handle, 
								win32_fds[fd].drive, sector, s_cnt, readbuf_9xme);
		if (rvl == 0) {
			if (win32_fds[fd].mode) {
				memmove(readbuf_9xme+byte_offset,buf, byte_cnt);
				rvl = writeabsolutesectors32(win32_fds[fd].handle, 
								win32_fds[fd].drive, sector, s_cnt, readbuf_9xme);
			} else {
				Dprintf("writeabsolutsector32 suppressed\n");
				if (memcmp(readbuf_9xme+byte_offset,buf,byte_cnt) != 0) {
					Dprintf("writeabsolutsector32, content different %d\n", sector);
				}
			}
		}
		if (rvl == 0) {
			Dprintf("writeabsolutsector32 return %ld:%ld:%ld\n", sector, byte_offset, byte_cnt);
			rvl = 1;
		} else rvl = 0;
	} else {
		LARGE_INTEGER offset;

		offset.QuadPart = (ntfs_s64) sector*SECTOR_SIZE + byte_offset;
		if (win32_lseek(fd, offset.QuadPart, SEEK_SET) != -1) {
			if (win32_write(fd, buf, byte_cnt) != -1) rvl = 1;
		} else {
			perror("lseek failed");
		}
	}
	return rvl;
}

int win32_read95disk(BYTE *buffer, WORD drive, WORD cylinder, WORD sector, WORD head, int count)
{
 	HANDLE 	hDevice;   
	DIOC_REGISTERS 	reg;
	BOOL 	fResult;
	DWORD 	byteCnt; 
	    	
	hDevice = CreateFile("\\\\.\\vwin32", 0, 0, NULL, 0,
		FILE_FLAG_DELETE_ON_CLOSE, NULL);

	reg.reg_EAX=0x0200 + count;
	reg.reg_EBX=(DWORD)buffer;
	reg.reg_ECX=cylinder*0x100 + sector;
	reg.reg_EDX=head;
	reg.reg_EDX<<=8;
	reg.reg_EDX+=drive;
	reg.reg_Flags=0x0001;

	readabsolutesectors(hDevice, 3, 0, 1 ,buffer);
	readabsolutesectors32(hDevice, 3, 0, 1 ,buffer);
	win32_disk95hasInt13ext(hDevice, drive);
	win32_get95disk_info(hDevice, drive, &cylinder, &sector, &head);
#if 0
	fResult = DeviceIoControl(hDevice, VWIN32_DIOC_DOS_INT13, &reg,
		sizeof(reg), &reg, sizeof(reg), &byteCnt, 0);
#endif	
	CloseHandle(hDevice);

	if(!fResult || (reg.reg_Flags & 0x0001))
		return -1;

	return 0;
}

#define PT_OFFSET	0x1be
int win32_mbr(char *mbr)
{
	struct partitiontable *pt=(struct partitiontable *)&mbr[PT_OFFSET];
	int i;

	for (i = 0; i < 4; i++) {
		printf("%02x %04i %04i(%04i) %02x %04i %04i(%04d) %08ld %08ld\n", 
			(short)pt->boot_flag,(short) pt->head_start,
			(unsigned short)pt->cylinder_sector_start>>6,
			(unsigned short)pt->cylinder_sector_start & 0x3f,
			(short)pt->fs_type,
			(short)pt->head_end,
			(unsigned short)pt->cylinder_sector_end>>6,
			(unsigned short)pt->cylinder_sector_end & 0x3f,
			pt->sect_start,pt->sect_total);
		pt++;
	}
	return 0;
}
