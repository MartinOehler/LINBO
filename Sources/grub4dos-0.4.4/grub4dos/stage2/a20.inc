/* real-mode A20 gate control code for grub4dos.
 *
 *  Copyright (C) 2008 Tinybit <tinybit@tom.com>
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

/* The code is based on bcopy32.c from syslinux-3.63. Here is the original
 * copyright notice:
 *
 * -----------------------------------------------------------------------
 *
 *   Copyright 1994-2008 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * -----------------------------------------------------------------------
 */

#
# Routines to enable and disable (yuck) A20.  These routines are gathered
# from tips from a couple of sources, including the Linux kernel and
# http://www.x86.org/.  The need for the delay to be as large as given here
# is indicated by Donnie Barnes of RedHat, the problematic system being an
# IBM ThinkPad 760EL.
#
# We typically toggle A20 twice for every 64K transferred.
#

#define	DISABLE_CPU_CACHE	1

#define IO_DELAY_PORT	0x80		/* Invalid port (we hope!) */

# Note the skip of 2 here
#define A20_DUNNO	0		/* A20 type unknown */
#define A20_NONE	2		/* A20 always on? */
#define A20_BIOS	4		/* A20 BIOS enable */
#define A20_KBC		6		/* A20 through KBC */
#define A20_FAST	8		/* A20 through port 92h */

enable_disable_a20:

	###################################################################
	# input:	DL=0		disable a20
	#		DL=non-zero	enable a20
	#		DH=0		a20 debug off
	#		DH=non-zero	a20 debug on
	#		CX=loops to try when failure
	#
	# output:	ZF=0		failed
	#		ZF=1		completed ok. If ZF=CF=1, then
	#				the A20 status needn't change and
	#				was not touched.
	#		EAX modified
	#		CX modified
	###################################################################

	# First, see if the A20 status is already what we desired.
	pushl	%ecx
	movl	$0x2, %ecx
	call	a20_test_match
	popl	%ecx
	/* ZF=1(means equal) for desired and we needn't do anything. */
	jnz	1f
	stc
	ret

assign_base_pointer:
	call	base_addr
base_addr:
	popw	%bp
	ret
1:
	/********************************************/
	/**  Now we have to enable or disable A20  **/
	/********************************************/

	pushal			# save all

	movw	$200, %cx
	testb	%dl, %dl
	jnz	1f
	movw	$20, %cx
1:
	call	assign_base_pointer	/* BP points to base_addr */

	# Times to try to make this work
	movw	%cx, %cs:(A20Tries - base_addr)(%bp)

	/* save DX */
	movw	%dx, %cs:(A20Config - base_addr)(%bp)

	/* save original IF, DF */
	pushfw
	popw	%cs:(A20Flags - base_addr)(%bp)

	/* save original return address */
	movw	%sp, %bp
	pushw	32(%bp)
	call	assign_base_pointer	/* BP points to base_addr */
	popw	%cs:(A20ReturnAddress - base_addr)(%bp)

a20_try_again:

	######################################################################
	## If the A20 type is known, jump straight to type
	######################################################################

	call	assign_base_pointer	/* BP points to base_addr */
	movw	%cs:(A20Type - base_addr)(%bp), %si
	movw	%bp, %bx
	addw	%cs:(A20List - base_addr)(%bp, %si), %bx
	jmp	*%bx

	######################################################################
	## First, see if we are on a system with no A20 gate
	######################################################################
	/*
	 * If the system has no A20 gate, then we needn't enable it and could
	 * return SUCCESS right now without calling A20_TEST.
	 */
a20_none:
	testb	%dl, %dl
	jz	a20_done_fail
//	cmpb	%dl, %dl	# set ZF=1 for success
//	jmp	a20_done

a20_dunno:
	//movb	$A20_DUNNO, %cs:(A20Type - base_addr)(%bp)
	call	a20_debug_print

	pushl	%ecx
	movl	$0x2, %ecx
	call	a20_test_match
	popl	%ecx
	/* ZF=1(means equal) for desired and we needn't do anything. */
	jz	a20_done

	#######################################################
	## Next, try the BIOS (INT 15h AX=240xh)
	#######################################################
a20_bios:
#if 0
	/* dell hangs on the A20 BIOS call, so we avoid calling it. */
	
	testb	%dl, %dl
	jz	1f
	call	assign_base_pointer	/* BP points to base_addr */
	movb	$A20_BIOS, %cs:(A20Type - base_addr)(%bp)
	call	a20_debug_print
1:
	pushw	%bp		/* in case it is destroyed by int 15 */
	pushw	%dx		/* in case it is destroyed by int 15 */
	pushfw			# Some BIOSes muck with IF

	testb	%dl, %dl
	setnz	%al
	movb	$0x24, %ah

.ifdef int13_handler
.ifdef ROM_int15
	/* we are inside asm.S */
	pushfw
	lcall	%cs:*(ROM_int15 - int13_handler)
.else
	int	$0x15
.endif
.else
	int	$0x15
.endif

	popfw
	popw	%dx
	popw	%bp

	/* restore BP and DX in the safest way! */
	call	assign_base_pointer	/* BP points to base_addr */
	movw	%cs:(A20Config - base_addr)(%bp), %dx

	/* restore original return address */
	movw	%cs:(A20ReturnAddress - base_addr)(%bp), %ax
	movw	%sp, %bp
	movw	%ax, 32(%bp)		/* update the stack! */
	call	assign_base_pointer	/* BP points to base_addr */

	/* restore original IF, DF */
	pushw	%cs:(A20Flags - base_addr)(%bp)
	popfw

	pushl	%ecx
	movl	$0x2, %ecx
	call	a20_test_match
	popl	%ecx
	/* ZF=1(means equal) for desired and we needn't do anything. */
	jz	a20_done
#endif
	#######################################################
	## Enable the keyboard controller A20 gate
	#######################################################
a20_kbc:
	call	empty_8042

	pushfw			# ZF=0 indicates there is no 8042
	pushl	%ecx
	movl	$0x2, %ecx
	call	a20_test_match
	popl	%ecx
	popw	%ax		# flags
	/* ZF=1(means equal) for desired and we needn't do anything. */
	jz	a20_done	# A20 live, no need to use KBC

	pushw	%ax		# flags
	popfw
	jnz	a20_fast	# failure, no 8042, try next

	testb	%dl, %dl
	jz	1f
	call	assign_base_pointer	/* BP points to base_addr */
	movb	$A20_KBC, %cs:(A20Type - base_addr)(%bp)
	call	a20_debug_print
1:
	movb	$0xD1, %al	# 8042 command byte to write output port
	outb	%al, $0x64	# write command to port 64h
	call	empty_8042

	movb	$0xDD, %al	# 0xDD is for disable, 0xDF is for enable
	testb	%dl, %dl
	setne	%ah
	shlb	$1, %ah
	orb	%ah, %al
	outb	%al, $0x60
	call	empty_8042

	pushl	%ecx
	movl	$0x2, %ecx
	call	a20_test_match
	popl	%ecx
	/* ZF=1(means equal) for desired and we needn't do anything. */
	pushfw			# ZF=1 for "A20 is OK"

	/* output a dummy command (USB keyboard hack) */
	movb	$0xFF, %al
	outb	%al, $0x64
	call	empty_8042

	popfw			# ZF=1 for "A20 is OK"
	jz	a20_done	# A20 live, no need to use KBC

	pushl	%ecx
	movl	$0x2, %ecx	# 0x200000 is too big
	call	a20_test_match
	popl	%ecx
	/* ZF=1(means equal) for desired and we needn't do anything. */
	jz	a20_done

	######################################################################
	## Fast A20 Gate: System Control Port A
	######################################################################

a20_fast:
	inb	$0x92, %al
	testb	%dl, %dl
	jz	2f
	/* enable a20 */
	call	assign_base_pointer	/* BP points to base_addr */
	movb	$A20_FAST, %cs:(A20Type - base_addr)(%bp)
	call	a20_debug_print
	testb	$0x02, %al
	jnz	1f		# chipset bug: do nothing if already set
	orb	$0x02, %al	# "fast A20" version
	andb	$0xFE, %al	# don't accidentally reset the cpu
	jmp	3f
2:
	/* disable a20 */
	testb	$0x02, %al
	jz	1f		# chipset bug: do nothing if already cleared
	andb	$0xFC, %al	# don't accidentally reset the cpu
3:
	outb	%al, $0x92
1:

	pushl	%ecx
	movl	$0x8, %ecx	# 0x200000 is too big
	call	a20_test_match
	popl	%ecx
	/* ZF=1(means equal) for desired and we needn't do anything. */
	jz	a20_done

	#==================================================================
	#	A20 is not responding.  Try again.
	#==================================================================

	/* A20Type is now A20_FAST, so it must be reset!! */
	call	assign_base_pointer	/* BP points to base_addr */
	movb	$A20_DUNNO, %cs:(A20Type - base_addr)(%bp)
	call	a20_debug_print
	decw	%cs:(A20Tries - base_addr)(%bp)
	jnz	a20_try_again

	#==================================================================
	#	Finally failed.
	#==================================================================

	testb	%dl, %dl
	jnz	a20_done_fail
	/* We cannot disable it, so consider there is no A20 gate. */
	call	assign_base_pointer	/* BP points to base_addr */
	movb	$A20_NONE, %cs:(A20Type - base_addr)(%bp)

a20_done_fail:
	incw	%dx		# set ZF=0 for failure

a20_done:
	pushfw
	/* print "done!" to show that a return was executed */
	testb	%dh, %dh
	jz	1f
	call	assign_base_pointer	/* BP points to base_addr */
	leaw	(A20DbgMsgEnd - base_addr)(%bp), %si	/* CS:SI is string */
	call	a20_print_string
1:
	popfw
	popal
	ret


//////////////////////////////////////////////////////////////////////////////
//
//	/////////////////////////////////////////////////////////
//	//
//	//	Subroutines begin here
//	//
//	/////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////


	######################################################################
	## This routine tests if A20 status matches the desired.
	######################################################################

a20_test_match:
1:
	call	a20_test
	sete	%al		/* save ZF to AL */
	testb	%dl, %dl
	sete	%ah		/* save ZF to AH */
	cmpb	%al, %ah
	ADDR32 loopnz	1b	/* dec ECX */
	/* ZF=1(means equal) for match */
	ret

	######################################################################
	## This routine tests if A20 is enabled (ZF = 0).  This routine
	## must not destroy any register contents.
	######################################################################

a20_test:
	pushl	%eax
	pushw	%cx
	pushw	%ds
	pushw	%es
	pushw	%si
	pushw	%di

	pushfw				/* save old IF, DF */

#if DISABLE_CPU_CACHE

	/* disable CPU cache for the test to work reliably. */

	movl	%cr0, %eax

	pushl	%eax			/* save old cr0 */

	orl	$0x60000000, %eax	/* set CD and NW */
	movl	%eax, %cr0
	movl	%cr0, %eax
	testl	$0x60000000, %eax	/* check if we can use wbinvd. */
	jz	1f			/* CPU has no wbinvd instruction. */
	wbinvd
	andl	$0xDFFFFFFF, %eax	/* clear NW */
	movl	%eax, %cr0
1:
#endif
	xorw	%ax, %ax
	movw	%ax, %ds	/* DS=0 */
	decw	%ax
	movw	%ax, %es	/* ES=0xFFFF */

	movw	$(0xFFF0 / 4), %cx
	xorw	%si, %si
	movw	$0x0010, %di
	cld
	repz cmpsl
	jne	1f		/* A20 is known to be enabled */

	/* A20 status unknown */

	movl	0, %eax
	pushl	%eax		/* save old int 0 vector */

	movw	$32, %cx	# Loop count
	cli			/* safe to touch int 0 vector */
2:
	incl	%eax
	movl	%eax, 0
	call	delay		# Serialize, and fix delay
	cmpl	%es:0x10, %eax
	loopz	2b

	popl	%eax		/* restore int 0 vector */
	movl	%eax, 0
1:
	/* ZF=0(means not equal) for A20 on, ZF=1(means equal) for A20 off. */

#if DISABLE_CPU_CACHE
	popl	%eax		/* restore cr0 */
	movl	%eax, %cr0
#endif

	lahf			/* Load Flags into AH Register. */
				/* AH = SF:ZF:xx:AF:xx:PF:xx:CF */
	
	popfw			/* restore IF, DF */
	sahf			/* update ZF */

	popw	%di
	popw	%si
	popw	%es
	popw	%ds
	popw	%cx
	popl	%eax
	ret

slow_out:
	outb	%al, %dx	# Fall through

delay:
	pushw	%ax
	movb	$0x80, %al	/* try to write only a known value to port */
	outb	%al, $IO_DELAY_PORT
	outb	%al, $IO_DELAY_PORT
	popw	%ax
	ret

	######################################################################
	##
	## Print A20Tries, A20Type
	##
	######################################################################

a20_debug_print:
	testb	%dh, %dh	/* debug mode? */
	jnz	1f		/* yes, continue */
	ret
1:
	pushal
	call	assign_base_pointer	/* BP points to base_addr */

	movb	%cs:(A20Tries - base_addr)(%bp), %al		/* A20Tries */
	call	a20_hex
	movw	%ax, %cs:(A20DbgMsgTryHex - base_addr)(%bp)	/* A20Tries */

	movb	%cs:(A20Type - base_addr)(%bp), %al		/* A20Type */
	call	a20_hex
	movw	%ax, %cs:(A20DbgMsgTryHex - base_addr + 2)(%bp)	/* A20Type */

	leaw	(A20DbgMsgTry - base_addr)(%bp), %si	/* CS:SI is string */
	call	a20_print_string
	popal
	ret

	/************************************************/
	/* print ASCIZ string CS:SI (modifies AX BX SI) */
	/************************************************/
3:
	xorw	%bx, %bx	/* video page 0 */
	movb	$0x0e, %ah	/* print char in AL */
	int	$0x10		/* via TTY mode */

a20_print_string:

	lodsb	%cs:(%si), %al	/* get token */
	cmpb	$0, %al		/* end of string? */
	jne	3b
	ret

	/****************************************/
	/* convert AL to hex ascii number in AX */
	/****************************************/

a20_hex:
	movb	%al, %ah
	shrb	$4, %al
	andb	$0x0F, %ah
	orw	$0x3030, %ax

	/* least significant digit in AH */
	cmpb	$0x39, %ah
	jbe	1f
	addb	$7, %ah
1:
	/* most significant digit in AL */
	cmpb	$0x39, %al
	jbe	1f
	addb	$7, %al
1:
	ret

	######################################################################
	##
	## Routine to empty the 8042 KBC controller. Return ZF=0 on failure.
	##
	######################################################################

empty_8042:
	pushl	%ecx
	movl	$10000, %ecx	# 100000 is too big
4:
	call	delay

	inb	$0x64, %al	# read 8042 status from port 64h
	testb	$1, %al		# is output buffer(data FROM keyboard) full?
	jnz	1f		# yes, read it and discard
	testb	$2, %al		# is input buffer(data TO keyboard) empty?
	jnz	2f		# no, wait until time out
	jmp	3f		# both input buffer and output buffer are empty
				# success and return with ZF=1
1:
	call	delay		# ZF=0, DELAY should not touch flags!!
	inb	$0x60, %al	# read output buffer and discard input
				# data/status from 8042
2:
	ADDR32 loop	4b	# ZF=0
				# timed out and failure, return with ZF=0
3:
	popl	%ecx
	ret

	.align	2

A20List:
	.word	a20_dunno - base_addr
	.word	a20_none - base_addr
	.word	a20_bios - base_addr
	.word	a20_kbc - base_addr
	.word	a20_fast - base_addr
A20Type:
	.word	A20_DUNNO	// default = unknown
A20Tries:
	.word	0		// Times until giving up on A20

	/* Just in case INT 15 might have destroyed the stack... */
A20Config:
	.word	0		// save original DX here
A20Flags:
	.word	0		// save original Flags here
A20ReturnAddress:
	.word	0		// save original return address here

	/* a20 debug message. 25 backspaces to wipe out the previous
	 * "A20 Debug: XXXX trying..." message.
	 */
A20DbgMsgTry:
	.ascii	"\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\bA20 Debug: "
A20DbgMsgTryHex:
	.string	"XXXX trying..."		// null terminated

	/* a20 done message. 9 backspaces to wipe out the previous
	 * "trying..." message.
	 */
A20DbgMsgEnd:
	.string	"\b\b\b\b\b\b\b\b\bdone! "	// null terminated

