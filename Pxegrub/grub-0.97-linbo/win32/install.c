#include <stdlib.h>
#include <getopt.h>
#include <win32.h>

char* ntfs_blocklist(char* device, char* path);
char* fat_blocklist(char* device, char* path);
int stage1(char* filename, long sector);
int stage2(char* filename, char* blocklist, char* menu);

char* opts = "d:1:2:m:i:h";
char* usage = 
"w32grub -d device -1 stage1 -2 stage2 -m menu\n"
"       -d (hd0,0)      : partition where the files are\n"
"       -1 C:/boot/stage1 : boot sector\n"
"       -2 C:/boot/stage2 : secondary boot loader\n"
"       -m /boot/grub/menu.lst : grub boot menu\n"
"       -i A:                  : device to install grub\n";


int 
main(int argc, char* argv[]) {
  char* device = "(hd0,0)";
  char* image1 = "C:/boot/stage1";
  char* image2 = "C:/boot/stage2";
  char* menu = "/boot/grub/menu.lst";
  char* install_to = "";
  int fd;
  int drive, part=-1;
  int device_given = 0;
  char stage2_device[16];
  
  int c;
  do {
	c = getopt(argc, argv, opts);
	switch (c) {
	case 'd': 
	  device_given = 1;
	  device = optarg;
	  break;
	case '1':
	  image1 = optarg;
	  break;
	case '2':
	  image2 = optarg;
	  break;
	case 'i':
	  install_to = optarg;
	  printf("install to %s\n", install_to);
	  break;
	case 'm':
	  menu = optarg;
	  printf("menu file is %s\n", menu);
	  break;
	default:
	  printf("unknown option '%c'\n", (char)c);
	case 'h':
	  printf(usage);
	  return 0;
	  break;
	case -1:
	  break;
	}
  } while (c>0);

  if (!device_given && logical_to_physical(image2, &drive, &part)) {
	sprintf(stage2_device,"(hd%d,%d)", drive, part);
	device = stage2_device;
  }

  char* blocklist = ntfs_blocklist(device, image2);
  if (blocklist == NULL) {
	printf("blocklist failed\n");
	return 0;
  } else if (strlen(blocklist)==0) {
    blocklist = fat_blocklist(device, image2);
    if (blocklist == NULL || strlen(blocklist) == 0) return 0;
  }

  int sector = 0;
  if (sscanf(blocklist, "%d", &sector)==0) {
	printf("cannot parse blocklist\n");
	sector = 0;
	return 0;
  }

  if ( stage1(image1, sector) < 0 ) {
	printf("configuring stage1 failed\n");
	return 0;
  }

  if ((menu != NULL) && (*menu != '(')){
	char* tmp = malloc(strlen(device) + strlen(menu));
	sprintf(tmp,"%s%s", device, menu);
	//sprintf(tmp,"%s%s", "", menu);
	menu = tmp;
  }

  if (stage2(image2, blocklist, menu) < 0) {
	printf("configuring stage2 failed\n");
	return 0;
  }
  if (strlen(install_to)) install(install_to, image1, image2);
  return 1;
}

