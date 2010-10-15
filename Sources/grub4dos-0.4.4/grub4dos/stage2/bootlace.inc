/*
 *  bootlace.inc -- code shared by 16-bit DOS and 32-bit Linux
 *  Copyright (C) 2005,2006,2007  Tinybit(tinybit@tom.com)
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

	.file	"bootlace.inc"

#ifdef __DOS_16

#undef ABS
#define ABS(x) (x - _start + 0x100)

#undef iBX
#define iBX	%bx

#undef iSI
#define iSI	%si

#undef iDI
#define iDI	%di

#undef iDX
#define iDX	%dx

#undef LEAL
#define LEAL	ADDR32 leal

#undef MOVZWL
#define MOVZWL	ADDR32 movzwl

#undef MOVL
#define MOVL	ADDR32 movl

#undef MOVW
#define MOVW	ADDR32 movw

#undef CMPL
#define CMPL	ADDR32 cmpl

#undef CMPW
#define CMPW	ADDR32 cmpw

#undef SUBL
#define SUBL	ADDR32 subl

#undef SBBL
#define SBBL	ADDR32 sbbl

#undef NEGL
#define NEGL	ADDR32 negl

#undef NOTL
#define NOTL	ADDR32 notl

#undef ADCL
#define ADCL	ADDR32 adcl

#undef ADDL
#define ADDL	ADDR32 addl

#undef DIVL
#define DIVL	ADDR32 divl

	. = _start + 0x34		// dos entry point invalid_dos_exec 

#if 0
	movl	$ABS(msg_invalid_dos), %ecx
	call	8f

	movw	$0x4c01, %ax		// EXIT - TERMINATE WITH RETURN CODE 1
	int	$0x21			// call DOS
#else
	jmp	1f
#endif


	//////////////////////////////////////////////////////////////////////
	
	. = _start + 0x47		// dos entry point

1:
	/* dos entry point */

	movl	$0x81, %esi
	movl	%esi, %edi
	movl	$0x7f, %ecx
	movb	$0x0d, %al	/* CR */

	cld
	repnz scasb

	decl	%edi		/* points to CR */
	std
	movb	$0, %al
	stosb
	movl	%edi, %esi
	xorl	%ecx, %ecx
	pushl	%ecx		/* 0 is the end of argv */
	xorl	%edx, %edx	/* find spaces */
3:
	cmpl	$0x81, %edi
	jb	3f
	lodsb
	cmpb	$0x20, %al
	je	1f
	cmpb	$0x09, %al
	jne	2f
1:
	testl	%edx, %edx
	jz	1f
	notl	%edx
	incl	%edi
	pushl	%edi		/* argv */
	decl	%edi
1:
	movb	$0, %al
	stosb
	jmp	3b
2:
	testl	%edx, %edx
	jnz	1f
	incl	%ecx
	notl	%edx
1:
	stosb
	jmp	3b
3:
	testl	%edx, %edx
	jz	1f
	incl	%edi
	pushl	%edi
1:
	pushl	%esi		/* dummy for program name */
	incl	%ecx
	pushl	%ecx		/* argc */
	cld
#else

#undef ABS
#define ABS(x) (x)

#undef iBX
#define iBX	%ebx

#undef iSI
#define iSI	%esi

#undef iDI
#define iDI	%edi

#undef iDX
#define iDX	%edx

#undef LEAL
#define LEAL	leal

#undef MOVZWL
#define MOVZWL	movzwl

#undef MOVL
#define MOVL	movl

#undef MOVW
#define MOVW	movw

#undef CMPL
#define CMPL	cmpl

#undef CMPW
#define CMPW	cmpw

#undef SUBL
#define SUBL	subl

#undef SBBL
#define SBBL	sbbl

#undef NEGL
#define NEGL	negl

#undef NOTL
#define NOTL	notl

#undef ADCL
#define ADCL	adcl

#undef ADDL
#define ADDL	addl

#undef DIVL
#define DIVL	divl

#endif

	popl	%ecx		# argc

	/* argc must be > 1 */

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$1, %ecx
	movl	$ABS(msg_no_args), %ecx
	jbe	4f	/* error */

	/* Note: ECX does not hold the argc now. */

	xorl	%eax, %eax		# EAX will point to file name
	//movl	%ebx, ABS(filename)	# save 0 to filename
	popl	%ecx		# discard argv[0], the program name
1:
	popl	%ecx		# argv
	movl	%ecx, %ebx
	test 	%ecx, %ecx
	jz	1f		# end of arguments

	/* check if it is an option */
	cmpb	$0x2D, (iBX)	# "-"
	jz	2f

	/* it is a filename */

	testl	%eax, %eax	# filename already specified?
	//cmpl	$0, ABS(filename)
	jz	3f

	/* error: only one file should be specified. */

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	movl	$ABS(msg_too_many_files), %ecx
	jmp	4f	/* error */

3:

	/* not yet, this is the first filename */

	movl	%ecx, %eax		# save ECX to EAX
	//movl	%ecx, ABS(filename)	# save ECX to filename
	jmp	1b			# continue

2:

	/* it is an option */

	/* check --no-backup-mbr */

	movl	$16, %ecx
	movl	$ABS(option_no_backup_mbr), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$1, ABS(backup_mbr)
	movl	$ABS(msg_option_backup_mbr), %ecx
	jnz	4f	/* error */
	movl	$0, ABS(backup_mbr)
	jmp	1b

3:
	/* check --force-backup-mbr */

	movl	$19, %ecx
	movl	$ABS(option_force_backup_mbr), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$1, ABS(backup_mbr)
	movl	$ABS(msg_option_backup_mbr), %ecx
	jnz	4f	/* error */
	movl	$2, ABS(backup_mbr)
	jmp	1b

3:

	/* check --mbr-enable-floppy */

	movl	$20, %ecx
	movl	$ABS(option_mbr_enable_floppy), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0, ABS(mbr_floppy)
	movl	$ABS(msg_option_mbr_floppy), %ecx
	jnz	4f	/* error */
	movl	$2, ABS(mbr_floppy)
	jmp	1b

3:

	/* check --mbr-disable-floppy */

	movl	$21, %ecx
	movl	$ABS(option_mbr_disable_floppy), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0, ABS(mbr_floppy)
	movl	$ABS(msg_option_mbr_floppy), %ecx
	jnz	4f	/* error */
	movl	$1, ABS(mbr_floppy)
	jmp	1b

3:

	/* check --mbr-enable-osbr */

	movl	$18, %ecx
	movl	$ABS(option_mbr_enable_osbr), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0, ABS(mbr_osbr)
	movl	$ABS(msg_option_mbr_osbr), %ecx
	jnz	4f	/* error */
	movl	$2, ABS(mbr_osbr)
	jmp	1b

3:

	/* check --mbr-disable-osbr */

	movl	$19, %ecx
	movl	$ABS(option_mbr_disable_osbr), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0, ABS(mbr_osbr)
	movl	$ABS(msg_option_mbr_osbr), %ecx
	jnz	4f	/* error */
	movl	$1, ABS(mbr_osbr)
	jmp	1b

3:

	/* check --duce */

	movl	$7, %ecx
	movl	$ABS(option_duce), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0, ABS(duce)
	movl	$ABS(msg_option_duce), %ecx
	jnz	4f	/* error */
	movl	$1, ABS(duce)
	jmp	1b

3:

	/* check --boot-prevmbr-first */

	movl	$21, %ecx
	movl	$ABS(option_boot_prevmbr_first), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0, ABS(boot_prevmbr)
	movl	$ABS(msg_option_boot_prevmbr), %ecx
	jnz	4f	/* error */
	movl	$1, ABS(boot_prevmbr)
	jmp	1b

3:

	/* check --boot-prevmbr-last */

	movl	$20, %ecx
	movl	$ABS(option_boot_prevmbr_last), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0, ABS(boot_prevmbr)
	movl	$ABS(msg_option_boot_prevmbr), %ecx
	jnz	4f	/* error */
	movl	$2, ABS(boot_prevmbr)
	jmp	1b

3:

	/* check --preferred-drive= */

	movl	$18, %ecx
	movl	$ABS(option_preferred_drive), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0xff, ABS(preferred_drive)
	movl	$ABS(msg_option_preferred_drive), %ecx
	jnz	4f	/* error */

	movl	$0xfe, %ebx
	call	7f	/* parse_number */
	movl	%ebx, %edi	# save EBX to EDI
	movl	$0, %ebx	# invalid file descriptor for CLOSE below
	movl	$ABS(msg_invalid_number), %ecx
	jc	4f	/* error */
	movl	%edi, ABS(preferred_drive)
	jmp	1b

3:

	/* check --preferred-partition= */

	movl	$22, %ecx
	movl	$ABS(option_preferred_partition), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0xff, ABS(preferred_partition)
	movl	$ABS(msg_option_preferred_partition), %ecx
	jnz	4f	/* error */

	movl	$0xfe, %ebx
	call	7f	/* parse_number */
	movl	%ebx, %edi	# save EBX to EDI
	movl	$0, %ebx	# invalid file descriptor for CLOSE below
	movl	$ABS(msg_invalid_number), %ecx
	jc	4f	/* error */
	movl	%edi, ABS(preferred_partition)
	jmp	1b

3:

	/* check --time-out= */

	movl	$11, %ecx
	movl	$ABS(option_time_out), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0xffffff05, ABS(time_out)
	movl	$ABS(msg_option_time_out), %ecx
	jnz	4f	/* error */

	movl	$0xff, %ebx
	call	7f	/* parse_number */
	movl	%ebx, %edi	# save EBX to EDI
	movl	$0, %ebx	# invalid file descriptor for CLOSE below
	movl	$ABS(msg_invalid_number), %ecx
	jc	4f	/* error */
	movl	%edi, ABS(time_out)
	jmp	1b

3:

	/* check --hot-key= */

	movl	$10, %ecx
	movl	$ABS(option_hot_key), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0xffff3920, ABS(hot_key)
	movl	$ABS(msg_option_hot_key), %ecx
	jnz	4f	/* error */

	movl	$0xffff, %ebx
	call	7f	/* parse_number */
	movl	%ebx, %edi	# save EBX to EDI
	movl	$0, %ebx	# invalid file descriptor for CLOSE below
	movl	$ABS(msg_invalid_number), %ecx
	jc	4f	/* error */
	movl	%edi, ABS(hot_key)
	jmp	1b

3:

	/* check --install-partition= */

	movl	$20, %ecx
	movl	$ABS(option_install_partition), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0xffffffff, ABS(install_partition)
	movl	$ABS(msg_option_install_partition), %ecx
	jnz	4f	/* error */

	movl	$0xffff, %ebx
	call	7f	/* parse_number */
	movl	%ebx, %edi	# save EBX to EDI
	movl	$0, %ebx	# invalid file descriptor for CLOSE below
	movl	$ABS(msg_invalid_number), %ecx
	jc	4f	/* error */
	movl	%edi, ABS(install_partition)
	jmp	1b

3:

	/* check --sectors-per-track= */

	movl	$20, %ecx
	movl	$ABS(option_sectors_per_track), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0xffffff3f, ABS(sectors_per_track)
	movl	$ABS(msg_option_sectors_per_track), %ecx
	jnz	4f	/* error */

	movl	$0x3f, %ebx	# maximum is 63
	call	7f	/* parse_number */
	movl	%ebx, %edi	# save EBX to EDI
	movl	$0, %ebx	# invalid file descriptor for CLOSE below
	movl	$ABS(msg_invalid_number), %ecx
	jc	4f	/* error */
	cmpl	$0, %edi
	je	4f	/* error */
	movl	%edi, ABS(sectors_per_track)
	jmp	1b

3:

	/* check --heads= */

	movl	$8, %ecx
	movl	$ABS(option_heads), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0xffff00ff, ABS(heads)
	movl	$ABS(msg_option_heads), %ecx
	jnz	4f	/* error */

	movl	$0x100, %ebx	# maximum is 256
	call	7f	/* parse_number */
	movl	%ebx, %edi	# save EBX to EDI
	movl	$0, %ebx	# invalid file descriptor for CLOSE below
	movl	$ABS(msg_invalid_number), %ecx
	jc	4f	/* error */
	cmpl	$0, %edi
	je	4f	/* error */
	movl	%edi, ABS(heads)
	jmp	1b

3:

	/* check --start-sector= */

	movl	$15, %ecx
	movl	$ABS(option_start_sector), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0xffffffff, ABS(start_sector)
	movl	$ABS(msg_option_start_sector), %ecx
	jnz	4f	/* error */

	movl	$0xfffffffe, %ebx
	call	7f	/* parse_number */
	movl	%ebx, %edi	# save EBX to EDI
	movl	$0, %ebx	# invalid file descriptor for CLOSE below
	movl	$ABS(msg_invalid_number), %ecx
	jc	4f	/* error */
	movl	%edi, ABS(start_sector)
	jmp	1b

3:

	/* check --total-sectors= */

	movl	$16, %ecx
	movl	$ABS(option_total_sectors), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0, ABS(total_sectors)
	movl	$ABS(msg_option_total_sectors), %ecx
	jnz	4f	/* error */

	movl	$0xffffffff, %ebx
	call	7f	/* parse_number */
	movl	%ebx, %edi	# save EBX to EDI
	movl	$0, %ebx	# invalid file descriptor for CLOSE below
	movl	$ABS(msg_invalid_number), %ecx
	jc	4f	/* error */
	cmpl	$0, %edi
	je	4f	/* error */
	movl	%edi, ABS(total_sectors)
	jmp	1b

3:

	/* check --floppy= */

	movl	$9, %ecx
	movl	$ABS(option_floppy_partition), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0xffffffff, ABS(floppy)
	movl	$ABS(msg_option_floppy), %ecx
	jnz	4f	/* error */

	movl	$0xfe, %ebx
	call	7f	/* parse_number */
	movl	%ebx, %edi	# save EBX to EDI
	movl	$0, %ebx	# invalid file descriptor for CLOSE below
	movl	$ABS(msg_invalid_number), %ecx
	jc	4f	/* error */
	movl	%edi, ABS(floppy)
	jmp	1b

3:

	/* check --read-only */

	movl	$12, %ecx
	movl	$ABS(option_read_only), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$2, ABS(read_only)
	movl	$ABS(msg_option_read_only), %ecx
	jnz	4f	/* error */

	movl	$0, ABS(read_only)
	jmp	1b

3:

	/* check --floppy */

	movl	$9, %ecx
	movl	$ABS(option_floppy), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0xffffffff, ABS(floppy)
	movl	$ABS(msg_option_floppy), %ecx
	jnz	4f	/* error */

	movl	$0xff, ABS(floppy)
	jmp	1b

3:

	/* check --lba */

	movl	$6, %ecx
	movl	$ABS(option_lba), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0xffffffff, ABS(lba)
	movl	$ABS(msg_option_lba), %ecx
	jnz	4f	/* error */

	movl	$1, ABS(lba)
	jmp	1b

3:

	/* check --chs */

	movl	$6, %ecx
	movl	$ABS(option_chs), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0xffffffff, ABS(lba)
	movl	$ABS(msg_option_lba), %ecx
	jnz	4f	/* error */

	movl	$0, ABS(lba)
	jmp	1b

3:

	/* check --fat12 */

	movl	$8, %ecx
	movl	$ABS(option_fat12), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0xfffffff7, ABS(fstypes)
	jnz	2f
	movl	$0, ABS(fstypes)
2:
	orb	$1, ABS(fstypes)
	jmp	1b

3:

	/* check --fat16 */

	movl	$8, %ecx
	movl	$ABS(option_fat16), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0xfffffff7, ABS(fstypes)
	jnz	2f
	movl	$0, ABS(fstypes)
2:
	orb	$2, ABS(fstypes)
	jmp	1b

3:

	/* check --fat32 */

	movl	$8, %ecx
	movl	$ABS(option_fat32), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0xfffffff7, ABS(fstypes)
	jnz	2f
	movl	$0, ABS(fstypes)
2:
	orb	$4, ABS(fstypes)
	jmp	1b

3:

	/* check --ntfs */

	movl	$7, %ecx
	movl	$ABS(option_ntfs), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0xfffffff7, ABS(fstypes)
	jnz	2f
	movl	$0, ABS(fstypes)
2:
	orb	$8, ABS(fstypes)
	jmp	1b

3:

	/* check --ext2 */

	movl	$7, %ecx
	movl	$ABS(option_ext2), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0xfffffff7, ABS(fstypes)
	jnz	2f
	movl	$0, ABS(fstypes)
2:
	orb	$0x10, ABS(fstypes)
	jmp	1b

3:

	/* check --vfat */

	movl	$7, %ecx
	movl	$ABS(option_vfat), %esi
	movl	%ebx, %edi

	cld
	repz cmpsb

	jnz	3f

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0xfffffff7, ABS(fstypes)
	jnz	2f
	movl	$0, ABS(fstypes)
2:
	orb	$7, ABS(fstypes)
	jmp	1b

3:

	movl	$ABS(msg_invalid_option), %ecx
	jmp	4f	/* error */

1:

//----------------------------------------------------------------------------
	/* end of arguments */

	/* ECX=EBX=0 */

	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	testl	%eax, %eax		# filename already specified?
	//cmpl	$0, ABS(filename)	# filename already specified?
	movl	$ABS(msg_no_file), %ecx
	jz	4f	/* error */

	xchgl	%eax, %ebx		# move EAX to EBX
	//movl	ABS(filename), %ebx	# move filename to EBX

	/* EBX points to the pathname of the file. */

	/* check if preferred_partition matches preferred_drive */

	movb	ABS(preferred_drive), %al
	movb	ABS(preferred_partition), %ah

	cmpb	$0xff, %al
	jne	1f

	movl	%ebx, %edi	# save EBX to EDI
	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	cmpb	$0xff, %ah
	movl	$ABS(msg_partition_without_drive), %ecx
	jne	4f	/* error */
	movl	%edi, %ebx	# restore EBX from EDI
1:

	/* EBX points to the pathname of the file. */

	movl	%ebx, %edi	# save EBX to EDI
	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	
	cmpl	$0xffffffff, ABS(floppy)
	je	1f		# --floppy is not specified

	/* if --floppy is specified, should not specify the options for MBR. */
	cmpl	$1, ABS(backup_mbr)
	movl	$ABS(msg_option_backup_mbr_with_floppy), %ecx
	jnz	4f	/* error */

	cmpl	$0, ABS(mbr_floppy)
	movl	$ABS(msg_option_mbr_floppy_with_floppy), %ecx
	jnz	4f	/* error */
	
	cmpl	$0, ABS(mbr_osbr)
	movl	$ABS(msg_option_mbr_osbr_with_floppy), %ecx
	jnz	4f	/* error */
	
	cmpl	$0, ABS(duce)
	movl	$ABS(msg_option_duce_with_floppy), %ecx
	jnz	4f	/* error */
	
	cmpl	$0, ABS(boot_prevmbr)
	movl	$ABS(msg_option_boot_prevmbr_with_floppy), %ecx
	jnz	4f	/* error */
	
	cmpl	$0xff, ABS(preferred_drive)
	movl	$ABS(msg_option_preferred_drive_with_floppy), %ecx
	jnz	4f	/* error */

	cmpl	$0xff, ABS(preferred_partition)
	movl	$ABS(msg_option_preferred_partition_with_floppy), %ecx
	jnz	4f	/* error */

	cmpl	$0xffffffff, ABS(install_partition)
	movl	$ABS(msg_option_install_partition_with_floppy), %ecx
	jnz	4f	/* error */

	cmpl	$0xffffff05, ABS(time_out)
	movl	$ABS(msg_option_time_out_with_floppy), %ecx
	jnz	4f	/* error */

	cmpl	$0xffff3920, ABS(hot_key)
	movl	$ABS(msg_option_hot_key_with_floppy), %ecx
	jnz	4f	/* error */
	
	/* for a floppy, start_sector should be 0, and
	 * for a partition, start_sector should be non-zero
	 */
	cmpl	$0xff, ABS(floppy)
	je	2f		# a real floppy

	/* a single partition */
	cmpl	$0, ABS(start_sector)
	movl	$ABS(msg_partition_start), %ecx
	jz	4f	/* error */

	jmp	3f	
2:
	/* a real floppy */
	/* */
	cmpl	$0xffffffff, ABS(start_sector)
	je	3f
	cmpl	$0, ABS(start_sector)
	movl	$ABS(msg_floppy_start), %ecx
	jnz	4f	/* error */

3:
	jmp	2f
1:

	/* EBX=0 */

	cmpl	$0xffffffff, ABS(install_partition)
	je	1f		# --install-partition is not specified

	/* should not specify --install-partition with options for MBR. */
	cmpl	$1, ABS(backup_mbr)
	movl	$ABS(msg_option_backup_mbr_with_partition), %ecx
	jnz	4f	/* error */

	cmpl	$0, ABS(mbr_floppy)
	movl	$ABS(msg_option_mbr_floppy_with_partition), %ecx
	jnz	4f	/* error */
	
	cmpl	$0, ABS(mbr_osbr)
	movl	$ABS(msg_option_mbr_osbr_with_partition), %ecx
	jnz	4f	/* error */
	
	cmpl	$0, ABS(duce)
	movl	$ABS(msg_option_duce_with_partition), %ecx
	jnz	4f	/* error */
	
	cmpl	$0, ABS(boot_prevmbr)
	movl	$ABS(msg_option_boot_prevmbr_with_partition), %ecx
	jnz	4f	/* error */
	
	cmpl	$0xff, ABS(preferred_drive)
	movl	$ABS(msg_option_preferred_drive_with_partition), %ecx
	jnz	4f	/* error */

	cmpl	$0xff, ABS(preferred_partition)
	movl	$ABS(msg_option_preferred_partition_with_partition), %ecx
	jnz	4f	/* error */

	cmpl	$0xffffff05, ABS(time_out)
	movl	$ABS(msg_option_time_out_with_partition), %ecx
	jnz	4f	/* error */

	cmpl	$0xffff3920, ABS(hot_key)
	movl	$ABS(msg_option_hot_key_with_partition), %ecx
	jnz	4f	/* error */

	cmpl	$0xffffff3f, ABS(sectors_per_track)
	movl	$ABS(msg_option_sectors_per_track_with_partition), %ecx
	jnz	4f	/* error */

	cmpl	$0xffff00ff, ABS(heads)
	movl	$ABS(msg_option_heads_with_partition), %ecx
	jnz	4f	/* error */

	cmpl	$0xffffffff, ABS(start_sector)
	movl	$ABS(msg_option_start_sector_with_partition), %ecx
	jnz	4f	/* error */

	cmpl	$0, ABS(total_sectors)
	movl	$ABS(msg_option_total_sectors_with_partition), %ecx
	jnz	4f	/* error */

	jmp	2f
1:

	/* EBX=0 */

	/* this is an MBR device. Should not specify floppy-specific options. */

	testl	$1, ABS(boot_prevmbr)	/* --boot-prevmbr-first specified? */
	jnz	2f			/* Yes, specified. Continue */
	/* No, not specified. */
	/* Should not specify --time-out without --boot-prevmbr-first. */
	cmpl	$0xffffff05, ABS(time_out)
	movl	$ABS(msg_option_time_out_without_prevmbr), %ecx
	jne	4f	/* error */

	/* Should not specify --hot-key without --boot-prevmbr-first. */
	cmpl	$0xffff3920, ABS(hot_key)
	movl	$ABS(msg_option_hot_key_without_prevmbr), %ecx
	jne	4f	/* error */

2:

	/* EBX=0, EDI points to the pathname of the file. */

//----------------------------------------------------------------------------
	movl	%edi, %ebx	# restore EBX from EDI
	
	/* EBX=EDI points to the pathname of the file. */

#ifdef __DOS_16

	xorl	%eax, %eax
	//movl	%ebx, %edi	/* EBX points to the pathname of the file. */
	movl	$0xfe, %ebx
	call	7f	/* parse_number */
	jc	2f	/* invalid number */
	/* EBX holds the bios drive number */
	movl	%ebx, ABS(bios_drive_number)
	jmp	1f
2:
	//AH = 3Dh	//OPEN EXISTING FILE
	//AL = access and sharing modes (see #01402)
	//DS:DX -> ASCIZ filename
	//CL = attribute mask of files to look for (server call only)

	//Return:
	//CF clear if successful AX = file handle
	//CF set on error AX = error code (01h,02h,03h,04h,05h,0Ch,56h)

	movzbl	ABS(read_only), %eax
	movb	$0x3d, %ah	// open file for read/write
	movl	%edi, %edx	// DS:DX points to ASCIZ filename
	int	$0x21
	jnc	1f
	negl	%eax		/* EAX < 0 */
1:
#else

	/* int open(const char *pathname, int flags, mode_t mode) */

	movl	$5, %eax	# sys_open
	movzbl	ABS(read_only), %ecx	# O_RDWR = 02
	int	$0x80
#endif
	xorl	%ebx, %ebx	# invalid file descriptor for CLOSE below
	movl	$ABS(msg_open_file_rw), %ecx
	cmpw	$2, ABS(read_only)
	je	1f
	movl	$ABS(msg_open_file_ro), %ecx
1:
	testl	%eax, %eax
	js	4f	/* error */

//----------------------------------------------------------------------------
	/* open file succeeded */

	pushl	%eax		# file descriptor, or 0 if specified BIOS drive
	movl	%eax, %ebx

	/* EBX and the stack holds the file descriptor number or 0 */
	
#ifdef __DOS_16

	xorl	%eax, %eax
	cmpl	$0xff, ABS(bios_drive_number)
	je	1f
	movw	$0x204, %ax	# read 4 sectors of the MBR
	movw	$ABS(mbr_63_sectors), %bx
	movw	$1, %cx
	movb	ABS(bios_drive_number), %dl
	movb	$0, %dh
	int	$0x13
	jc	2f		# stop on error
	movw	$0x201, %ax	# read 1 sector of track 1(the 2nd track)
	movw	$ABS(mbr_63_sectors + 0x7E00), %bx
	movw	$1, %cx
	movb	ABS(bios_drive_number), %dl
	movb	$1, %dh
	int	$0x13
	clc			# ignore error
	jmp	2f
1:
	//READ FROM FILE OR DEVICE

	//AH = 3Fh
	//BX = file handle
	//CX = number of bytes to read
	//DS:DX -> buffer for data

	//Return:
	//CF clear if successful AX = number of bytes actually read (0 if at EOF before call)
       	//CF set on error AX = error code (05h,06h) 
	movl	$0x3f00, %eax
	movl	$((63+1)*512), %ecx
	movl	$ABS(mbr_63_sectors), %edx
	int	$0x21
2:
	jnc	1f
	negl	%eax
1:
#else
	/* ssize_t read(int fd, void *buf, size_t count) */

	movl	$3, %eax	# sys_read
	movl	$ABS(mbr_63_sectors), %ecx
	movl	$((63+1)*512), %edx	# read 63+1 sectors
	int	$0x80
#endif

	popl	%ebx		# file descriptor, or 0 if specified BIOS drive
	testl	%eax, %eax
	movl	$ABS(msg_read_file), %ecx
	js	4f	/* error */

//----------------------------------------------------------------------------

	/* rewind the file for write! important! */

	pushl	%ebx		# file descriptor, or 0 if specified BIOS drive

#ifdef __DOS_16

	xorl	%eax, %eax
	cmpl	$0xff, ABS(bios_drive_number)
	jne	1f
	//LSEEK - SET CURRENT FILE POSITION

	//AH = 42h
	//AL = origin of move
	//	00h start of file
	//	01h current file position
	//	02h end of file
	//BX = file handle
	//CX:DX = (signed) offset from origin of new file position

	//Return:
	//CF clear if successful DX:AX = new file position in bytes from start of file
	//CF set on error AX = error code (01h,06h) 
	movl	$0x4200, %eax
	movl	$0, %ecx
	movl	$0, %edx
	int	$0x21
	jnc	1f
	negl	%eax
1:
#else
	/* off_t lseek(int fildes, off_t offset, int whence) */

	movl	$19, %eax	# sys_lseek
	movl	$0, %ecx
	movl	$0, %edx	# SEEK_SET = 0
	int	$0x80
#endif

	popl	%ebx		# file descriptor, or 0 if specified BIOS drive
	testl	%eax, %eax
	movl	$ABS(msg_lseek_file), %ecx
	js	4f	/* error */

//----------------------------------------------------------------------------
	/* remember EBX holds file descriptor number, and don't touch it */

	/* check if the partition table is valid. */

	movl	$ABS(mbr_63_sectors), %esi
	cmpw	$0xAA55, 0x1fe(iSI)
	movl	$ABS(msg_boot_signature_mbr), %ecx
	//jne	4f	/* error */
	clc
	jne	2f

	call	9f	/* probe_geometry */
	jnc	1f

2:

	/* should be floppy since no partition table or no boot signature. */

	/* CF=0 means no boot signature, CF=1 means no partition table. */

	jnc	2f

	movl	$ABS(msg_invalid_partition_table), %ecx
2:
	cmpl	$0xffffffff, ABS(floppy)	/* Is --floppy specified? */
	je	4f	/* error */
	
	/* Yes, --floppy is specified */

	/* ESI points to ABS(mbr_63_sectors) */
	call	5f	/* floppy routine */
	movl	$0, %ecx
	jc	4f	/* error */
	
	jmp	2f	/* write file */
1:

//----------------------------------------------------------------------------
	/* the image type is MBR. So, should not specify --floppy */

	cmpl	$0xffffffff, ABS(floppy)
	movl	$ABS(msg_floppy_partition_table), %ecx
	jne	4f	/* error */
	
	cmpl	$0xffffff3f, ABS(sectors_per_track)
	movl	$ABS(msg_option_sectors_per_track_for_mbr), %ecx
	jnz	4f	/* error */

	cmpl	$0xffff00ff, ABS(heads)
	movl	$ABS(msg_option_heads_for_mbr), %ecx
	jnz	4f	/* error */

	cmpl	$0xffffffff, ABS(start_sector)
	movl	$ABS(msg_option_start_sector_for_mbr), %ecx
	jnz	4f	/* error */

	cmpl	$0, ABS(total_sectors)
	movl	$ABS(msg_option_total_sectors_for_mbr), %ecx
	jnz	4f	/* error */

	cmpl	$0xffffffff, ABS(lba)
	movl	$ABS(msg_option_lba_for_mbr), %ecx
	jnz	4f	/* error */

	cmpl	$0xfffffff7, ABS(fstypes)
	movl	$ABS(msg_option_fstypes_for_mbr), %ecx
	jnz	4f	/* error */

	cmpl	$((pre_stage2_start - _start1) / 512), ABS(probed_sectors_per_track)
	movl	$ABS(msg_sectors_per_track), %ecx
	jb	4f	/* error */

	cmpl	$0xffffffff, ABS(install_partition)
	movl	$ABS(msg_option_install_partition_not_implemented), %ecx
	je	1f
	jmp	4f	/* error */
	/* EBX holds the file descriptor number or 0 if it is BIOS drive */
	/* move partition table forward to the beginning of the 17th sector */
	movl	$ABS(mbr_63_sectors + 0x01be), %esi
	movl	$ABS(mbr_63_sectors + 0x2000), %edi
	movl	$0x10, %ecx	# 0x10 dwords = 0x40 bytes

	cld
	repz movsl

	/* initialize the current_partition number */
	movzbl	ABS(install_partition), %eax
	movl	%eax, ABS(current_partition)

3:
	/* load the next partition and modify the boot record */
	call	0f	/* partition */
	jc	3f	/* done */
	call	2f	/* write to file */
	incl	ABS(current_partition)
	movw	ABS(install_partition), %ax
	addb	%ah, %al	/* Max partition number for install */
	cmpb	%al, ABS(current_partition)
	jbe	3b
3:
	/* all partitions have been installed. */
	
	xorl	%eax, %eax	# exit code = 0
	
#ifdef __DOS_16
	
	movb	$0x4c, %ah	// EXIT - TERMINATE WITH RETURN CODE in AL
	int	$0x21		// call DOS

#else
	xchgl	%eax, %ebx	# move exit code in EAX to EBX
	movl	$1, %eax	# sys_exit
	int	$0x80
#endif
		
	ret

1:
//----------------------------------------------------------------------------
	/* backup mbr */

	/* auto backup */
	cmpl	$1, ABS(backup_mbr)
	jnz	1f

	/* check if the second sector consists of 512 dups of one byte */
	movb	ABS(mbr_63_sectors + 0x200), %al
	movl	$ABS(mbr_63_sectors + 0x200), %edi
	movl	$0x200, %ecx

	cld
	repz scasb

	jz	1f			# yes, enable backup

	movl	$0, ABS(backup_mbr)	# no, do not backup

1:

//----------------------------------------------------------------------------
	cmpl	$0, ABS(backup_mbr)
	jz	1f		# will not backup

	movl	$ABS(mbr_63_sectors), %esi
	movl	$ABS(mbr_63_sectors + 0x200), %edi
	movl	$0x80, %ecx	# 0x80 dwords = 0x200 bytes

	cld
	repz movsl

1:

//----------------------------------------------------------------------------
	/* copy byte 0000 to 01b7 of grldr.mbr to ABS(mbr_63_sectors) */

	movl	$ABS(_start1), %esi
	movl	$ABS(mbr_63_sectors), %edi
	movl	$0x6e, %ecx	# 0x6e dwords = 0x1b8 bytes

	cld
	repz movsl

	/* locate the BPB sector */

	movl	ABS(probed_sectors_per_track), %eax
	movl	$63, %esi
	testl	%ebx, %ebx
	jz	1f		# BIOS drive
	movl	%eax, %esi
1:
	shll	$9, %esi
	addl	$ABS(mbr_63_sectors), %esi

	/* ESI points to BPB sector */

	/* check if it has a valid FAT BPB */
	cmpw	$512, 0x0B(iSI)
	jne	1f
	cmpw	%ax, 0x18(iSI)
	jne	1f
	cmpl	%eax, 0x1C(iSI)	#; partition must start at the second track
	jne	1f
	movl	ABS(probed_heads), %eax
	cmpw	%ax, 0x1A(iSI)
	jne	1f
	movb	0x0D(iSI), %cl
	testb	%cl, %cl
	jz	1f
	movw	$128, %ax
	divb	%cl
	testb	%ah, %ah
	jnz	1f
	movw	0x0E(iSI), %cx
	jcxz	1f
	movb	0x10(iSI), %al	# number of FATs
	decw	%ax
	cmpb	$1, %al
	ja	1f

	/* copy the BPB to ABS(mbr_63_sectors) */
	addl	$0x0B, %esi
	movl	$ABS(mbr_63_sectors + 0x0B), %edi
	movl	$(0x5A - 0x0B), %ecx

	cld
	repz movsb

	/* modify reserved sectors */
	movl	$ABS(mbr_63_sectors), %esi
	movl	0x1C(iSI), %eax			#; EAX=sectors per track
	addw	%ax, 0x0E(iSI)

	/* modify total sectors word */
	movw	0x13(iSI), %ax			#; EAX hi word = 0
	testw	%ax, %ax
	jz	3f
	addw	0x1C(iSI), %ax
	movw	%ax, 0x13(iSI)
3:
	/* modify total sectors dword */
	movl	0x20(iSI), %eax
	testl	%eax, %eax
	jz	3f
	addl	0x1C(iSI), %eax
	movl	%eax, 0x20(iSI)
3:
	/* clear the hidden sectors */
	xorl	%eax, %eax
	movl	%eax, 0x1C(iSI)

1:
	/* modify ABS(mbr_floppy) bit */

	movb	ABS(mbr_floppy), %al
	andb	$0x01, %al
	movb	%al, ABS(mbr_63_sectors + 2)

	/* modify ABS(mbr_osbr) bit */

	movb	ABS(mbr_osbr), %al
	andb	$0x01, %al
	shlb	$1, %al
	orb	%al, ABS(mbr_63_sectors + 2)

	/* modify ABS(duce) bit */

	movb	ABS(duce), %al
	andb	$0x01, %al
	shlb	$2, %al
	orb	%al, ABS(mbr_63_sectors + 2)

	/* modify ABS(boot_prevmbr) bit */

	movb	ABS(boot_prevmbr), %al
	notb	%al
	andb	$0x01, %al
	rorb	$1, %al
	orb	%al, ABS(mbr_63_sectors) + 2

	/* modify ABS(time_out) byte */

	movb	ABS(time_out), %al
	movb	%al, ABS(mbr_63_sectors) + 3

	/* modify ABS(hot_key) byte */

	movw	ABS(hot_key), %ax
	movw	%ax, ABS(mbr_63_sectors) + 4

	/* modify ABS(preferred_drive) byte */

	movb	ABS(preferred_drive), %al
	movb	%al, ABS(mbr_63_sectors) + 6

	/* modify ABS(preferred_partition) byte */

	movb	ABS(preferred_partition), %al
	movb	%al, ABS(mbr_63_sectors) + 7

	/* copy byte 0400 and the rest of grldr.mbr to ABS(mbr_63_sectors) */

	movl	$ABS(_start1 + 0x400), %esi
	movl	$ABS(mbr_63_sectors + 0x400), %edi
	movl	$((pre_stage2_start - _start1 - 0x400) / 4), %ecx

	cld
	repz movsl

//----------------------------------------------------------------------------
2:	/* label for the above floppy code to jump */

	/* OK. Now write file! */

	xorl	%eax, %eax
	
	cmpw	$2, ABS(read_only)
	jne	3f

	pushl	%ebx		# file descriptor, or 0 if specified BIOS drive

	/* for safety, don't write too many sectors */
	cmpl	$0xffffffff, ABS(install_partition)
	jne	1f
	cmpl	$0xffffffff, ABS(floppy)
	je	2f
1:
	cmpl	$4, ABS(sectors_to_write)
	jbe	1f
	movl	$4, ABS(sectors_to_write)
	jmp	1f
2:
	cmpl	$((pre_stage2_start - _start1) / 512), ABS(sectors_to_write)
	jbe	1f
	movl	$((pre_stage2_start - _start1) / 512), ABS(sectors_to_write)
1:
#ifdef __DOS_16

	xorl	%eax, %eax
	cmpl	$0xff, ABS(bios_drive_number)
	je	1f
	movb	$0x03, %ah	# write to disk
	movb	ABS(sectors_to_write), %al
//	cmpl	$0xffffffff, ABS(floppy)
//	je	2f
//	movb	$4, %al		# write 4 sectors if it is floppy
//2:
	movw	$ABS(mbr_63_sectors), %bx
	movw	$1, %cx
	movw	ABS(bios_drive_number), %dx
	int	$0x13
	jmp	2f
1:
	//WRITE TO FILE OR DEVICE

	//AH = 40h
	//BX = file handle
	//CX = number of bytes to write
	//DS:DX -> data to write

	//Return:
	//CF clear if successful AX = number of bytes actually written
	//CF set on error AX = error code (05h,06h) 
	movl	$0x4000, %eax
	movl	ABS(sectors_to_write), %ecx
	shll	$9, %ecx
	//movl	$(pre_stage2_start - _start1), %ecx
//	cmpl	$0xffffffff, ABS(floppy)
//	je	1f
//	movl	$0x800, %ecx	# write 4 sectors if it is floppy
//1:
	movl	$ABS(mbr_63_sectors), %edx
	int	$0x21
2:
	jnc	1f
	negl	%eax
1:
#else
	/* ssize_t write(int fd, const void *buf, size_t count) */

	movl	$4, %eax	# sys_write
	movl	$ABS(mbr_63_sectors), %ecx
	movl	ABS(sectors_to_write), %edx
	shll	$9, %edx
	//movl	$(pre_stage2_start - _start1), %edx	# length of grldr.mbr
//	cmpl	$0xffffffff, ABS(floppy)
//	je	1f
//	movl	$0x800, %edx	# write 4 sectors if it is floppy
//1:
	int	$0x80
#endif

	popl	%ebx		# file descriptor, or 0 if specified BIOS drive
3:
	cmpl	$0xffffffff, ABS(install_partition)
	je	3f
	
	/* for install_partition, SF=0 when success, SF=1 when failure */
	testl	%eax, %eax
	ret
3:
	testl	%eax, %eax
	movl	$ABS(msg_write_file), %ecx
	js	4f	/* error */

//----------------------------------------------------------------------------
	cmpl	$0xffffffff, ABS(floppy)
	jne	3f

	movl	$ABS(msg_geometry_S), %ecx
	call	8f	/* linux_print */

	movl	ABS(probed_sectors_per_track), %eax
	call	6f	/* print decimal number */

	movl	$ABS(msg_geometry_H), %ecx
	call	8f	/* linux_print */

	movl	ABS(probed_heads), %eax
	call	6f	/* print decimal number */

	movl	$ABS(msg_success), %ecx
	call	8f	/* linux_print */
3:
//success:

	# EBX=file descriptor, or 0 if specified BIOS drive
	
	xorl	%eax, %eax	# exit code = 0

	jmp	1f
	
//error:
4:
	# EBX=file descriptor, or 0 if specified BIOS drive
	
	testl	%ecx, %ecx
	jz	2f
	call	8f	/* linux_print */
2:
	movl	$ABS(msg_usage), %ecx
	call	8f	/* linux_print */

	movl	$1, %eax	# exit code = 1

1:
	testl	%ebx, %ebx
	jz	1f

	pushl	%eax		# exit code

#ifdef __DOS_16

	movb	$0x3e, %ah	// close file (BX = file handle)
	int	$0x21
	jnc	2f
	negl	%eax		/* EAX < 0 */
2:
#else

	/* int close(int fd); */
	movl	$6, %eax	# sys_close
	int	$0x80
#endif
	movl	$ABS(msg_close_file), %ecx
	testl	%eax, %eax
	
	popl	%eax		# exit code

	jns	1f

	call	8f	/* linux_print */

//exit:
1:
#ifdef __DOS_16
	
	movb	$0x4c, %ah	// EXIT - TERMINATE WITH RETURN CODE in AL
	int	$0x21		// call DOS

#else
	xchgl	%eax, %ebx	# move exit code in EAX to EBX
	movl	$1, %eax	# sys_exit
	int	$0x80
#endif
		
	ret

//----------------------------------------------------------------------------
//floppy:
5:

	pushal

	/* Beforehand, try MBR */

	/* ESI=$ABS(mbr_63_sectors) */

	cld
	cmpw	$0xAA55, 0x1fe(iSI)
	jne	1f

	pushl	%esi
	pushl	%edi
	pushl	%ebx

	/* has a valid partition table ? */
	movb	$0, %bl		/* count valid partition entries */
	addl	$0x1be, %esi
3:
	cmpl	$(ABS(mbr_63_sectors) + 0x1fe), %esi
	jnb	3f		/* partition table is OK */
	movl	$4, %ecx

	movl	%esi, %edi
2:
	lodsl
	negl	%eax
	jc	2f
	loop	2b
	/* empty entry, check next */
	jmp	3b
2:
	/* non-empty entry */
	movl	%edi, %esi

	lodsw
	shlb	$1, %al		/* boot indicator */
	jnz	2f
	lodsw
	andb	$63, %al
	jz	2f
	lodsw
	lodsw
	andb	$63, %al
	jz	2f
	lodsl
	negl	%eax
	jnc	2f
	lodsl
	negl	%eax
	jnc	2f
	incb	%bl
	jmp	3b
2:
	stc		/* invalid partition table */
3:
	popl	%eax
	popl	%edi
	popl	%esi

	xchgl	%eax, %ebx

	jc	1f
	testb	%al, %al
	jz	1f

	/* MBR write not allowed */
	
	movl	$0, ABS(sectors_to_write)
	
	movl	$ABS(msg_fstype_mbr_deny), %ecx
	call	8f	/* linux_print */
	
	stc
	popal
	ret
1:
	/* First, try ext2 */

	cmpw	$0xEF53, 0x438(iSI)	/* Magic signature */
	jnz	1f
	xorl	%eax, %eax
	cmpl	%eax, 0x400(iSI)	/* s_inodes_count */
	jz	1f
	cmpl	%eax, 0x404(iSI)	/* s_blocks_count */
	jz	1f
//	cmpw	%ax, 0x458(iSI)		/* s_inode_size, usually 0x80 */
//	jz	1f
	cmpl	%eax, 0x420(iSI)	/* s_blocks_per_group */
	jz	1f
	cmpl	%eax, 0x428(iSI)	/* s_inodes_per_group */
	jz	1f
	movl	0x414(iSI), %eax	/* s_first_data_block */
	movw	%ax, %bx		/* BX=1 for 1K block, 0 otherwise */
	shrl	$1, %eax		/* must be 0 */
	jnz	1f
	movl	0x418(iSI), %ecx	/* s_log_block_size */
	cmpl	$4, %ecx		/* max size of block is 16K */
	ja	1f
	negw	%cx			/* CF=0 for 1K block, CF=1 otherwise */
	adcw	%ax, %bx		/* EAX=0 */
	decw	%bx
	jnz	1f

	/* BX = 0 */

	movl	$ABS(_start1 + 0x826), %edi /* inode size in the 5th sector */
	movw	$0x80, %ax		/* EXT2_GOOD_OLD_INODE_SIZE */
	movw	%ax, (iDI)		/* inode size */
	movl	0x44C(iSI), %ecx	/* ECX=s_rev_level */
	jecxz	3f			/* EXT2_GOOD_OLD_REV */
	movw	0x458(iSI), %ax		/* AX=s_inode_size */
	testw	%ax, %ax
	jz	1f			/* invalid inode size */
	pushw	%ax
	pushw	%dx
	movb	0x418(iSI), %cl		/* s_log_block_size */
	addb	$10, %cl
	xorw	%dx, %dx		/* DX=0 */
	incw	%dx			/* DX=1 */
	shlw	%cl, %dx		/* DX=block size in bytes */
	xchgw	%ax, %cx		/* CX=s_inode_size */
	xchgw	%ax, %dx		/* AX=block size in bytes */
	xorw	%dx, %dx		/* DX:AX=block size in bytes */
	divw	%cx			/* quo=AX, rem=DX */
	testw	%dx, %dx
	popw	%dx
	popw	%ax
	jnz	1f			/* invalid inode size */
	movw	%ax, (iDI)		/* inode size */
3:
	/* BX = 0 */

	/* super block is sane */

	/* is EXT2 allowed write? */

	testl	$0x10, ABS(fstypes)
	jnz	2f
	/* EXT2 write not allowed */
	
	movl	$0, ABS(sectors_to_write)
	
	movl	$ABS(msg_fstype_ext2_deny), %ecx
	call	8f	/* linux_print */
	
	stc
	popal
	ret
2:
	/* write to ext2 boot area */

	cld
	movl	$ABS(_start1 + 0x800), %esi	/* points to the 5th sector */
	movl	$ABS(mbr_63_sectors), %edi
	lodsw				/* The first 2 byte: short jmp */
	LEAL	0x400(%edi), %esi	/* ESI points to super block */
	stosw

	/* lba indicator byte is not used. so we shouldn't touch it. */

	/* check lba-chs-mode specified, 1 byte */
	cmpl	$-1, ABS(lba)		/* lba-chs-mode  not specified? */
	je	2f			/* yes, do nothing. */

#if 0
	movb	$0x42, %al		/* initialize to lba mode. */
	cmpl	$0, ABS(lba)		/* chs ? */
	jnz	10f			/* no, it is not chs.*/
	movb	$0x02, %al		/* yes, it is chs. */
10:
#else
	/* if specified lba or chs, we simply place a 0x90. */
	movb	$0x90, %al
#endif
	stosb
	decl	%edi
2:

	addl	$(1+10), %edi		/* skip 10-byte OEM name */
	
	/* Sectors per block, byte */
	
	movb	0x18(iSI), %cl		/* s_log_block_size */
	movl	$2, %eax
	shlw	%cl, %ax
	stosb
	
	/* Bytes per block, word */
	
	shlw	$9, %ax			/* block size is word wide */
	stosw

	/* Pointers covered by an indirect block, dword */
	/* Pointers per block, dword */
	
	shrw	$2, %ax
	pushl	%eax		/* Pointers per block */
	addb	$8, %cl
	shll	%cl, %eax
	stosl			/* Pointers covered by an indirect block */
	popl	%eax
	stosl			/* Pointers per block */
	
	/* Sectors per track, word */

	cmpl	$0xFFFFFF3F, ABS(sectors_per_track)
	je	10f
	movb	ABS(sectors_per_track), %al
	cbw
	stosw
	subl	$2, %edi
10:
	addl	$2, %edi

	/* Number of heads, word */
	
	cmpl	$0xFFFF00FF, ABS(heads)
	je	10f
	movw	ABS(heads), %ax
	stosw
	subl	$2, %edi
10:
	addl	$2, %edi
	
	/* hidden sectors(i.e., partition start), dword */
	
	movb	ABS(floppy), %al	/* partition number(0xFF for floppy) */
	cmpb	$0xff, %al
	jne	10f
	/* real floppy */
	cmpl	$0xffffffff, ABS(start_sector)
	sete	%al
	movzbl	%al, %eax
	addl	ABS(start_sector), %eax
	stosl
	jmp	11f
10:
	/* hard drive partition */
	addl	$4, %edi
	cmpl	$0xffffffff, ABS(start_sector)
	je	11f
	movl	ABS(start_sector), %eax
	subl	$4, %edi
	stosl
11:
	
	/* total sectors, dword */

	movl	%es:(iDI), %eax
	cmpl	%eax, ABS(total_sectors)
	jbe	10f
	movl	ABS(total_sectors), %eax
10:
	stosl
	
	/* drive number, byte */

	//this byte is not used.
	incl	%edi

	/* partition number, byte */

	movb	ABS(floppy), %al
	stosb
	
	/* reserved, word */

	incl	%edi
	incl	%edi

	/* Number of inodes per group, dword */

	movl	0x28(iSI), %eax		/* s_inodes_per_group */
	stosl
	
	/* block number for group descriptors, dword */

	movl	0x14(iSI), %eax		/* s_first_data_block */
	incw	%ax
	stosl
	
	/* Machine code begins at offset 0x30, 462 bytes plus 
	 * 2 bytes of boot signature */

	movl	$ABS(_start1 + 0x830), %esi
	lodsl
	movb	ABS(floppy), %cl	/* partition number(0xFF for floppy) */
	cmpb	$0xff, %cl
	jne	10f
	/* real floppy */
	cmpl	$0x90C031FC, %eax	/* FC=cld; 31 C0=xor ax,ax; 90=nop */
	jne	10f
	movl	$0x99C031FC, %eax	/* FC=cld; 31 C0=xor ax,ax; 99=cwd */
10:
	stosl
	
	movl	$((464 / 4) - 1), %ecx
	repz movsl
	
	movl	$1, ABS(sectors_to_write)
	
	movl	$ABS(msg_fstype_ext2_allow), %ecx
	call	8f	/* linux_print */
	
	clc
	popal
	ret

1:
	/* Secondly, try FAT12/16/32/NTFS */
	
	cld
	cmpw	$0xAA55, 0x1fe(iSI)	/* boot signature */
	jne	1f			/* not a normal BPB */
	cmpw	$0x200, 0x0b(iSI)	/* bytes per sector */
	jne	1f			/* not a normal BPB */
	movb	0x0d(iSI), %al		/* sectors per cluster */
	testb	%al, %al
	jz	1f			/* invalid if = 0 */
	movb	%al, %cl
	movw	$128, %ax
	divb	%cl			/* quo=AL, rem=AH */
	testb	%ah, %ah
	jnz	1f			/* invalid if not 2^n */
	movw	0x18(iSI), %ax		/* sectors per track */
	testw	%ax, %ax
	jz	1f			/* invalid if = 0 */
	cmpw	$63, %ax
	ja	1f			/* invalid if > 63 */
	movw	0x1a(iSI), %ax		/* number of heads */
	decw	%ax			/* Max head number, should be a byte */
	testb	%ah, %ah		/* should be 0 */
	jnz	1f			/* invalid if number of heads > 256 */
	cmpb	$0xf0, 0x15(iSI)	/* media descriptor */
	jb	1f

//	movw	$0x0600, %bx		/* FAT12/FAT16 */
//	movw	$0x003c, %cx		/* FAT12/FAT16 */

	movb	0x10(iSI), %al		/* number of FATs(NTFS:0, FAT:1,2) */
	cmpb	$2, %al
	ja	1f			/* abnormal FAT */
	movw	0x11(iSI), %ax		/* max root entries */
	testw	%ax, %ax
	jnz	2f			/* FAT12/FAT16 */

	/* FAT32 or NTFS */
	movw	0x13(iSI), %ax		/* total sectors(small) */
	testw	%ax, %ax
	jnz	1f			/* invalid FAT32 BPB */
	movw	0x16(iSI), %ax		/* sectors per FAT(small) */
	testw	%ax, %ax
	jnz	1f			/* invalid FAT32 BPB */
	movb	0x10(iSI), %al		/* number of FATs(NTFS:0, FAT:1,2) */
	testb	%al, %al
	jz	4f			/* NTFS */

	/* FAT32 */
	movl	0x20(iSI), %eax		/* FAT32 total sectors */
	testl	%eax, %eax
	jz	1f
	movl	0x24(iSI), %eax		/* FAT32 sectors per FAT */
	testl	%eax, %eax
	jz	1f
	/* sure it is FAT32 */

	/* is FAT32 allowed write? */

	testl	$0x04, ABS(fstypes)
	jnz	3f
	/* FAT32 write not allowed */
	movl	$0, ABS(sectors_to_write)
	
	movl	$ABS(msg_fstype_fat32_deny), %ecx
	call	8f	/* linux_print */
	
	stc
	popal
	ret
3:
	/* write to FAT32 boot area */
	
	cld
	movl	$ABS(_start1 + 0x400), %esi	/* points to the 3rd sector */
	movl	$ABS(mbr_63_sectors), %edi
	lodsw				/* The first 2 byte: short jmp */
	//leal	0x400(%edi), %esi	/* ESI points to super block */
	stosw

	/* check lba-chs-mode specified, 1 byte */
	cmpl	$-1, ABS(lba)		/* lba-chs-mode  not specified? */
	je	3f			/* yes, do nothing. */
	movb	$0x0e, %al		/* initialize to lba mode. */
	cmpl	$0, ABS(lba)		/* chs ? */
	jnz	10f			/* no, it is not chs.*/
	movb	$0x90, %al		/* yes, it is chs. */
10:
	stosb
	decl	%edi
3:
	addl	$(1+8+2+1+2+1+2+2+1+2), %edi
	addl	$(1+8+2+1+2+1+2+2+1+2), %esi
				/* skip 8-byte OEM name and Bytes per sector */
	
				/* and Sectors per cluster, byte */
				/* and Reserved sectors, word */
				/* and Number of FATs, byte */
				/* (Max root dir entries)Must be 0, word */
				/* (Total sectors small)Must be 0, word */
				/* Media descriptor, byte */
				/* (Sectors per FAT)Must be 0, word */
	/* sectors per track, word */
	lodsw
	addl	$2, %edi
	cmpl	$0xffffff3f, ABS(sectors_per_track)
	je	3f
	movb	ABS(sectors_per_track), %al
	cbw
	subl	$2, %edi
	stosw
3:
	
	/* number of heads, word */
	lodsw
	addl	$2, %edi
	cmpl	$0xffff00ff, ABS(heads)
	je	3f
	movw	ABS(heads), %ax
	subl	$2, %edi
	stosw
3:
	
	/* hidden sectors(i.e., partition start), dword */
	lodsl
	addl	$4, %edi
	cmpl	$0xffffffff, ABS(start_sector)
	je	3f
	movl	ABS(start_sector), %eax
	subl	$4, %edi
	stosl
3:
	
	/* total sectors, dword */
	lodsl
	addl	$4, %edi
	cmpl	$0, ABS(total_sectors)
	je	3f
	movl	ABS(total_sectors), %eax
	subl	$4, %edi
	stosl
3:
	
	/* FAT32 sectors per FAT, dword, etc... */

	addl	$(4+2+2+4+2+2+12+1+1+1+4+11+8), %esi
	addl	$(4+2+2+4+2+2+12+1+1+1+4+11+8), %edi

	//movl	$0xFFB6FCFA, %eax	/* CLI, CLD, MOV DH,FF */
	lodsw
	stosw
	lodsb
	stosb
	lodsb
	
	//decl	%edi

	/* partition number, byte */

	movb	ABS(floppy), %al
	stosb
	
	/* Machine code begins at offset 0x5e, 416 bytes without 
	 * the ending 2 bytes of boot signature */

	//movl	$ABS(_start1 + 0x45e), %esi
	movl	$(416 / 4), %ecx
	repz movsl
	
	movl	$1, ABS(sectors_to_write)
	
	movl	$ABS(msg_fstype_fat32_allow), %ecx
	call	8f	/* linux_print */
	
	clc
	popal
	ret
4:
	/* NTFS */
	movl	0x20(iSI), %eax		/* FAT32 total sectors */
	testl	%eax, %eax
	jnz	1f
	//movw	0x11(%si), %ax		/* max root entries */
	//testw	%ax, %ax
	//jnz	1f
	movw	0x0e(iSI), %ax		/* reserved sectors */
	testw	%ax, %ax
	jnz	1f
	/* sure it is NTFS */

	/* is NTFS allowed write? */

	testl	$0x08, ABS(fstypes)
	jnz	3f
	/* NTFS write not allowed */
	movl	$0, ABS(sectors_to_write)
	
	movl	$ABS(msg_fstype_ntfs_deny), %ecx
	call	8f	/* linux_print */
	
	stc
	popal
	ret
3:
#if 1
	/* write to NTFS boot area */

	cld
	movl	$ABS(_start1 + 0xA00), %esi	/* points to the 6th sector */
	movl	$ABS(mbr_63_sectors), %edi
	lodsw				/* The first 2 byte: short jmp */
	stosw
	
	/* check lba-chs-mode specified, 1 byte */
	cmpl	$-1, ABS(lba)		/* lba-chs-mode  not specified? */
	je	3f			/* yes, do nothing. */
	movb	$0x0e, %al		/* initialize to lba mode. */
	cmpl	$0, ABS(lba)		/* chs ? */
	jnz	10f			/* no, it is not chs.*/
	movb	$0x90, %al		/* yes, it is chs. */
10:
	stosb
	decl	%edi
3:
	addl	$(1+8+2+1+2+1+2+2+1+2), %edi
	addl	$(1+8+2+1+2+1+2+2+1+2), %esi
				/* skip 8-byte OEM name and Bytes per sector */
	
				/* and Sectors per cluster, byte */
				/* and Reserved sectors, word */
				/* and Number of FATs, byte */
				/* (Max root dir entries)Must be 0, word */
				/* (Total sectors small)Must be 0, word */
				/* Media descriptor, byte */
				/* (Sectors per FAT)Must be 0, word */
	/* sectors per track, word */
	lodsw
	addl	$2, %edi
	cmpl	$0xffffff3f, ABS(sectors_per_track)
	je	3f
	movb	ABS(sectors_per_track), %al
	cbw
	subl	$2, %edi
	stosw
3:
	
	/* number of heads, word */
	lodsw
	addl	$2, %edi
	cmpl	$0xffff00ff, ABS(heads)
	je	3f
	movw	ABS(heads), %ax
	subl	$2, %edi
	stosw
3:
	
	/* hidden sectors(i.e., partition start), dword */
	lodsl
	addl	$4, %edi
	cmpl	$0xffffffff, ABS(start_sector)
	je	3f
	movl	ABS(start_sector), %eax
	subl	$4, %edi
	stosl
3:	


	addl	$(4+1+1+1+1+8), %esi
	addl	$(4+1+1+1+1+8), %edi
			/* total sectors, unused, dword */
			/* drive number, byte, and another 3 bytes; Usually 80 00 80 00 */
			/* Number of sectors in the volume, qword */
				//lodsl
				//lodsl
				//addl	$8, %edi
	cmpl	$0, ABS(total_sectors)
	je	3f
	movl	ABS(total_sectors), %eax
	subl	$8, %edi
	stosl
	xorl	%eax, %eax	/* high 32 bits */
	stosl
3:
	
	addl	$(8+8+4+4+8+4), %esi
	addl	$(8+8+4+4+8+4), %edi
			/* 30 - LCN of VCN 0 of the $MFT, qword */
			/* 38 - LCN of VCN 0 of the $MFTMirr, qword */
			/* 40 - Clusters per MFT Record, dword */
			/* 44 - Clusters per Index Record, dword */
			/* 48 - Volume serial number, qword */
			/* 50 - Checksum, usually 0, dword */
	//movl	$0xFFB6FCFA, %eax	/* CLI, CLD, MOV DH,FF */
	lodsw
	stosw
	lodsb
	stosb
	lodsb
	
	//decl	%edi

	/* partition number, byte */

	movb	ABS(floppy), %al
	stosb
	
	/* Machine code begins at offset 0x58, ending at 0x7FF, where 
	 * we assume that NTFS boot record takes up 4 sectors. */

	movl	$((0x800 - 0x58)/ 4), %ecx
	repz movsl
	
	movl	$4, ABS(sectors_to_write)
	
	movl	$ABS(msg_fstype_ntfs_allow), %ecx
	call	8f	/* linux_print */
	
	clc
	popal
	ret
#else
	/* write to NTFS boot area */

	movl	$ABS(mbr_63_sectors), %edi
	LEAL	0x400(%edi), %esi	/* ESI points to super block */
	
	/* hidden sectors(i.e., partition start), dword */
	cmpl	$0xffffffff, ABS(start_sector)
	je	3f
	movl	ABS(start_sector), %eax
	movl	%eax, 0x1c(iDI)
3:
	
	/* drive number needn't touch */

	/* partition number */

	call	4f
	jc	5f
	movl	0x06(iSI), %eax		/* NT 4 */
	cmpl	$0x03E8B800, %eax	/* MOV ES,AX */
	jnz	3f

	cmpl	$0x680007E8, 0x84(iDI)	/* call 008e; push (0D00) */
	jnz	3f
	movb	$0xB6, %al		/* 0xB6="MOV DH,imm8" */
	movb	ABS(floppy), %ah	/* partition number */
	movw	%ax, 0x0a(iSI)
	
	movl	$4, ABS(sectors_to_write)
	
	movl	$ABS(msg_fstype_ntfs_allow), %ecx
	call	8f	/* linux_print */
	
	clc
	popal
	ret
3:
		/* NT 5.0 */

	cmpl	$0x680053E8, 0x71(iDI)	/* call 00C7; push (0D00) */
	jnz	5f
	
	movl	$0x4b, %ebx
	movl	(iBX, iSI), %eax		/* NT 5 */
	cmpl	$0x03E8B800, %eax		/* MOV ES,AX */
	jz	3f
	
	movl	$0x79, %ebx
	movl	(iBX, iSI), %eax		/* NT 5p */
	cmpl	$0x03E8B800, %eax		/* MOV ES,AX */
	jnz	5f
3:
	movb	$0xB6, %al		/* 0xB6="MOV DH,imm8" */
	movb	ABS(floppy), %ah	/* partition number */
	movw	%ax, 4(iBX, iSI)
	
	movl	$4, ABS(sectors_to_write)
	
	movl	$ABS(msg_fstype_ntfs_allow), %ecx
	call	8f	/* linux_print */
	
	clc
	popal
	ret
5:
	/* No NTFS boot record present */
	movl	$0, ABS(sectors_to_write)
	
	movl	$ABS(msg_no_ntfs_boot_record), %ecx
	call	8f	/* linux_print */
	
	stc
	popal
	ret

4:	/* change NT to GR */
	
	pushal
	cld
	movl	$0x00520047, 0x202(iDI)	/* G R L D R */
	addl	$0x0100, %edi
	movl	$0x00fa, %ecx
	movw	$0x014e, %ax    /* AL="N", AH=Carry for SAHF below */
	movl	$0x52444c54, %ebx       /* "TLDR" */

3:
	repnz scasb		/* find "N" */
	jcxz	4f		/* gets the end, exit */
	cmpl	%ebx, (iDI)	/* is it "NTLDR"? */
	jnz	3b		/* no, continue to find */

	/* "NTLDR" is found, so we believe it is NT boot sector. */

	movw	$0x5247, -1(iDI)	/* change "NT" to "GR" */

	/* CF=0 for now */

	lahf			/* Load Flags into AH */
				/* AH = SF:ZF:xx:AF:xx:PF:xx:CF */
				/* AH = binary xxxxxxx0 */
	jmp	3b
4:
	sahf		/* Store AH into flags SF ZF xx AF xx PF xx CF */

	/* CF=0 means "NTLDR" is found, CF=1 means "NTLDR" is not found. */

	popal
	ret
#endif

2:
	/* FAT12/FAT16 */
	movb	0x10(iSI), %al		/* number of FATs(NTFS:0, FAT:1,2) */
	testb	%al, %al
	jz	1f
	movw	0x16(iSI), %ax		/* sectors per FAT(small) */
	testw	%ax, %ax
	jz	1f
//	movw	$(FAT16_message - _start1), %si
	cmpw	$12, %ax
	ja	4f			/* FAT16 */
	/* sure it is FAT12 */

	/* is FAT12 allowed write? */

	testl	$0x01, ABS(fstypes)
	jnz	2f
	/* FAT12 write not allowed */
	movl	$0, ABS(sectors_to_write)
	
	movl	$ABS(msg_fstype_fat12_deny), %ecx
	call	8f	/* linux_print */
	
	stc
	popal
	ret
2:
	/* write to FAT12 boot area */
	
	cld
	movl	$ABS(_start1 + 0x600), %esi	/* points to the 4th sector */
	movl	$ABS(mbr_63_sectors), %edi
	lodsw				/* The first 2 byte: short jmp */
	//leal	0x400(%edi), %esi	/* ESI points to super block */
	stosw
	
	/* check lba-chs-mode specified, 1 byte */
	cmpl	$-1, ABS(lba)		/* lba-chs-mode  not specified? */
	je	2f			/* yes, do nothing. */
	movb	$0x0e, %al		/* initialize to lba mode. */
	cmpl	$0, ABS(lba)		/* chs ? */
	jnz	10f			/* no, it is not chs.*/
	movb	$0x90, %al		/* yes, it is chs. */
10:
	stosb
	decl	%edi
2:
	addl	$(1+8+2+1+2+1+2+2+1+2), %edi
	addl	$(1+8+2+1+2+1+2+2+1+2), %esi
				/* skip 8-byte OEM name and Bytes per sector */
	
				/* and Sectors per cluster, byte */
				/* and Reserved sectors, word */
				/* and Number of FATs, byte */
				/* (Max root dir entries)Must be 0, word */
				/* (Total sectors small)Must be 0, word */
				/* Media descriptor, byte */
				/* (Sectors per FAT)Must be 0, word */
	/* sectors per track, word */
	lodsw
	addl	$2, %edi
	cmpl	$0xffffff3f, ABS(sectors_per_track)
	je	2f
	movb	ABS(sectors_per_track), %al
	cbw
	subl	$2, %edi
	stosw
2:
	
	/* number of heads, word */
	lodsw
	addl	$2, %edi
	cmpl	$0xffff00ff, ABS(heads)
	je	2f
	movw	ABS(heads), %ax
	subl	$2, %edi
	stosw
2:
	
	/* hidden sectors(i.e., partition start), dword */
	lodsl
	addl	$4, %edi
	cmpl	$0xffffffff, ABS(start_sector)
	je	2f
	movl	ABS(start_sector), %eax
	subl	$4, %edi
	stosl
2:	

	/* total sectors, dword */
	lodsl
	addl	$4, %edi
	cmpl	$0, ABS(total_sectors)
	je	2f
	movl	ABS(total_sectors), %eax
	subl	$4, %edi
	stosl
2:

	/* drive number, byte, etc... */

	addl	$(1+1+1+4+11+8), %esi
	addl	$(1+1+1+4+11+8), %edi

	//movl	$0xFFB6FCFA, %eax	/* CLI, CLD, MOV DH,FF */
	lodsw
	stosw
	lodsb
	stosb
	lodsb
	
	//decl	%edi

	/* partition number, byte */

	movb	ABS(floppy), %al
	stosb
	
	/* Machine code begins at offset 0x42, 444 bytes without 
	 * the ending 2 bytes of boot signature */

	//movl	$ABS(_start1 + 0x642), %esi
	movl	$(444 / 4), %ecx
	repz movsl
	
	movl	$1, ABS(sectors_to_write)
	
	movl	$ABS(msg_fstype_fat12_allow), %ecx
	call	8f	/* linux_print */
	
	clc
	popal
	ret

4:

	/* sure it is FAT16 */

	/* is FAT16 allowed write? */

	testl	$0x02, ABS(fstypes)
	jnz	2f
	/* FAT16 write not allowed */
	movl	$0, ABS(sectors_to_write)
	
	movl	$ABS(msg_fstype_fat16_deny), %ecx
	call	8f	/* linux_print */
	
	stc
	popal
	ret
2:
	/* write to FAT16 boot area */
	
	cld
	movl	$ABS(_start1 + 0x600), %esi	/* points to the 4th sector */
	movl	$ABS(mbr_63_sectors), %edi
	lodsw				/* The first 2 byte: short jmp */
	//leal	0x400(%edi), %esi	/* ESI points to super block */
	stosw
	
	/* check lba-chs-mode specified, 1 byte */
	cmpl	$-1, ABS(lba)		/* lba-chs-mode  not specified? */
	je	2f			/* yes, do nothing. */
	movb	$0x0e, %al		/* initialize to lba mode. */
	cmpl	$0, ABS(lba)		/* chs ? */
	jnz	10f			/* no, it is not chs.*/
	movb	$0x90, %al		/* yes, it is chs. */
10:
	stosb
	decl	%edi
2:
	addl	$(1+8+2+1+2+1+2+2+1+2), %edi
	addl	$(1+8+2+1+2+1+2+2+1+2), %esi
				/* skip 8-byte OEM name and Bytes per sector */
	
				/* and Sectors per cluster, byte */
				/* and Reserved sectors, word */
				/* and Number of FATs, byte */
				/* (Max root dir entries)Must be 0, word */
				/* (Total sectors small)Must be 0, word */
				/* Media descriptor, byte */
				/* (Sectors per FAT)Must be 0, word */
	/* sectors per track, word */
	lodsw
	addl	$2, %edi
	cmpl	$0xffffff3f, ABS(sectors_per_track)
	je	2f
	movb	ABS(sectors_per_track), %al
	cbw
	subl	$2, %edi
	stosw
2:
	
	/* number of heads, word */
	lodsw
	addl	$2, %edi
	cmpl	$0xffff00ff, ABS(heads)
	je	2f
	movw	ABS(heads), %ax
	subl	$2, %edi
	stosw
2:
	
	/* hidden sectors(i.e., partition start), dword */
	lodsl
	addl	$4, %edi
	cmpl	$0xffffffff, ABS(start_sector)
	je	2f
	movl	ABS(start_sector), %eax
	subl	$4, %edi
	stosl
2:	
	
	/* total sectors, dword */
	lodsl
	addl	$4, %edi
	cmpl	$0, ABS(total_sectors)
	je	2f
	movl	ABS(total_sectors), %eax
	subl	$4, %edi
	stosl
2:
	
	/* drive number, byte, etc... */

	addl	$(1+1+1+4+11+8), %esi
	addl	$(1+1+1+4+11+8), %edi

	//movl	$0xFFB6FCFA, %eax	/* CLI, CLD, MOV DH,FF */
	lodsw
	stosw
	lodsb
	stosb
	lodsb
	
	//decl	%edi

	/* partition number, byte */

	movb	ABS(floppy), %al
	stosb
	
	/* Machine code begins at offset 0x42, 444 bytes without 
	 * the ending 2 bytes of boot signature */

	//movl	$ABS(_start1 + 0x642), %esi
	movl	$(444 / 4), %ecx
	repz movsl
	
	movl	$1, ABS(sectors_to_write)
	
	movl	$ABS(msg_fstype_fat16_allow), %ecx
	call	8f	/* linux_print */
	
	clc
	popal
	ret
1:

	movl	$0, ABS(sectors_to_write)
	
	movl	$ABS(msg_unsupported_fstype), %ecx
	call	8f	/* linux_print */
	
	stc
	popal
	ret

//----------------------------------------------------------------------------
//print decimal number in EAX
6:
	/* print EAX value in decimal format */
	pushal

#if 0
	cld
	movl	$ABS(number_digits), %edi
	xchgl	%eax, %edx	// save EAX to EDX
	movw	$0x2020, %ax	// two spaces
	stosw
	stosw
	stosw
	stosw
	stosw
	decl	%edi		// EDI points to the last digit
	xchgl	%eax, %edx	// restore EAX from EDX
#endif
	std
	movl	$ABS(number_digits + 9), %edi
	movl	$10, %ebp
1:
	xorl	%edx, %edx
	divl	%ebp		// quo=EAX, rem=EDX
	xchgl	%eax, %edx
	orb	$0x30, %al
	stosb
	xchgl	%eax, %edx
	testl	%eax, %eax
	jnz	1b

	/* all digits are stored */
	incl	%edi		// points to the digit string

	movl	%edi, %ecx
	call	8f	/* linux_print */

	cld
	popal
	ret

//----------------------------------------------------------------------------
//parse_number:
7:

	/* input:	EDI	points to the number
	 *		EBX	upper limit of the number
	 * output:	EBX	the value of the number
	 *		EDX	changed
	 *		ESI	changed
	 *		EBP	changed
	 *		CF=1	failure
	 *		CF=0	success
	 */

	pushl	%eax
	pushl	%ebx		/* upper limit */
	cld
	movl	%edi, %esi
	movl	$0xffffffff, %edx
	xorl	%ebx, %ebx
	lodsw
	cmpw	$0x5830, %ax	/* 0X */
	je	1f
	cmpw	$0x7830, %ax	/* 0x */
	je	1f

	/* decimal */
	decl	%esi
	decl	%esi
	movl	$10, %ebp
2:
	xorl	%eax, %eax
	lodsb
	cmpb	$0, %al
	je	4f		/* CF=0 */
	cmpb	$0x30, %al
	jb	3f
	cmpb	$0x39, %al
	ja	3f
	subb	$0x30, %al
	pushl	%eax
	movl	%ebx, %eax
	mull	%ebp
	movl	%eax, %ebx
	popl	%eax
	jc	3f
	addl	%eax, %ebx
	jc	3f
	popl	%eax
	pushl	%eax		/* upper limit */
	cmpl	%eax, %ebx
	ja	3f
	jmp	2b
1:

	/* hex */
	movl	$16, %ebp
2:
	xorl	%eax, %eax
	lodsb
	cmpb	$0, %al
	je	4f		/* CF=0 */
	cmpb	$0x30, %al
	jb	3f
	cmpb	$0x39, %al
	jbe	1f
	orb	$0x20, %al
	cmpb	$0x61, %al
	jb	3f
	cmpb	$0x66, %al
	ja	3f
	subb	$0x27, %al
1:
	subb	$0x30, %al
	pushl	%eax
	movl	%ebx, %eax
	mull	%ebp
	movl	%eax, %ebx
	popl	%eax
	jc	3f
	addl	%eax, %ebx
	jc	3f
	popl	%eax
	pushl	%eax		/* upper limit */
	cmpl	%eax, %ebx
	ja	3f
	jmp	2b

3:
	stc
	popl	%eax		/* upper limit */
	popl	%eax
	ret
4:
	testl	%edx, %edx	/* CF=0 */
	jnz	3b
	popl	%eax		/* upper limit */
	popl	%eax
	ret

//----------------------------------------------------------------------------
//linux_print:
8:

	/* print a message.
	 * input:	ECX	points to message string
	 * output:	message is displayed on the stdout device
	 */

	/* ssize_t write(int fd, const void *buf, size_t count) */

	pushal

#ifdef __DOS_16

	cld
	movl	%ecx, %esi
//	movw	%si, %dx
//	movb	$0x09, %ah		// WRITE STRING TO STANDARD OUTPUT
//	int	$0x21			// call DOS
//	ret

1:      
	lodsb	(%si), %al		// get token
	cmpb	$0, %al			// end of string?
	je	1f
#if 0
	/* use BIOS independent of DOS */
	xorw	%bx, %bx		// video page 0
	movb	$0x0e, %ah		// print it
	int	$0x10			// via TTY mode
#else
	/* use stdout for redirection of the output */
	movb	%al, %dl		// character to write
	movb	$0x02, %ah		// WRITE CHARACTER TO STANDARD OUTPUT
	int	$0x21			// call DOS
#endif
	jmp	1b			// until done
1:

#else
	pushl	%ecx
	movl	%ecx, %edi	# scan the message string ...
	movl	$(msg_end - msg_start), %ecx
	movb	$0, %al		# ... find the ending zero byte

	cld
	repnz scasb

	movl	%edi, %edx
	popl	%ecx		# ECX points to the message
	subl	%ecx, %edx	# EDX=message length
	decl	%edx		# discard the ending zero byte

	movl	$4, %eax        # sys_write
	movl	$1, %ebx        # file descriptor = stdout
	int	$0x80
#endif

	popal
	ret

//----------------------------------------------------------------------------
//probe_geometry:
9:
	pushal

	addl	$0x200, %esi
	
	movw	$0, ABS(Cmax)
	movl	$0, ABS(Hmax)
	movw	$0, ABS(Smax)

	movl	$0, ABS(i)			# partition number
1:
	cmpl	$4, ABS(i)
	jae	1f

	/* the boot indicator must be 0x80(bootable) or 0(non-bootable) */

	movl	ABS(i), %ebx
	movl	ABS(i), %edi
	shll	$4, %ebx		# EBX = i * 16
	movb	-66(iBX, iSI), %al	# boot_indicator
	shlb	$1, %al
	jnz	6f	/* geometry_probe_failed */

	/* check if the entry is empty, i.e., all the 16 bytes are 0 */

	movl	-66(iBX, iSI), %eax
	orl	-62(iBX, iSI), %eax
	orl	-58(iBX, iSI), %eax
	orl	-54(iBX, iSI), %eax
	jz	2f			# empty partition entry

	/* valid partitions never start at 0, because this is where the MBR
	 * lives; and more, the number of total sectors should be non-zero.
	 */

	cmpl	$0, -58(iBX, iSI)	# start_lba
	jz	6f	/* geometry_probe_failed */

	cmpl	$0, -54(iBX, iSI)	# total_sectors
	jz	6f	/* geometry_probe_failed */

	/* the partitions should not overlap each other */

	movl	$-1, ABS(j)		// j = -1
3:
	// for (j = 0; j < i; j++)
	incl	ABS(j)			// j++
	cmpl	%edi, ABS(j)		# i = EDI
	jae	3f			// j >= i, break.

	movl	-58(iBX, iSI), %ecx	# ECX=P[i].start_lba

	movl	ABS(j), %edx
	shll	$4, %edx		# EDX = j * 16
	MOVL	-58(%edx, %esi), %eax	# EAX=P[j].start_lba

	// if ((P[j].start_lba <= P[i].start_lba) && (P[j].start_lba + P[j].total_sectors >= P[i].start_lba + P[i].total_sectors))

	cmpl	%ecx, %eax
	ja	4f

	pushl	%ecx
	pushl	%eax

	addl	-54(iBX, iSI), %ecx	// ECX = P[i].start_lba + P[i].total_sectors
	ADDL	-54(%edx, %esi), %eax	// EAX = P[j].start_lba + P[j].total_sectors

	cmpl	%ecx, %eax

	popl	%eax
	popl	%ecx

	jb	4f

	jmp	3b			//continue
4:
	// if ((P[j].start_lba >= P[i].start_lba) && (P[j].start_lba + P[j].total_sectors <= P[i].start_lba + P[i].total_sectors))
	
	cmpl	%ecx, %eax
	jb	4f

	pushl	%ecx
	pushl	%eax

	addl	-54(iBX, iSI), %ecx	// ECX = P[i].start_lba + P[i].total_sectors
	ADDL	-54(%edx, %esi), %eax	// EAX = P[j].start_lba + P[j].total_sectors

	cmpl	%ecx, %eax

	popl	%eax
	popl	%ecx

	ja	4f

	jmp	3b			//continue
4:

	cmpl	%ecx, %eax
	jae	4f

	// P[j].start_lba < P[i].start_lba
	subl	%eax, %ecx		// ECX = P[i].start_lba - P[j].start_lba
	CMPL	-54(%edx, %esi), %ecx	# ECX < P[j].total_sectors
	jmp	5f
4:
	// P[j].start_lba >= P[i].start_lba
	subl	%ecx, %eax		// EAX = P[j].start_lba - P[i].start_lba
	cmpl	-54(iBX, iSI), %eax	# EAX < P[i].total_sectors
5:
	jb	6f	/* geometry_probe_failed */

	jmp	3b			//continue
3:

	/* the start cylinder number */

	movzwl	-64(iBX, iSI), %eax	# start_sector_cylinder

	movl	%eax, %edx
	andw	$0x003f, %dx
	jz	6f	/* geometry_probe_failed */

	movw	%dx, ABS(X)			# the sector number

	cmpw	%dx, ABS(Smax)
	jae	3f
	movw	%dx, ABS(Smax)
3:
	movl	-58(iBX, iSI), %ecx	# start_lba
	incl	%ecx
	subl	%edx, %ecx

	movl	$ABS(L), %edx
	MOVL	%ecx, (%edx, %edi, 8)	# L[i]

	///* partitions should not start at the first track, the MBR-track */
	//movw	ABS(Smax), %dx
	//cmpw	%dx, -58(%ebx, %esi)	# start_lba
	//jb	6f	/* geometry_probe_failed */

	shrb	$6, %al
	xchgb	%al, %ah

	movl	$ABS(C), %edx
	MOVW	%ax, (%edx, %edi, 4)	# C[i]

	cmpw	%ax, ABS(Cmax)
	jae	3f
	movw	%ax, ABS(Cmax)
3:

	//movl	ABS(i), %ebx
	//shll	$4, %ebx		# EBX = ABS(i) * 16
	movzbl	-65(iBX, iSI), %eax	# start_head
	movl	$ABS(H), %edx
	MOVL	%eax, (%edx, %edi, 4)	# H[i]

	cmpl	%eax, ABS(Hmax)
	jae	3f
	movl	%eax, ABS(Hmax)
3:

	/* the end cylinder number */

	movzwl	-60(iBX, iSI), %eax	# end_sector_cylinder

	movl	%eax, %edx
	andw	$0x003f, %dx
	jz	6f	/* geometry_probe_failed */

	movw	%dx, ABS(Y)			# the sector number

	cmpw	%dx, ABS(Smax)
	jae	3f
	movw	%dx, ABS(Smax)
3:
	movl	-58(iBX, iSI), %ecx	# start_lba
	addl	-54(iBX, iSI), %ecx	# total_sectors
					# XXX: possible overflow!

	subl	%edx, %ecx
	jb	6f	/* geometry_probe_failed */

	addl	$4, %edi		# EDI=i+4

	movl	$ABS(L), %edx
	MOVL	%ecx, (%edx, %edi, 8)	# L[i+4]

	movl	%ecx, %ebp		# save ECX to EBP

	/* partitions should not start at the first track, the MBR-track */
	movw	ABS(Smax), %dx
	cmpw	%dx, -58(iBX, iSI)	# start_lba
	jb	6f	/* geometry_probe_failed */

	shrb	$6, %al
	xchgb	%al, %ah

	movl	$ABS(C), %edx
	MOVW	%ax, (%edx, %edi, 4)	# C[i+4]

	cmpw	%ax, ABS(Cmax)
	jae	3f
	movw	%ax, ABS(Cmax)
3:
	xchgl	%eax, %ecx		# save AX to CX

	//movl	ABS(i), %ebx
	//shll	$4, %ebx		# EBX = ABS(i) * 16
	movzbl	-61(iBX, iSI), %eax	# end_head
	movl	$ABS(H), %edx
	MOVL	%eax, (%edx, %edi, 4)	# H[i+4]

	cmpl	%eax, ABS(Hmax)
	jae	3f
	movl	%eax, ABS(Hmax)
3:

	/* Check the large disk partition -- Win98 */
	cmpw	$63, ABS(Y)
	jne	3f
	cmpl	%eax, ABS(Hmax)		# EAX=H[i+4]
	jne	3f
	cmpw	%cx, ABS(Cmax)		# ECX=C[i+4]
	jne	3f

	cmpl	$254, %eax		# EAX=ABS(Hmax)
	jnb	4f
	cmpw	$1022, %cx		# CX=ABS(Cmax)
	jb	3f
4:
	movl	$63, %edx		# EDX=ABS(Y)
	addl	%edx, %ebp
	incl	%eax
	mull	%edx			# EDX:EAX=product
					# EDX=0
					# EAX high word = 0
					# AX=(Hmax+1)*63
	pushl	%eax			# EAX=(Hmax+1)*63
	incl	%ecx
	mull	%ecx			# EDX:EAX=product
					# EDX=0
	cmpl	%ebp, %eax
	popl	%eax			# EAX=(Hmax+1)*63
	jnb	3f

	/* EDI=i+4 at this moment */

	pushl	%eax			# EAX=(Hmax+1)*63
	subl	$4, %edi		# EDI=i
	
	movl	$ABS(C), %ebx
	MOVZWL	(%ebx, %edi, 4), %eax	# C[i]
	
	movl	ABS(Hmax), %ebp		# Hmax
	incl	%ebp

	pushl	%eax			# EAX=C[i]

	movl	$ABS(H), %ebx
	MOVL	(%ebx, %edi, 4), %ecx	# H[i]

	mull	%ebp			# EDX=0, EAX=product
	addl	%ecx, %eax
	movl	$63, %edx
	mull	%edx			# EDX=0, EAX=product
	
	movl	$ABS(L), %ebx
	MOVL	(%ebx, %edi, 8), %ebp	# L[i]

	cmpl	%ebp, %eax
	popl	%eax			# EAX=C[i]

	jae	4f
	
	/* calculate CHS numbers from start LBA */

	//pushl	%eax			# EAX=C[i]

	xorl	%edx, %edx
	xchgl	%eax, %ebp		# EBP=C[i], EDX:EAX=L[i]
	
	divl	ABS(Y)			# Y=63
					# quo=EAX, rem=EDX
	incl	%edx			# sector number
	cmpw	%dx, ABS(X)
	je	15f
	cmpw	$63, ABS(X)
	jne	4f
15:
	xorl	%edx, %edx
	movl	ABS(Hmax), %ebx
	incl	%ebx			# EBX=Hmax+1
	divl	%ebx			# quo=EAX=cylinder number
					# rem=EDX=head number
	cmpl	%edx, %ecx		# ECX=H[i]
	je	15f
	decl	%ebx			# EBX=Hmax
	cmpl	%ebx, %ecx
	jne	4f
15:
	/* EAX=calculated cylinder number, EBP=C[i] */

	cmpl	ABS(Cmax), %ebp
	je	4f
	andw	$0x3ff, %ax		# low 10 bit of calculated cylinder
	cmpw	%ax, %bp
//5:
//	cmpw	ABS(Cmax), %ax
//	jne	4f
//	cmpl	ABS(Hmax), %ecx
//	jne	4f
//	cmpw	$63, ABS(X)
4:
	popl	%eax
	jne	6f	/* geometry_probe_failed */
	jmp	5f
3:

	/* Check the large disk partition -- Win2K */

	/* EDI=i+4 at this moment */
	cmpw	$63, ABS(Y)
	jne	3f
	movl	$ABS(H), %edx
	MOVL	(%edx, %edi, 4), %eax	# H[i+4]
	cmpl	%eax, ABS(Hmax)		# EAX=H[i+4]
	jne	3f
	
	movl	$ABS(L), %edx
	MOVL	(%edx, %edi, 8), %ebp	# L[i+4]
	
	movl	$ABS(C), %edx
	MOVZWL	(%edx, %edi, 4), %ecx	# C[i+4]

	incl	%ecx
	incl	%eax
	
	mull	%ecx			# EDX=0, EAX=product
	mull	ABS(Y)			# Y=63, EDX=0, EAX=product
	addl	ABS(Y), %ebp
	cmpl	%ebp, %eax
	jnb	3f

	movl	ABS(Hmax), %eax
	incl	%eax
	mull	ABS(Y)			# Y=63, EDX=0, EAX=product
	
	xorl	%edx, %edx
	xchgl	%eax, %ebp		# EBP=(Hmax+1)*63
					# EAX=L[i+4]+Y

	divl	%ebp			# EAX=quo, EDX=rem
	testl	%edx, %edx
	jnz	3f
	decl	%eax
	andl	$0x3ff, %eax
	decl	%ecx
	cmpl	%eax, %ecx
	jne	3f
	
	/* EDI=i+4 at this moment */

	subl	$4, %edi		# EDI=i

	movl	$ABS(C), %ebx
	MOVL	(%ebx, %edi, 4), %eax	# C[i]

	pushl	%eax
	
	mull	%ebp			# EBP=(Hmax+1)*63
					# EDX=0, EAX=product
	
	pushl	%ebp			# EBP=(Hmax+1)*63
	
	xchgl	%eax, %ebp		# EBP=product
	movl	$ABS(H), %ebx
	MOVL	(%ebx, %edi, 4), %eax	# H[i]
	mull	ABS(Y)			# Y=63, EDX=0, EAX=H[i]*63

	pushl	%eax

	addl	%eax, %ebp
	
	movl	$ABS(L), %ebx
	MOVL	(%ebx, %edi, 8), %eax	# L[i]
	cmpl	%eax, %ebp

	popl	%ebx			# EBX=H[i]*63
	popl	%ebp			# EBP=(Hmax+1)*63
	popl	%ecx			# ECX=C[i]
	xchgl	%eax, %ebp

	ja	6f	/* geometry_probe_failed */
	je	5f
	xchgl	%eax, %ebp
	subl	%ebx, %eax
	xorl	%edx, %edx
	divl	%ebp			# EAX=quo, EDX=rem
	testl	%edx, %edx
	jnz	6f	/* geometry_probe_failed */
	andl	$0x3ff, %eax
	cmpl	%eax, %ecx
	jne	6f	/* geometry_probe_failed */
	
	xchgl	%eax, %ebp
	jmp	5f
3:
	/* Maximum of C[n] * (H * S) + H[n] * S = 1023 * 255 * 63 + 254 * 63 = 0xFB03C1 */

	/* EDI=i+4 at this moment */
	movl	$ABS(L), %edx
	MOVL	(%edx, %edi, 8), %ebp	# L[i+4]
	cmpl	$0xFB03C1, %ebp
	jbe	3f

	/* set H/S to max */
	cmpl	$254, ABS(Hmax)
	jae	4f
	movl	$254, ABS(Hmax)
4:
	movw	$63, ABS(Smax)
	xorl	%edx, %edx
	movl	%ebp, %eax
	divl	ABS(Smax)
	testl	%edx, %edx
	jnz	6f	/* geometry_probe_failed */

	/* EAX=L[i+4]/63, EDX=0 */

	movl	$ABS(H), %ebx
	MOVL	(%ebx, %edi, 4), %ebx	# EBX=H[i+4]
	movl	ABS(Hmax), %ecx
	incl	%ecx			# ECX=Hmax+1
	divl	%ecx
	cmpl	%edx, %ebx
	je	15f
	cmpl	ABS(Hmax), %ebx
	jne	6f	/* geometry_probe_failed */
15:
	/* EAX=L[i+4]/63/(Hmax+1) */
	andl	$0x3FF, %eax
	movl	$ABS(C), %edx
	MOVZWL	(%edx, %edi, 4), %ebx	# C[i+4]
	cmpl	%eax, %ebx
	je	15f
	cmpl	ABS(Cmax), %ebx
	jne	6f	/* geometry_probe_failed */
15:
	movl	$63, %eax
	mull	%ecx

	/* EDI=i+4 at this moment */

	pushl	%eax			# EAX=(Hmax+1)*63
	subl	$4, %edi		# EDI=i
	
	movl	$ABS(C), %ebx
	MOVZWL	(%ebx, %edi, 4), %eax	# C[i]
	
	movl	ABS(Hmax), %ebp		# Hmax
	incl	%ebp

	pushl	%eax			# EAX=C[i]

	movl	$ABS(H), %ebx
	MOVL	(%ebx, %edi, 4), %ecx	# H[i]

	mull	%ebp			# EDX=0, EAX=product
	addl	%ecx, %eax
	movl	$63, %edx
	mull	%edx			# EDX=0, EAX=product
	
	movl	$ABS(L), %ebx
	MOVL	(%ebx, %edi, 8), %ebp	# L[i]

	cmpl	%ebp, %eax
	popl	%eax			# EAX=C[i]

	jae	4f
	
	/* calculate CHS numbers from start LBA */

	//pushl	%eax			# EAX=C[i]

	xorl	%edx, %edx
	xchgl	%eax, %ebp		# EBP=C[i], EDX:EAX=L[i]
	
	divl	ABS(Y)			# Y=63
					# quo=EAX, rem=EDX
	incl	%edx			# sector number
	cmpw	%dx, ABS(X)
	je	15f
	cmpw	$63, ABS(X)
	jne	4f
15:
	xorl	%edx, %edx
	movl	ABS(Hmax), %ebx
	incl	%ebx			# EBX=Hmax+1
	divl	%ebx			# quo=EAX=cylinder number
					# rem=EDX=head number
	cmpl	%edx, %ecx		# ECX=H[i]
	je	15f
	decl	%ebx			# EBX=Hmax
	cmpl	%ebx, %ecx
	jne	4f
15:
	/* EAX=calculated cylinder number, EBP=C[i] */

	cmpl	ABS(Cmax), %ebp
	je	4f
	andw	$0x3ff, %ax		# low 10 bit of calculated cylinder
	cmpw	%ax, %bp
//5:
//	cmpw	ABS(Cmax), %ax
//	jne	4f
//	cmpl	ABS(Hmax), %ecx
//	jne	4f
//	cmpw	$63, ABS(X)
4:
	popl	%eax
	jne	6f	/* geometry_probe_failed */
	//jmp	5f
5:

	/* EAX = (Hmax+1)*63 */

	movl	$ABS(C), %ebx
	MOVW	$0, (%ebx, %edi, 4)	# C[i]=0

	movl	$ABS(H), %ebx
	MOVL	$1, (%ebx, %edi, 4)	# H[i]=1

	movl	$ABS(L), %ebx
	MOVL	$63, (%ebx, %edi, 8)	# L[i]=63

	addl	$4, %edi		# EDI=i+4
	
	movl	$ABS(C), %ebx
	MOVW	$1, (%ebx, %edi, 4)	# C[i+4]=1

	movl	$ABS(H), %ebx
	MOVL	$0, (%ebx, %edi, 4)	# H[i+4]=0

	movl	$ABS(L), %ebx
	MOVL	%eax, (%ebx, %edi, 8)	# L[i+4]=(Hmax+1)*63
3:
	jmp	3f

2:	/* else */

	/* empty entry, zero all the coefficients */

	movl	$ABS(C), %ebx
	MOVW	$0, (%ebx, %edi, 4)	# C[i]=0

	movl	$ABS(H), %ebx
	MOVW	$0, (%ebx, %edi, 4)	# H[i]=0

	movl	$ABS(L), %ebx
	MOVL	$0, (%ebx, %edi, 8)	# L[i]=0

	addl	$4, %edi
	movl	$ABS(C), %ebx
	MOVW	$0, (%ebx, %edi, 4)	# C[i+4]=0

	movl	$ABS(H), %ebx
	MOVW	$0, (%ebx, %edi, 4)	# H[i+4]=0

	movl	$ABS(L), %ebx
	MOVL	$0, (%ebx, %edi, 8)	# L[i+4]=0

3:

	incl	ABS(i)
	jmp	1b
1:

	/* all coefficients are generated ok */

# --------------------------------------------------------------------

#if 0

	/* print the coefficients */
	
	xorl	%edi, %edi
1:
	cmpl	$4, %edi
	jae	1f

	/* print C[i] */
	movl	$ABS(C), %ebx
	MOVL	(%ebx, %edi, 4), %eax		# C[i]

	call	6b	//print decimal number
	
	/* print spaces */
	movl	$ABS(delimit_space), %ecx
	call	8b	/* linux_print */
	
	/* print H[i] */
	movl	$ABS(H), %ebx
	MOVL	(%ebx, %edi, 4), %eax		# H[i]

	call	6b	//print decimal number
	
	/* print spaces */
	movl	$ABS(delimit_space), %ecx
	call	8b	/* linux_print */
	
	/* print L[i] */
	movl	$ABS(L), %ebx
	MOVL	(%ebx, %edi, 8), %eax		# L[i]

	call	6b	//print decimal number
	
	/* print spaces */
	movl	$ABS(delimit_space), %ecx
	call	8b	/* linux_print */
	
	addl	$4, %edi

	/* print C[i] */
	movl	$ABS(C), %ebx
	MOVL	(%ebx, %edi, 4), %eax		# C[i+4]

	call	6b	//print decimal number
	
	/* print spaces */
	movl	$ABS(delimit_space), %ecx
	call	8b	/* linux_print */
	
	/* print H[i] */
	movl	$ABS(H), %ebx
	MOVL	(%ebx, %edi, 4), %eax		# H[i+4]

	call	6b	//print decimal number
	
	/* print spaces */
	movl	$ABS(delimit_space), %ecx
	call	8b	/* linux_print */
	
	/* print L[i] */
	movl	$ABS(L), %ebx
	MOVL	(%ebx, %edi, 8), %eax		# L[i+4]

	call	6b	//print decimal number
	
	/* print spaces */
	movl	$ABS(delimit_newline), %ecx
	call	8b	/* linux_print */
	
	subl	$4, %edi
	
	incl	%edi
	jmp	1b
1:

#endif
	
# --------------------------------------------------------------------

	xorl	%edi, %edi			# equation number ABS(i)
1:
	cmpl	$8, %edi
	jae	1f

	movl	$ABS(C), %ebx
	CMPW	$0, (%ebx, %edi, 4)		# C[i]

	jne	1f

	incl	%edi
	jmp	1b
1:

	cmpl	$8, %edi
	jb	1f

	/* all C[i] == 0 */

	xorl	%edi, %edi			# equation number ABS(i)
	movl	$ABS(H), %ebx
2:
	cmpl	$8, %edi
	jae	2f

	MOVL	(%ebx, %edi, 4), %ecx		# H[i]
	testl	%ecx, %ecx

	jne	2f

	incl	%edi
	jmp	2b
2:

	cmpl	$8, %edi
	je	6f	/* geometry_probe_failed */	# all H[i] == 0

	xorl	%edx, %edx			# equation number j
	movl	$ABS(L), %ebx
2:
	cmpl	%edi, %edx
	jae	2f

	CMPW	$0, (%ebx, %edx, 8)		# L[j]

	jne	6f	/* geometry_probe_failed */

	incl	%edx
	jmp	2b
2:

	/* j == i */

	MOVL	(%ebx, %edi, 8), %eax		# L[i]
	testl	%eax, %eax

	jle	6f	/* geometry_probe_failed */
						# OF=0; jmp if SF=1 or ZF=1

	cdq					# EDX=0
	divl	%ecx				# ECX=H[i]
						# EAX=quo=sectors_per_track
						# EDX=rem
	testl	%edx, %edx
	jnz	6f	/* geometry_probe_failed */

	movl	%eax, ABS(probed_sectors_per_track)

	cmpl	$63, %eax
	ja	6f	/* geometry_probe_failed */

	cmpw	ABS(Smax), %ax
	jb	6f	/* geometry_probe_failed */

	movw	%ax, ABS(Smax)

	movl	%edi, %edx
	incl	%edx				# equation number j=i+1
2:
	cmpl	$8, %edx
	jae	2f

	movl	$ABS(H), %ebx
	MOVL	(%ebx, %edx, 4), %ecx		# H[j]

	movl	$ABS(L), %ebx
	MOVL	(%ebx, %edx, 8), %eax		# L[j]

	jecxz	3f

	pushl	%edx

	xchgl	%eax, %ecx			# EAX=H[j], ECX=L[j]
	mull	ABS(probed_sectors_per_track)	# EAX=product
						# EDX=0
	popl	%edx

	cmpl	%eax, %ecx

	jnz	6f	/* geometry_probe_failed */

	jmp	4f
3:
	testl	%eax, %eax
	jnz	6f	/* geometry_probe_failed */
4:

	incl	%edx
	jmp	2b
2:

	movl	ABS(Hmax), %eax
	incl	%eax
	movl	%eax, ABS(probed_heads)

	jmp	2f
1:

	testl	%edi, %edi
	jz	1f

	/* xchgw C[i], C[0] */

	movl	$ABS(C), %ebx
	MOVW	(%ebx, %edi, 4), %ax		# C[i]
	movw	(iBX), %cx			# C[0]

	movw	%ax, (iBX)
	MOVW	%cx, (%ebx, %edi, 4)

	/* xchgw H[i], H[0] */

	movl	$ABS(H), %ebx
	MOVL	(%ebx, %edi, 4), %eax		# H[i]
	movl	(iBX), %ecx			# H[0]

	movl	%eax, (iBX)
	MOVL	%ecx, (%ebx, %edi, 4)

	/* xchgw L[i], L[0] */

	movl	$ABS(L), %ebx
	MOVL	(%ebx, %edi, 8), %eax		# L[i]
	movl	(iBX), %ecx			# L[0]

	movl	%eax, (iBX)
	MOVL	%ecx, (%ebx, %edi, 8)

1:

	movl	$0, ABS(H8)			# will store sectors per track

	/* for (i = 1; i < 8; i++) */

	movl	$1, %edi
1:
	cmpl	$8, %edi
	jae	1f

	movl	$ABS(L), %ebx
	MOVL	(%ebx, %edi, 8), %eax		# L[i]

	movzwl	ABS(C), %edx			# C[0]
	mull	%edx				# EDX:EAX=product

	MOVL	%eax, (%ebx, %edi, 8)		# L[i] low
	MOVL	%edx, 4(%ebx, %edi, 8)		# L[i] high

	movl	$ABS(C), %ebx
	MOVZWL	(%ebx, %edi, 4), %eax		# C[i]

	mull	ABS(L)				# L[0]

	movl	$ABS(L), %ebx
	SUBL	%eax, (%ebx, %edi, 8)		# L[i] low
	SBBL	%edx, 4(%ebx, %edi, 8)		# L[i] high

	movl	$ABS(H), %ebx
	MOVL	(%ebx, %edi, 4), %eax		# H[i]

	movzwl	ABS(C), %edx			# C[0]
	mull	%edx				# EDX:EAX=product
						# EDX=0

	MOVL	%eax, (%ebx, %edi, 4)		# H[i]

	movl	$ABS(C), %ebx
	MOVZWL	(%ebx, %edi, 4), %eax		# C[i]

	mull	ABS(H)				# H[0]

	movl	$ABS(H), %ebx
	SUBL	%eax, (%ebx, %edi, 4)		# H[i]

	/* if (H[i]) */

	jz	3f

	ja	4f

	movl	$ABS(H), %ebx
	NEGL	(%ebx, %edi, 4)			# H[i]

	movl	$ABS(L), %ebx
	xorl	%eax, %eax
	NEGL	(%ebx, %edi, 8)			# L[i] low
	cmc
	NOTL	4(%ebx, %edi, 8)		# L[i] high
	ADCL	%eax, 4(%ebx, %edi, 8)

4:
	movl	$ABS(L), %ebx
	CMPL	$0, 4(%ebx, %edi, 8)		# L[i] high
	jne	6f	/* geometry_probe_failed */

	MOVL	(%ebx, %edi, 8), %eax		# L[i] low
	testl	%eax, %eax

	jle	6f	/* geometry_probe_failed */
						# OF=0; jmp if SF=1 or ZF=1

	cdq					# EDX=0

	movl	$ABS(H), %ebx
	DIVL	(%ebx, %edi, 4)			# H[i]
						# EAX=sectors per track
	testl	%edx, %edx
	jnz	6f	/* geometry_probe_failed */

	movl	%eax, ABS(L8)

	cmpl	$63, %eax
	ja	6f	/* geometry_probe_failed */

	cmpw	ABS(Smax), %ax
	jb	6f	/* geometry_probe_failed */

	movw	%ax, ABS(Smax)

	cmpl	$0, ABS(H8)
	je	4f

	cmpl	%eax, ABS(H8)			# EAX=L[8]
	jne	6f	/* geometry_probe_failed */

	jmp	5f
4:
	movl	%eax, ABS(H8)			# EAX=L[8]
5:

	jmp	4f
3:	/* else */
	movl	$ABS(L), %ebx
	CMPL	$0, (%ebx, %edi, 8)		# L[i] low
	jne	6f	/* geometry_probe_failed */

	CMPL	$0, 4(%ebx, %edi, 8)		# L[i] high
	jne	6f	/* geometry_probe_failed */

4:	/* end if (H[i]) */

	incl	%edi
	jmp	1b
1:	/* end for (i = 1; i < 8; i++) */

	/* if (ABS(H)[8]) */

	cmpl	$0, ABS(H8)
	je	3f

	/* ABS(H)[8] is sectors per track */

	movl	ABS(H), %eax				# H[0]
	mull	ABS(H8)

	//movl	$ABS(L), %ebx
	subl	%eax, ABS(L)				# L[0] low
	sbbl	%edx, ABS(L) + 4			# L[0] high

	jnz	6f	/* geometry_probe_failed */

	movl	ABS(L), %eax
	testl	%eax, %eax

	jle	6f	/* geometry_probe_failed */
						# OF=0; jmp if SF=1 or ZF=1

	cdq					# EDX=0

	divl	ABS(H8)

	testl	%edx, %edx
	jnz	6f	/* geometry_probe_failed */

	movzwl	ABS(C), %ecx				# C[0]
	divl	%ecx

	testl	%edx, %edx
	jnz	6f	/* geometry_probe_failed */

	movl	%eax, ABS(L8)			# L[8] is number of heads

	cmpl	$256, %eax
	ja	6f	/* geometry_probe_failed */

	cmpl	ABS(Hmax), %eax
	jbe	6f	/* geometry_probe_failed */

	movl	ABS(H8), %eax
	movl	%eax, ABS(probed_sectors_per_track)

	jmp	4f
3:	/* else */

	/* fail to set L[8], this means all H[i]==0, i=1,2,3,4,5,6,7 */

	/* Now the only equation is: C[0] * H * S + H[0] * S = L[0] */

	/* for (i = 63; i >= Smax; i--) */

	movl	ABS(L), %ebx				# EBX=L[0]
	movl	$63, %edi
3:
	cmpl	ABS(Smax), %edi
	jb	3f

	movl	ABS(H), %eax			# H[0]
	mull	%edi				# EDX=0
						# EAX=product
	//movl	%ebx, ABS(L8)
	//subl	%eax, ABS(L8)

	subl	%ebx, %eax
	negl	%eax

	jz	5f				# continue
	js	5f				# continue

	cdq					# EDX=0
	divl	%edi

	testl	%edx, %edx
	jnz	5f

	movzwl	ABS(C), %ecx			# C[0]
	divl	%ecx

	testl	%edx, %edx
	jnz	5f

	movl	%eax, ABS(L8)			# L[8] is number of heads

	cmpl	$256, %eax
	ja	5f

	cmpl	ABS(Hmax), %eax
	ja	3f
5:
	decl	%edi
	jmp	3b
3:	/* end for (i = 63; i >= Smax; i--) */

	cmpl	ABS(Smax), %edi
	jb	6f	/* geometry_probe_failed */

	movl	%edi, ABS(probed_sectors_per_track)

4:	/* end if (ABS(H)[8]) */

	movl	ABS(L8), %eax
	movl	%eax, ABS(probed_heads)

2:	/* end if (ABS(i) == 8) */

	/* geometry_probe_ok */

	clc
	popal
	ret

//geometry_probe_failed:
6:
	stc
	popal
	ret

//partition:
0:
#if 0
	/* load the next partition and modify the boot record */

	/* EBX holds the file descriptor number or 0 if it is BIOS drive */
	/* partition table is at ABS(mbr_63_sectors + 0x2000) */
	/* current partition number is ABS(current_partition) */

	pushal

	/* enumerate all partitions */
	
	/* initialize the partition table, move start_LBA(the 3rd dword)
	 * to the total_sectors field(the 4th dword) and clear the 3rd dword.
	 * The sum of the 3rd and 4th dwords gives the absolute start_LBA.
	 */
	movl	$ABS(mbr_63_sectors + 0x2000), %ebp
	xorl	%eax, %eax
	xchgl	%eax, 0x08(%ebp)
	movl	%eax, 0x0c(%ebp)
	xorl	%eax, %eax
	xchgl	%eax, 0x18(%ebp)
	movl	%eax, 0x1c(%ebp)
	xorl	%eax, %eax
	xchgl	%eax, 0x28(%ebp)
	movl	%eax, 0x2c(%ebp)
	xorl	%eax, %eax
	xchgl	%eax, 0x38(%ebp)
	movl	%eax, 0x3c(%ebp)

	movl	$0x04, ABS(total_partition_entries)
	/* copy extended partition entries to the end of the partition table */
	xorl	%esi, %esi
1:
	movb	4(%ebp, %esi), %al	# partition id
	cmpb	$0x05, %al
	je	2f
	cmpb	$0x0f, %al
	je	2f
	cmpb	$0x15, %al
	je	2f
	cmpb	$0x1f, %al
	je	2f
	cmpb	$0x85, %al
	jne	4f
2:
	/* extended partition */

	/* read one sector of the partition to ABS(mbr_63_sectors) */
	........
	/* check if the partition table is valid */
	j_invalid	3f
	/* approve the partition entries and move onto the free entries */
	movl	ABS(total_partition_entries), %edi
	shll	$4, %edi
	addl	%ebp, %edi		# EDI points to the first free entry
	/* move the extended partition entry onto the first free entry */
	movl	(%ebp, %esi), %eax
	stosl
	movl	4(%ebp, %esi), %eax
	stosl
	xorl	%eax, %eax
	stosl
	movl	8(%ebp, %esi), %eax
	stosl
	incl	ABS(total_partition_entries)
3:
	///* disable the original extended partition entry by clearing its id */
	//movb	$0, 4(%ebp, %esi)
	cmpl	$0x40, %esi		# Is it in the main partition table?
	jb	4f			# yes, keep the entry in its place.
	
					# no, remove the entry.
	pushl	%esi
	addl	%ebp, %esi
	movl	%esi, %edi
	addl	$0x10, %esi
	movl	ABS(total_partition_entries), %ecx
	shll	$4, %ecx
	subl	%esi, %ecx
	shrl	$2, %ecx
	cld
	repz movsl
	popl	%esi
	decl	ABS(total_partition_entries)
	subl	$0x10, %esi		# go back one entry and redo
4:
	addl	$0x10, %esi
	movl	ABS(total_partition_entries), %edi
	shll	$4, %edi
	cmpl	%edi, %esi
	jb	1b

	/* end of partition enumeration */
	
	/* recursively resolve extended partition entries */
	//xorl	%esi, %esi
	//movl	$4, %esi
	//cmpl	ABS(total_partition_entries), %esi
	
	
	cmpl	$3, ABS(current_partition)
	ja	1f
	/* it is primary partition */
	movl	ABS(current_partition), %esi
	shll	$4, %esi
	movl	8(%ebp, %esi), %eax	# start LBA
	movl	$ABS(msg_no_such_partition), ABS(error_message_pointer)
	testl	%eax, %eax
	stc
	jz	2f
	/* read 4 sectors at start LBA to ABS(mbr_63_sectors) */
	movl	$4, %ecx
	clc		/* read */
	call	6f
	movl	$ABS(msg_read_file), ABS(error_message_pointer)
	jc	2f
	/* modify the boot record */
	call	7f
	movl	$ABS(msg_unsupported_fstype), ABS(error_message_pointer)
	jc	2f
	/* write sectors to start LBA at ABS(mbr_63_sectors) */
	movl	ABS(sectors_to_write), %ecx
	stc		/* write */
	call	6f
	movl	$ABS(msg_write_file), ABS(error_message_pointer)
2:
	popal
	ret

1:
	/* it is logical partition */
	movl	$ABS(msg_end_of_partition_chain), ABS(error_message_pointer)
	popal
	ret
6:
	/* before the call: */
	/* EAX=start LBA */
	/* EBX=file descriptor or 0 for BIOS drive */
	/* ECX=sectors */
	/* CF=0 for read; CF=1 for write */
	/* on return: */
	/* CF=0 for success; CF=1 for failure */

	pushal
	
	popal
	ret
7:
	pushal
	
	popal
	ret
	/* EBX holds the file descriptor number or 0 if it is BIOS drive */
	/* move partition table forward to the beginning of the 17th sector */
	movl	$ABS(mbr_63_sectors + 0x01be), %esi
	movl	$ABS(mbr_63_sectors + 0x2000), %edi
	movl	$0x10, %ecx	# 0x10 dwords = 0x40 bytes

	cld
	repz movsl

	/* initialize the current_partition number */
	movzbl	ABS(install_partition), %eax
	movl	%eax, ABS(current_partition)

3:
	/* load the next partition and modify the boot record */
	call	0f	/* partition */
	jc	3f	/* done */
	call	2f	/* write to file */
	incl	ABS(current_partition)
	movw	ABS(install_partition), %ax
	addb	%ah, %al	/* Max partition number for install */
	cmpb	%al, ABS(current_partition)
	jbe	3b
3:
	/* all partitions have been installed. */
	
	xorl	%eax, %eax	# exit code = 0
	
#ifdef __DOS_16
	
	movb	$0x4c, %ah	// EXIT - TERMINATE WITH RETURN CODE in AL
	int	$0x21		// call DOS

#else
	xchgl	%eax, %ebx	# move exit code in EAX to EBX
	movl	$1, %eax	# sys_exit
	int	$0x80
#endif

#endif		
	ret

