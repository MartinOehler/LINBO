#ifdef WIN32

typedef long long int ntfs_s64;

int win32_open(const char* name, int mode);
ntfs_s64 win32_bias(const int fd);
unsigned long win32_bias_sector(const int fd);
ntfs_s64 win32_filepos(const int fd);
ntfs_s64 win32_lseek(const int fd, ntfs_s64 pos, int mode);
ntfs_s64 win32_read(const int fd, const void* buffer, ntfs_s64 count);
ntfs_s64 win32_write(const int fd, const void* buffer, ntfs_s64 count);
int win32_close(const int fd);
//int win32_read95disk(BYTE *buffer, WORD drive, WORD cylinder, WORD sector, WORD head, int count);
int logical_to_physical(char *name, int *drive, int *part);

#define open win32_open
#define lseek win32_lseek
#define close win32_close
#define read win32_read
#define off_t ntfs_s64

#endif
