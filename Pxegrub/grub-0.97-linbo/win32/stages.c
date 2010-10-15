#include <stdio.h>
#include "stage1.h"

#define SECTOR_SIZE 512
#define STAGE2_ADDRESS 0x8000
#define STAGE2_INSTALLPART  0x8
#define STAGE2_VER_STR_OFFS 0x12
#define BOOTSEC_BPB_OFFSET		0x3
#define BOOTSEC_BPB_LENGTH		0x3B
#define BOOTSEC_SIG_OFFSET		0x1FE
#define BOOTSEC_SIGNATURE		0xAA55

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long LONG;

BYTE stage1_buffer[SECTOR_SIZE];
BYTE stage2_buffer[SECTOR_SIZE];
BYTE stage2_buffer2[SECTOR_SIZE];
LONG stage2_address = STAGE2_ADDRESS;

struct {
  BYTE* major;
  BYTE* minor;
  BYTE* boot_drive;
  BYTE* force_lba;
  WORD* stage2_address;
  LONG* stage2_sector;
  WORD* stage2_segment;
} bootparams = {
  stage1_buffer + STAGE1_VER_MAJ_OFFS,
  stage1_buffer + STAGE1_VER_MAJ_OFFS+1,
  stage1_buffer + STAGE1_BOOT_DRIVE,
  stage1_buffer + STAGE1_FORCE_LBA,
  (WORD*)(stage1_buffer + STAGE1_STAGE2_ADDRESS),
  (LONG*)(stage1_buffer + STAGE1_STAGE2_SECTOR),
  (WORD*)(stage1_buffer + STAGE1_STAGE2_SEGMENT),
};

void 
dump(BYTE* buffer, int rows, int cols)
{
  int pos=0;
  int i,j;
  for(i=0; i<rows; i++) {
	printf("%4.4x   ", i*cols);
	for(j=0; j<cols; j++) {
	  printf("%2.2x ", buffer[pos++]);
	}
	printf("  ");
	pos -= cols;
	for(j=0; j<cols; j++) {
	  if (isprint(buffer[pos])) {
		printf("%c", buffer[pos]);
	  } else {
		printf(".");
	  }
	  pos++;
	}
	printf("\n");
  }
}

int 
stage1(char* filename, LONG sector) 
{
  int rvl = 0;
  int b_drive;
  int lba;

  printf("stage1(%d)\n", sector);
  FILE* fp=fopen(filename,"r+b");
  if (fp == NULL) {
	perror("open failed");
	rvl--;
	return rvl;
  }
  fseek(fp,0,SEEK_SET);
  fread(stage1_buffer,SECTOR_SIZE,1,fp);

  if (*((short *)(stage1_buffer + STAGE1_VER_MAJ_OFFS)) != COMPAT_VERSION
     || (*((unsigned short *) (stage1_buffer + BOOTSEC_SIG_OFFSET))
	 != BOOTSEC_SIGNATURE)) {
	 printf("bad stage1 signature\n");
	 return 0;
  }

  if (sector != 0) {
	*bootparams.stage2_sector = sector;
	*bootparams.stage2_address = stage2_address;

	fseek(fp,0,SEEK_SET);
	if (fwrite(stage1_buffer,SECTOR_SIZE,1,fp) < 0) {
	  rvl--;
	}
  }

  //dump(stage1_buffer,32,16);
  printf(" version=%d.%d\n", *bootparams.major, *bootparams.minor);
  printf(" boot_drive=%x\n", b_drive=*bootparams.boot_drive);
  printf(" force_lba=%d\n", lba = *bootparams.force_lba);
  printf(" stage2_address=0x%x\n", *bootparams.stage2_address);
  printf(" stage2_sector=%d\n", *bootparams.stage2_sector);
  printf(" stage2_segment=0x%x\n", *bootparams.stage2_segment);

  fclose(fp);
  return rvl;
}

#define GET(x) {ptr-=sizeof(x);memcpy(&x,ptr,sizeof(x));}
#define PUT(x) {ptr-=sizeof(x);memcpy(ptr,&x,sizeof(x));}
#define BACK(x) {ptr+=sizeof(x);}

int 
stage2(char* filename, char* blocklist, char* menu) {
  int rvl = 0;
  int real_len;
  int len_written = 0;
  int src_drive,src_part=-1;

  printf("stage2(%s)\n", blocklist);
  FILE* fp=fopen(filename,"r+b");
  if (fp == NULL) {
	perror("open failed\n");
	rvl--;
	return rvl;
  }

  logical_to_physical(filename,&src_drive,&src_part);
  fseek(fp, 0, SEEK_SET);
  fread(stage2_buffer,SECTOR_SIZE,1,fp);
  fseek(fp, 0, SEEK_END);
  real_len = ftell(fp);
#if 0
  printf("first 8 byte of stage2 %lx/%lx\n", 
			*((unsigned long*)stage2_buffer),
			*((unsigned long*)stage2_buffer+4));
#endif

  BYTE* ptr;
  WORD count;
  LONG offset;
  WORD addr;

  if (blocklist) {
	int numread;
	ptr = &(stage2_buffer[SECTOR_SIZE]);

	addr = (stage2_address + SECTOR_SIZE) >> 4;

	if (*blocklist=='(') {
	  blocklist = strchr(blocklist,')');
	  if (blocklist) {
		blocklist++;
	  }
	}
	int first = 1;
	while(blocklist && *blocklist) {
	  if (sscanf(blocklist, "%d+%hd", &offset, &count)==2) {
		len_written += count*SECTOR_SIZE;
		if (first) {
		  first = 0;
		  // first sector was already loaded
		  offset++;
		  count--;
		}
		// printf(" offset=%d, count=%d, addr=0x%x\n", offset, count, addr);

		if (((*(WORD*)(ptr-4)) & 0x8000) != 0) {
		  printf("IMAGE TOO LARGE! p=0x%x\n",*(WORD*)(ptr-4));
		  break;
		}

		if (count != 0) {
		  if (len_written > real_len) count-= (len_written-real_len)/SECTOR_SIZE;
		  PUT(addr);
		  PUT(count);
		  PUT(offset);
		}
		addr += (count*SECTOR_SIZE)>>4;

	  } else {
		break;
	  }
	  blocklist = strchr(blocklist,',');
	  if (blocklist) {
		blocklist++;
	  }
	}
	while (((*(WORD*)(ptr)) & 0x8000) == 0) {
	  count = 0; offset = 0; addr = 0;
	  PUT(addr);
	  PUT(count);
	  PUT(offset);
	  break;
	}
	
	fseek(fp, 0, SEEK_SET);
#if 0
	printf("pointer %x %d %x\n", 
		*(unsigned long *)(&stage2_buffer[SECTOR_SIZE -8]), 
		*(unsigned short *)(&stage2_buffer[SECTOR_SIZE -4]), 
		*(unsigned short *)(&stage2_buffer[SECTOR_SIZE -2]));
	printf("pointer %x %d %x\n", 
		*(unsigned long *)(&stage2_buffer[SECTOR_SIZE -16]), 
		*(unsigned short *)(&stage2_buffer[SECTOR_SIZE -12]), 
		*(unsigned short *)(&stage2_buffer[SECTOR_SIZE -10]));
#endif
	if (! fwrite(stage2_buffer,SECTOR_SIZE,1,fp)) {
	  rvl--;
	}
  }

  if (menu) {
	  fseek(fp, SECTOR_SIZE, SEEK_SET);
	  fread(stage2_buffer2,SECTOR_SIZE,1,fp);
	  if (menu[0] != '(') {
	  	if (src_part != -1) {
       		*((long *) (stage2_buffer2 + STAGE2_INSTALLPART))
				= (src_part << 16)+0xffff;
	  	} else {
       		*((long *) (stage2_buffer2 + STAGE2_INSTALLPART))
				= 0xffff;
	  	}
	  }
	  ptr = &(stage2_buffer2[STAGE2_VER_STR_OFFS]);
	  while (*ptr++);
	  memcpy(ptr,menu,strlen(menu));
	  ptr+=strlen(menu);
	  *ptr='\0';
	  fseek(fp, SECTOR_SIZE, SEEK_SET);
	  fwrite(stage2_buffer2,SECTOR_SIZE,1,fp);
	  printf("menu %s\n", menu);
  }
#if 0
  fseek(fp, 0, SEEK_SET);
  fread(stage2_buffer,SECTOR_SIZE,1,fp);
  ptr = &(stage2_buffer[SECTOR_SIZE]);

  do {
	GET(addr);
	GET(count);
	GET(offset);
	if ((count & 0x8000) != 0) {
	  break;
	}
	if (addr == 0) {
	  break;
	}
	printf(" %d sectors to load from %d at 0x%x\n", count, offset, addr);
  } while (addr != 0);
#endif

  //dump(stage2_buffer, 32, 16);

  return rvl;
}

char file_buf[32*1024];
int verify(char *device, char *stage1, char *stage2)
{
	FILE *fp;
	unsigned char stage1_buffer[512];
	unsigned char stage2_buffer[512];
	unsigned char buffer1[512];
	unsigned char buffer2[512];
	unsigned int first_sector;
	int fd;
	int sector_offset;
	int drive, part;
	int src_drive, src_part;
	int numparams;
	char device_name[16];
	int stage2_len;
	int i=0;
	int match = 0;
	int boot_drive;
	int cnt;	
	int test_me_write = 0;

	if (sscanf(device,"(fd%d)", &drive) > 0) return 1;
	if (device[1]==':' && toupper(device[0]) < 'C') return 1;

	if (!is9xME()) {
    	numparams = sscanf(device,"(hd%d,%d)",&drive,&part);
		if (numparams == 0 && 
			!logical_to_physical(device, &drive, &part)) return 0;
		if (!logical_to_physical(stage2, &src_drive, &src_part)) return 0;
	} else {
    	numparams = sscanf(device,"(hd%d,%d)",&drive,&part);
		if (numparams) {
		} else if (toupper(device[0]) != toupper(stage2[0]) ||	
					toupper(device[0]) != 'C') {
				printf("%c %c %c\n", 
						toupper(device[0]),
						toupper(stage2[0]));
				return 0;
				}
	}

	if (device[0] != '(') sprintf(device_name,"(hd%d,%d)",drive,part);
	else strcpy(device_name,device);

	if ((fp = fopen(stage1,"rb"))== NULL) return 0;
  	fseek(fp, 0, SEEK_SET);
	if (!(cnt = fread(stage1_buffer,512,1, fp))) {
		printf("stage1 file is bad %d\n",cnt);
		fclose(fp); return 0;
	}
	fclose(fp);
	if (*((short *)(stage1_buffer + STAGE1_VER_MAJ_OFFS)) != COMPAT_VERSION
      	|| (*((unsigned short *) (stage1_buffer + BOOTSEC_SIG_OFFSET))
	  	!= BOOTSEC_SIGNATURE)) {
		printf("bad stage1 signature\n");
		return 0;
		}
	first_sector = *(unsigned long *)(stage1_buffer+STAGE1_STAGE2_SECTOR);
	boot_drive = *(stage1_buffer + STAGE1_BOOT_DRIVE);
	if (boot_drive == 0xff && 
		src_drive != drive) {
		printf("installed drive != stage2\n");
		return 0;
		}
	if (boot_drive != 0xff && (boot_drive - 0x80) != src_drive) {
		printf("stage1 boot drive setting != stage2\n");
		return 0; 
		}
	if ((fp = fopen(stage2,"rb"))== NULL) {
		printf("stage2 file bad\n");
		return 0;
		}
	if ((fd = win32_open(device_name,test_me_write & is9xME())) <= 0) {
		printf("installed device non-verifable\n");
		fclose(fp);
		return 0;
		}
	sector_offset = win32_bias_sector(fd);
  	fseek(fp, 0, SEEK_END);
  	stage2_len = ftell(fp);
  	fseek(fp, 0, SEEK_SET);
	if (win32_readsector(fd,stage2_buffer,first_sector-sector_offset,0,512) &&
  		fread(buffer1,512,1,fp) > 0 && memcmp(stage2_buffer,buffer1,512) == 0) {
		int i = 1;
		int j = 0;
		long *ps = (long *)&stage2_buffer[512-8];
		short *pc = (short *)&stage2_buffer[512-8+4];
		int c_sector = *ps;
		int cnt = *pc;

		while (i < (stage2_len - 512)/512 + ((stage2_len - 512)%512 ? 1 : 0)) {
  			fseek(fp, 512*i, SEEK_SET);
			fread(buffer1,512,1,fp);
			win32_readsector(fd,buffer2,c_sector-sector_offset,0,512);
			if (memcmp(buffer1,buffer2,512) != 0) break;
			else if (is9xME() && test_me_write) {
				win32_writesector(fd,buffer2,c_sector-sector_offset,0,512);
			}
			i++,cnt--,c_sector++;
			if (cnt == 0) {
				ps-=2; pc-=4;
				cnt = *pc, c_sector = *ps;
			}
			if (cnt == 0) break;
		}
		if (stage2_len - i*512 < 512) match = 1;
		else {
			printf("verification failed at sector %d\n", i);
		}
	}
	fclose(fp);
	win32_close(fd);
	return match;
}

int install(char *device, char *stage1, char *stage2)
{
  int floppy = 0;
  int numparams;
  int drive = 0;
  int part = 0;
  int fd;
  char device_name[8];
  int open_mode = 1;
  int harddisk = 0;

  if (!verify(device,stage1,stage2)) {
	printf("broken stage1/stage2 file in order to install to %s\n", device);
	return -1;
	}

  if (device[1] == ':') {
	drive = toupper(device[0]) - 'A'+ 1;
	floppy = drive < 3;
	device_name[0] = device[0], device_name[1] = ':', device_name[2] = '\0';
  } else {
    numparams = sscanf(device,"(hd%d,%d)",&drive,&part);
    if (numparams == 0) {
	  numparams = sscanf(device,"(fd%d)",&drive);
	  if (numparams != 0) {
		floppy = 1, drive+=1;	
	    device_name[0] = 'A' + drive - 1;
	    device_name[1] = ':';
	    device_name[2] = '\0';
 	    }
	}
    else {
	  if (numparams == 1) {
		strcpy(device_name,device);
		harddisk = 1;
	  } else {
	    device_name[0] = 'C' + part;
	    device_name[1] = ':';
	    device_name[2] = '\0';
	  } 	
    }
  }

  if (!is9xME() && !floppy && open_mode == 1) {
	printf("In order to boot GRUB/Linux on NT/W2K/XP\n");
	printf("Please add: %s=\"GRUB/Linux\" to c:\\boot.ini\n", stage1);
	return 0;
  }

  if((fd = win32_open(device_name,open_mode)) > 0) {
    FILE* fp;
	char  saved_bootsector[128];
    char  buf[512];
    char  stage1_buffer[512];
	char  *pslash;
	int	  was_grub = 0;
	int	  raw_floppy = 0;	

    if (win32_readsector(fd,buf,0,0, 512) > 0) {
  		/* Check for the version and the signature of Stage 1.  */
		if (*((short *)(buf + STAGE1_VER_MAJ_OFFS)) != COMPAT_VERSION
      			|| (*((unsigned short *) (buf + BOOTSEC_SIG_OFFSET))
	  			!= BOOTSEC_SIGNATURE)) {
		} else {
			was_grub = 1;
			printf("Boot sector on %s was GRUB\n", device_name);
			}

    	if (fp=fopen(stage1,"r+b")) {
	
  			fseek(fp,0,SEEK_SET);
  			fread(stage1_buffer,512,1,fp);
  			/* Copy the possible DOS BPB, 59 bytes at byte offset 3.  */
  			memmove (stage1_buffer + BOOTSEC_BPB_OFFSET,
					 buf + BOOTSEC_BPB_OFFSET,
					BOOTSEC_BPB_LENGTH);

			if (!floppy) {
  				/* copy the possible MBR/extended part table.  */
    			memmove (stage1_buffer + STAGE1_WINDOWS_NT_MAGIC,
		  				buf + STAGE1_WINDOWS_NT_MAGIC,
		  				STAGE1_PARTEND - STAGE1_WINDOWS_NT_MAGIC);
			} else {
				if ((unsigned char)stage1_buffer[STAGE1_BOOT_DRIVE] == 0xff) {
					raw_floppy = 1;
					*(unsigned long *)&stage1_buffer[STAGE1_STAGE2_SECTOR] = 1;
					}
			}

  			/* Check for the version and the signature of Stage 1.  */
  			if (*((short *)(stage1_buffer + STAGE1_VER_MAJ_OFFS)) != COMPAT_VERSION
      			|| (*((unsigned short *) (stage1_buffer + BOOTSEC_SIG_OFFSET))
	  			!= BOOTSEC_SIGNATURE))
    		{
      			goto fail;
    		}
			if (!floppy && !was_grub) { //save the old boot record/MBR
				FILE *pold;
				sprintf(saved_bootsector,"%c:/boot/%c_boot.old",
						device_name[0],device_name[0]);
				pold = fopen(saved_bootsector,"wb+");
				if (pold) fwrite(buf,512,1,pold), fclose(pold);
				else {
					sprintf(saved_bootsector,"%c:/%c_boot.old",
							device_name[0],device_name[0]);
					pold = fopen(saved_bootsector,"wb+");
					if (pold) fwrite(buf,512,1,pold), fclose(pold);
				}
			}
    		if (win32_writesector(fd,stage1_buffer,0,0, 512) > 0) {
				printf("Stage1 written to %s successfully\n",device_name);
			} else goto fail;
			if (floppy) {
				FILE *fp2 = fopen(stage2,"rb");
				int real_len;
				int written = 0;
				int i = 1;
				int stage2_sectors;
				if (fp2) {
  					fseek(fp2, 0, SEEK_END);
  					real_len = ftell(fp2);
					stage2_sectors = (real_len/512) + (real_len%512 ? 1:0);
  					fseek(fp2, 0, SEEK_SET);
					while (written < real_len) {
						int to_read = real_len - written;
						if (to_read > sizeof(file_buf)) to_read = sizeof(file_buf);

						if (fread(file_buf,to_read,1,fp2) > 0) {
							if (i==1 && raw_floppy) {
								*(unsigned long *)&file_buf[504] = 2;
								*(unsigned short *)&file_buf[504+4] = stage2_sectors-1;
								*(unsigned short *)&file_buf[504-4] = 0;
							}
							if (win32_writesector(fd,file_buf,i,0,to_read) > 0) {
								printf("Writing stage2 sector %d - %d\n", i, i+to_read/512-1+(to_read%512 ? 1: 0));
							} else {
								printf("Writing stage2 sector %d - %d, failed \n", i, i+to_read/512+(to_read%512 ? 1: 0));
								break;	
							}
						} else {
							break;
						}
						written+=sizeof(file_buf);
						i+=sizeof(file_buf)/512;
					}
					if (written > real_len)
						printf("Stage2 written to %s successfully\n",
							device_name);
					fclose(fp2);
				}
			}
		}
fail:
			fclose(fp);
	}
    win32_close(fd);
  }
}

