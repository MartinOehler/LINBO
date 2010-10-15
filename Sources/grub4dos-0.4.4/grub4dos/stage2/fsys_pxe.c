/*
 *  PXE file system for GRUB
 *
 *  Copyright (C) 2007 Bean (bean123@126.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifdef FSYS_PXE

#include "shared.h"
#include "filesys.h"
#include "pxe.h"

#include "etherboot.h"

#ifdef GRUB_UTIL

int pxe_mount(void) { return 0; }
unsigned long pxe_read (char *buf, unsigned long len) { return -1; }
int pxe_dir (char *dirname) { return 0; }
void pxe_close (void) {}

#else

#ifndef TFTP_PORT
#define TFTP_PORT	69
#endif

#define PXE_MIN_BLKSIZE	512
#define PXE_MAX_BLKSIZE	1432

#define DOT_SIZE	1048576

#define PXE_BUF		FSYS_BUF
#define PXE_BUFLEN	FSYS_BUFLEN

//#define PXE_DEBUG	1

unsigned long pxe_entry,pxe_blksize=PXE_MAX_BLKSIZE;
unsigned short pxe_basemem,pxe_freemem;

static IP4 pxe_yip,pxe_sip,pxe_gip;
static UINT8 pxe_mac_len,pxe_mac_type,pxe_tftp_opened;
static MAC_ADDR pxe_mac;
static unsigned long pxe_saved_pos,pxe_cur_ofs,pxe_read_ofs,pxe_keep;

static PXENV_TFTP_OPEN_t pxe_tftp_open;
static char *pxe_tftp_name;

extern unsigned long ROM_int15;
extern struct drive_map_slot bios_drive_map[DRIVE_MAP_SIZE + 1];

static char* pxe_outhex(char* pc,unsigned char c)
{
  int i;

  pc+=2;
  for (i=1;i<=2;i++)
    {
      unsigned char t;

      t=c & 0xF;
      if (t>=10)
        t+='A'-10;
      else
        t+='0';
      *(pc-i)=t;
      c=c>>4;
    }
  return pc;
}

void pxe_detect(void)
{
  PXENV_GET_CACHED_INFO_t get_cached_info;
  BOOTPLAYER *bp;
  unsigned long tmp;
  char *pc;
  int i,ret;

  if (! pxe_scan())
    return;

  pxe_basemem=*((unsigned short*)0x413);

  get_cached_info.PacketType=PXENV_PACKET_TYPE_DHCP_ACK;
  get_cached_info.Buffer=get_cached_info.BufferSize=0;
  pxe_call(PXENV_GET_CACHED_INFO,&get_cached_info);
  if (get_cached_info.Status)
    return;
  bp=LINEAR(get_cached_info.Buffer);

  pxe_yip=bp->yip;
  pxe_sip=bp->sip;
  pxe_gip=bp->gip;

  pxe_mac_type=bp->Hardware;
  pxe_mac_len=bp->Hardlen;
  grub_memmove(&pxe_mac,&bp->CAddr,pxe_mac_len);

  get_cached_info.PacketType=PXENV_PACKET_TYPE_CACHED_REPLY;
  get_cached_info.Buffer=get_cached_info.BufferSize=0;
  pxe_call(PXENV_GET_CACHED_INFO,&get_cached_info);

  if (get_cached_info.Status)
    return;
  bp=LINEAR(get_cached_info.Buffer);

  if (bp->bootfile[0])
    {
      int n;

      n=grub_strlen((char*)bp->bootfile)-1;
      grub_strcpy((char*)&pxe_tftp_open.FileName,(char*)bp->bootfile);
      while ((n>=0) && (pxe_tftp_open.FileName[n]!='/'))
        n--;
      if (n<0)
        n=0;
      pxe_tftp_name=(char*)&pxe_tftp_open.FileName[n];
    }
  else
    pxe_tftp_name=(char*)&pxe_tftp_open.FileName[0];

  pxe_tftp_opened=0;

  ret=0;
  grub_strcpy(pxe_tftp_name,"/menu.lst/");

  pc=pxe_tftp_name+10;
  pc=pxe_outhex(pc,pxe_mac_type);
  for (i=0;i<pxe_mac_len;i++)
    {
      *(pc++)='-';
      pc=pxe_outhex(pc,pxe_mac[i]);
    }
  *pc=0;
  grub_printf("\n%s\n",pxe_tftp_open.FileName);
  if (pxe_dir(pxe_tftp_name))
    {
      ret=1;
      goto done;
    }

  pc=pxe_tftp_name+10;
  tmp=pxe_yip;
  for (i=0;i<4;i++)
    {
      pc=pxe_outhex(pc,tmp & 0xFF);
      tmp >>=8;
    }
  *pc=0;
  do
    {
      grub_printf("%s\n",pxe_tftp_open.FileName);
      if (pxe_dir(pxe_tftp_name))
        {
          ret=1;
          goto done;
        }
      *(--pc)=0;
    } while (pc>pxe_tftp_name+10);
  grub_strcpy(pc,"default");
  grub_printf("%s\n",pxe_tftp_open.FileName);
  ret=pxe_dir(pxe_tftp_name);

done:

  if (ret)
    {
      unsigned long nr;

      nr=4096-1;
      if (nr>filemax)
        nr=filemax;
      nr=pxe_read((char*)0x800,nr);
      if (nr!=PXE_ERR_LEN)
        {
          *(char*)(0x800+nr)=0;

          if (preset_menu!=(char*)0x800)
            preset_menu=(char*)0x800;

          if (nr<filemax)
            {
              grub_printf("Boot menu truncated\n");
              pxe_read(NULL,filemax-nr);
            }
        }
      pxe_close();
    }
  //getkey();
}

#if PXE_TFTP_MODE

static int pxe_reopen(void)
{
  pxe_close();

  pxe_call(PXENV_TFTP_OPEN, &pxe_tftp_open);
  if (pxe_tftp_open.Status)
    return 0;
  pxe_blksize=pxe_tftp_open.PacketSize;

  pxe_tftp_opened=1;
  pxe_saved_pos=pxe_cur_ofs=pxe_read_ofs=0;

  return 1;
}

static int pxe_open(char* name)
{
  PXENV_TFTP_GET_FSIZE_t *tftp_get_fsize;

  tftp_get_fsize=(void*)&pxe_tftp_open;
  tftp_get_fsize->ServerIPAddress=pxe_sip;
  tftp_get_fsize->GatewayIPAddress=pxe_gip;

  if (name!=pxe_tftp_name)
    grub_strcpy(pxe_tftp_name,name);

  pxe_call(PXENV_TFTP_GET_FSIZE,tftp_get_fsize);

  if (tftp_get_fsize->Status)
    return 0;

  filemax=tftp_get_fsize->FileSize;

  pxe_tftp_open.TFTPPort=htons(TFTP_PORT);
  pxe_tftp_open.PacketSize=pxe_blksize;

  return pxe_reopen();
}

void pxe_close(void)
{
  if (pxe_tftp_opened)
    {
      PXENV_TFTP_CLOSE_t tftp_close;

      pxe_call(PXENV_TFTP_CLOSE,&tftp_close);
      pxe_tftp_opened=0;
    }
}

#if PXE_FAST_READ

/* Read num packets , BUF must be segment aligned*/
static unsigned long pxe_read_blk(unsigned long buf,int num)
{
  PXENV_TFTP_READ_t tftp_read;
  unsigned long ofs;

  tftp_read.Buffer=SEGOFS(buf);
  ofs=tftp_read.Buffer & 0xFFFF;
  pxe_fast_read(&tftp_read, num);
  return (tftp_read.Status)?PXE_ERR_LEN:((tftp_read.Buffer & 0xFFFF)-ofs);
}

#else

static unsigned long pxe_read_blk(unsigned long buf,int num)
{
  PXENV_TFTP_READ_t tftp_read;
  unsigned long ofs;

  tftp_read.Buffer=SEGOFS(buf);
  ofs=tftp_read.Buffer & 0xFFFF;
  while (num>0)
    {
      pxe_call(PXENV_TFTP_READ, &tftp_read);
      if (tftp_read.Status)
        return PXE_ERR_LEN;
      tftp_read.Buffer+=tftp_read.BufferSize;
      if (tftp_read.BufferSize<pxe_blksize)
        break;
      num--;
    }
  return (tftp_read.Buffer & 0xFFFF) - ofs;
}

#endif

#else
#endif

static unsigned long pxe_read_len (char* buf, unsigned long len)
{
  unsigned long old_ofs,sz;

  if (len==0)
    return 0;

  sz=0;
  old_ofs=pxe_cur_ofs;
  pxe_cur_ofs+=len;
  if (pxe_cur_ofs>pxe_read_ofs)
    {
      unsigned long nb,nr;
      long nb_del,nb_pos;

      sz=(pxe_read_ofs-old_ofs);
      if ((buf) && (sz))
        {
          grub_memmove(buf,(char*)(PXE_BUF+old_ofs),sz);
          buf+=sz;
        }
      pxe_cur_ofs-=pxe_read_ofs;
      nb=pxe_cur_ofs / pxe_blksize;
      nb_del=DOT_SIZE / pxe_blksize;
      if (nb_del>nb)
        {
          nb_del=0;
          nb_pos=-1;
        }
      else
        nb_pos=nb-nb_del;
      pxe_cur_ofs-=pxe_blksize*nb;
      if (pxe_read_ofs+pxe_blksize>PXE_BUFLEN)
        pxe_read_ofs=0;
      while (nb>0)
        {
          unsigned long nn;

          nn=(PXE_BUFLEN - pxe_read_ofs) / pxe_blksize;
          if (nn>nb)
            nn=nb;
          nr=pxe_read_blk(PXE_BUF+pxe_read_ofs,nn);
          if (nr==PXE_ERR_LEN)
            return nr;
          sz+=nr;
          if (buf)
            {
              grub_memmove(buf,(char*)(PXE_BUF+pxe_read_ofs),nr);
              buf+=nr;
            }
          if (nr<nn*pxe_blksize)
            {
              pxe_read_ofs+=nr;
              pxe_cur_ofs=pxe_read_ofs;
              return sz;
            }
          nb-=nn;
          if (nb)
            pxe_read_ofs=0;
          else
            pxe_read_ofs+=nr;
          if ((long)nb<=nb_pos)
            {
              grub_putchar('.');
              nb_pos-=nb_del;
            }
        }

      if (nb_del)
        {
          grub_putchar('\r');
          grub_putchar('\n');
        }

      if (pxe_cur_ofs)
        {
          if (pxe_read_ofs+pxe_blksize>PXE_BUFLEN)
            pxe_read_ofs=0;

          nr=pxe_read_blk(PXE_BUF+pxe_read_ofs,1);
          if (nr==PXE_ERR_LEN)
            return nr;
          if (pxe_cur_ofs>nr)
            pxe_cur_ofs=nr;
          sz+=pxe_cur_ofs;
          if (buf)
            grub_memmove(buf,(char*)(PXE_BUF+pxe_read_ofs),pxe_cur_ofs);
          pxe_cur_ofs+=pxe_read_ofs;
          pxe_read_ofs+=nr;
        }
      else
        pxe_cur_ofs=pxe_read_ofs;
    }
  else
    {
      sz+=len;
      if (buf)
        grub_memmove(buf,(char*)PXE_BUF+old_ofs,len);
    }
  return sz;
}

/* Mount the network drive. If the drive is ready, return 1, otherwise
   return 0. */
int pxe_mount(void)
{
  if (current_drive != PXE_DRIVE)
    return 0;

  return 1;
}

/* Read up to SIZE bytes, returned in ADDR.  */
unsigned long pxe_read (char *buf, unsigned long len)
{
  unsigned long nr;

  if (! pxe_tftp_opened)
    return PXE_ERR_LEN;

  if (pxe_saved_pos!=filepos)
    {
      if ((filepos<pxe_saved_pos) && (filepos+pxe_cur_ofs>=pxe_saved_pos))
        pxe_cur_ofs-=pxe_saved_pos-filepos;
      else
        {
          if (pxe_saved_pos>filepos)
            {
              //grub_printf("reopen\n");
              if (! pxe_reopen())
                return PXE_ERR_LEN;
            }

          nr=pxe_read_len(NULL, filepos-pxe_saved_pos);
          if ((nr==PXE_ERR_LEN) || (pxe_saved_pos+nr!=filepos))
            return PXE_ERR_LEN;
        }
      pxe_saved_pos=filepos;
    }
  nr=pxe_read_len(buf,len);
  if (nr!=PXE_ERR_LEN)
    {
      filepos+=nr;
      pxe_saved_pos=filepos;
    }
  return nr;
}

/* Check if the file DIRNAME really exists. Get the size and save it in
   FILEMAX. return 1 if succeed, 0 if fail.  */
int pxe_dir (char *dirname)
{
  int ret,ch;

  if (print_possibilities)
    return 1;

  pxe_close();

  ret=1;
  ch=nul_terminate(dirname);

  if (! pxe_open(dirname))
    {
      errnum=ERR_FILE_NOT_FOUND;
      ret=0;
    }

  dirname[grub_strlen(dirname)]=ch;
  return ret;
}

void pxe_unload(void)
{
  PXENV_UNLOAD_STACK_t unload;
  unsigned char code[]={PXENV_UNDI_SHUTDOWN,PXENV_UNLOAD_STACK,PXENV_STOP_UNDI,0};
  int i,h;

  if (! pxe_entry)
    return;

  pxe_close();

  if (pxe_keep)
    return;

  h=unset_int13_handler(1);
  if (! h)
    unset_int13_handler(0);

  i=0;
  while (code[i])
    {
      grub_memset(&unload,0,sizeof(unload));
      pxe_call(code[i],&unload);
      if (unload.Status)
        {
          grub_printf("PXE unload fails: %d\n",unload.Status);
          goto quit;
        }
      i++;
    }
  if (*((unsigned short*)0x413)==pxe_basemem)
    *((unsigned short*)0x413)=pxe_freemem;
  pxe_entry=0;
  ROM_int15=*((unsigned long*)0x54);
  grub_printf("PXE stack unloaded\n");
quit:
  if (! h)
    set_int13_handler(bios_drive_map);
}

static void print_ip(IP4 ip)
{
  int i;

  for (i=0;i<3;i++)
    {
      grub_printf("%d.",ip & 0xFF);
      ip>>=8;
    }
  grub_printf("%d",ip);
}

int pxe_func(char* arg,int flags)
{
  if (! pxe_entry)
    {
      grub_printf("No PXE stack\n");
      errnum = ERR_BAD_ARGUMENT;
      return 1;
    }
  if (*arg==0)
    {
      char buf[4],*pc;
      int i;

      pxe_tftp_name[0]='/';
      pxe_tftp_name[1]=0;
      grub_printf("blksize : %d\n",pxe_blksize);
      grub_printf("basedir : %s\n",pxe_tftp_open.FileName);
      grub_printf("client ip  : ");
      print_ip(pxe_yip);
      grub_printf("\nserver ip  : ");
      print_ip(pxe_sip);
      grub_printf("\ngateway ip : ");
      print_ip(pxe_gip);
      grub_printf("\nmac : ");
      for (i=0;i<pxe_mac_len;i++)
        {
          pc=buf;
          pc=pxe_outhex(pc,pxe_mac[i]);
          *pc=0;
          grub_printf("%s%c",buf,(i==pxe_mac_len-1)?'\n':'-');
        }
    }
  else if (grub_memcmp(arg, "blksize", sizeof("blksize")-1)==0)
    {
      int val;

      arg=skip_to(0, arg);
      if (! safe_parse_maxint(&arg, &val))
        return 1;
      if (val>PXE_MAX_BLKSIZE)
        val=PXE_MAX_BLKSIZE;
      if (val<PXE_MIN_BLKSIZE)
        val=PXE_MIN_BLKSIZE;
      pxe_blksize=val;
    }
  else if (grub_memcmp(arg, "basedir", sizeof("basedir")-1)==0)
    {
      int n;

      arg=skip_to(0, arg);
      if (*arg==0)
        {
          grub_printf("No pathname\n");
          errnum = ERR_BAD_ARGUMENT;
          return 1;
        }
      if (*arg!='/')
        {
          grub_printf("Base directory must start with /\n");
          errnum = ERR_BAD_ARGUMENT;
          return 1;
        }
      n=grub_strlen(arg);
      if (n>sizeof(pxe_tftp_open.FileName)-8)
        {
          grub_printf("Path too long\n");
          errnum = ERR_BAD_ARGUMENT;
          return 1;
        }
      grub_strcpy((char*)pxe_tftp_open.FileName,arg);
      n--;
      while ((n>=0) && (pxe_tftp_open.FileName[n]=='/'))
        n--;
      pxe_tftp_name=(char*)&pxe_tftp_open.FileName[n+1];
    }
  else if (grub_memcmp(arg, "keep", sizeof("keep")-1)==0)
    pxe_keep=1;
  else if (grub_memcmp(arg, "unload", sizeof("unload")-1)==0)
    {
      pxe_keep=0;
      pxe_unload();
    }
#ifdef PXE_DEBUG
  else if (grub_memcmp(arg, "dump", sizeof("dump")-1)==0)
    {
      PXENV_GET_CACHED_INFO_t get_cached_info;
      BOOTPLAYER *bp;
      int val;

      arg=skip_to(0, arg);
      if (! safe_parse_maxint(&arg, &val))
        return 1;
      if ((val<1) || (val>3))
        {
          grub_printf("Invalid type\n");
          errnum = ERR_BAD_ARGUMENT;
          return 1;
        }
      get_cached_info.PacketType=val;
      get_cached_info.Buffer=get_cached_info.BufferSize=0;
      pxe_call(PXENV_GET_CACHED_INFO,&get_cached_info);
      if (get_cached_info.Status)
        return;
      bp=LINEAR(get_cached_info.Buffer);

      grub_printf("%X\n",(unsigned long)bp);
      hexdump(0,bp,get_cached_info.BufferSize);
    }
#endif
  else
    {
      errnum = ERR_BAD_ARGUMENT;
      return 1;
    }
  return 0;
}

#endif

#endif
