/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2000,2001,2002,2004,2005  Free Software Foundation, Inc.
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

#include <shared.h>
#include <term.h>

#ifdef SUPPORT_GRAPHICS
extern int disable_space_highlight;
#endif /* SUPPORT_GRAPHICS */

#ifdef GRUB_UTIL

grub_jmp_buf restart_env;

/* The preset_menu variable is referenced by asm.S */
# if defined(PRESET_MENU_STRING)
const char *preset_menu = PRESET_MENU_STRING;
# elif defined(SUPPORT_DISKLESS)
/* Execute the command "bootp" automatically.  */
const char *preset_menu = "bootp\n";
#else /* ! SUPPORT_DISKLESS */
/* The preset_menu variable is always available. This is for the loader, which
 * loads GRUB, to define a new preset menu before transferring control to GRUB.
 */
const char *preset_menu = 0;
# endif /* ! SUPPORT_DISKLESS */
#else /* ! GRUB_UTIL */
/* preset_menu is defined in asm.S */
#endif /* GRUB_UTIL */

#if 1 || defined(PRESET_MENU_STRING) || defined(SUPPORT_DISKLESS)

static int preset_menu_offset;

static int
open_preset_menu (void)
{
#ifdef GRUB_UTIL
  /* Unless the user explicitly requests to use the preset menu,
     always opening the preset menu fails in the grub shell.  */
  if (! use_preset_menu)
    return 0;
#endif /* GRUB_UTIL */
  
  preset_menu_offset = 0;
  return preset_menu != 0;
}

static int
read_from_preset_menu (char *buf, int max_len)
{
  int len;

  if (preset_menu == 0)
	return 0;

  len = grub_strlen (preset_menu + preset_menu_offset);

  if (len > max_len)
    len = max_len;

  grub_memmove (buf, preset_menu + preset_menu_offset, len);
  preset_menu_offset += len;

  return len;
}

static void
close_preset_menu (void)
{
  /* Disable the preset menu.  */
  preset_menu = 0;
}

#else /* ! PRESET_MENU_STRING && ! SUPPORT_DISKLESS */

#define open_preset_menu()	0
#define read_from_preset_menu(buf, max_len)	0
#define close_preset_menu()

#endif /* ! PRESET_MENU_STRING && ! SUPPORT_DISKLESS */

#ifdef GRUB_UTIL
#undef	DISP_UL		//218
#undef	DISP_UR		//191
#undef	DISP_LL		//192
#undef	DISP_LR		//217
#undef	DISP_HORIZ	//196
#undef	DISP_VERT	//179
#undef	DISP_LEFT	//0x1b
#undef	DISP_RIGHT	//0x1a
#undef	DISP_UP		//0x18
#undef	DISP_DOWN	//0x19

#define DISP_UL		ACS_ULCORNER	/* 0xDA , '+' */
#define DISP_UR		ACS_URCORNER	/* 0xBF , '+' */
#define DISP_LL		ACS_LLCORNER	/* 0xC0 , '+' */
#define DISP_LR		ACS_LRCORNER	/* 0xD9 , '+' */
#define DISP_HORIZ	ACS_HLINE	/* 0xC4 , '-' */
#define DISP_VERT	ACS_VLINE	/* 0xB3 , '|' */
#define DISP_LEFT	ACS_LARROW	/* 0x1B , '<' */
#define	DISP_RIGHT	ACS_RARROW	/* 0x1A , '>' */
#define DISP_UP		ACS_UARROW	/* 0x18 , '^' */
#define DISP_DOWN	ACS_DARROW	/* 0x19 , 'v' */
#endif

/* line start */
#define MENU_BOX_X	2

/* line width */
#define MENU_BOX_W	76

/* first line number */
#define MENU_BOX_Y	2

/* window height */
#define MENU_BOX_H	(current_term->max_lines - 8)

/* line end */
#define MENU_BOX_E	(MENU_BOX_X + MENU_BOX_W)

/* window bottom */
#define MENU_BOX_B	(MENU_BOX_Y + MENU_BOX_H)

static long temp_entryno;

static char *
get_entry (char *list, int num, int nested)
{
  int i;

  for (i = 0; i < num; i++)
    {
      do
	{
	  while (*(list++));
	}
      while (nested && *(list++));
    }

  return list;
}

/* Print an entry in a line of the menu box.  */
static void
print_entry (int y, int highlight, char *entry)
{
  int x;

  if (current_term->setcolorstate)
    current_term->setcolorstate (highlight ? COLOR_STATE_HIGHLIGHT : COLOR_STATE_NORMAL);
  
//  if (highlight && current_term->setcolorstate)
//    current_term->setcolorstate (COLOR_STATE_HIGHLIGHT);

  gotoxy (MENU_BOX_X - 1, y);
#ifdef SUPPORT_GRAPHICS
  disable_space_highlight = 1;
#endif /* SUPPORT_GRAPHICS */
  grub_putchar (' ');
#ifdef SUPPORT_GRAPHICS
  disable_space_highlight = 0;
#endif /* SUPPORT_GRAPHICS */
  for (x = MENU_BOX_X; x < MENU_BOX_E; x++)
    {
      if (*entry && x <= MENU_BOX_W)
	{
	  if (x == MENU_BOX_W)
	    grub_putchar (DISP_RIGHT);
	  else
	    grub_putchar (*entry++);
	}
      else
      {
#ifdef SUPPORT_GRAPHICS
	disable_space_highlight = 1;
#endif /* SUPPORT_GRAPHICS */
	grub_putchar (' ');
      }
    }
#ifdef SUPPORT_GRAPHICS
  disable_space_highlight = 0;
#endif /* SUPPORT_GRAPHICS */
  //gotoxy (MENU_BOX_E - 1, y);		/* XXX: Why? */

  if (current_term->setcolorstate)
    current_term->setcolorstate (COLOR_STATE_STANDARD);
}

/* Print entries in the menu box.  */
static void
print_entries (int first, int entryno, char *menu_entries)
{
  int i;
  
  if (current_term->setcolorstate)
    current_term->setcolorstate (COLOR_STATE_NORMAL);
  
  gotoxy (MENU_BOX_E, MENU_BOX_Y);

  if (first)
    grub_putchar (DISP_UP);
  else
    grub_putchar (DISP_VERT);

  menu_entries = get_entry (menu_entries, first, 0);

  for (i = 0; i < MENU_BOX_H/*size*/; i++)
    {
      print_entry (MENU_BOX_Y + i, entryno == i, menu_entries);

      while (*menu_entries)
	menu_entries++;

      if (*(menu_entries - 1))
	menu_entries++;
    }

  gotoxy (MENU_BOX_E, MENU_BOX_Y - 1 + MENU_BOX_H/*size*/);

  if (current_term->setcolorstate)
    current_term->setcolorstate (COLOR_STATE_NORMAL);
  
  if (*menu_entries)
    grub_putchar (DISP_DOWN);
  else
    grub_putchar (DISP_VERT);

  if (current_term->setcolorstate)
    current_term->setcolorstate (COLOR_STATE_STANDARD);

  gotoxy (MENU_BOX_E, MENU_BOX_Y + entryno);	/* XXX: Why? */
}

static void
print_entries_raw (int size, int first, char *menu_entries)
{
  int i;

  for (i = 0; i < MENU_BOX_W; i++)
    grub_putchar ('-');
  grub_putchar ('\n');

  for (i = first; i < size; i++)
    {
      /* grub's printf can't %02d so ... */
      if (i < 10)
	grub_putchar (' ');
      grub_printf ("%d: %s\n", i, get_entry (menu_entries, i, 0));
    }

  for (i = 0; i < MENU_BOX_W; i++)
    grub_putchar ('-');
  grub_putchar ('\n');
}


static void
print_border (int y, int size)
{
  int i;

  if (current_term->setcolorstate)
    current_term->setcolorstate (COLOR_STATE_NORMAL);
  
  gotoxy (MENU_BOX_X - 2, y);

  grub_putchar (DISP_UL);
  for (i = 0; i < MENU_BOX_W + 1; i++)
    grub_putchar (DISP_HORIZ);
  grub_putchar (DISP_UR);

  i = 1;
  while (1)
    {
      gotoxy (MENU_BOX_X - 2, y + i);

      if (i > size)
	break;
      
      grub_putchar (DISP_VERT);
      gotoxy (MENU_BOX_E, y + i);
      grub_putchar (DISP_VERT);

      i++;
    }

  grub_putchar (DISP_LL);
  for (i = 0; i < MENU_BOX_W + 1; i++)
    grub_putchar (DISP_HORIZ);
  grub_putchar (DISP_LR);

  if (current_term->setcolorstate)
    current_term->setcolorstate (COLOR_STATE_STANDARD);
}

static void
run_menu (char *menu_entries, char *config_entries, int num_entries,
	  char *heap, int entryno)
{
  int c, time1, time2 = -1, first_entry = 0;
  char *cur_entry = 0;
//  struct term_entry *prev_term = NULL;
		  

  /*
   *  Main loop for menu UI.
   */

restart:
  /* Dumb terminal always use all entries for display 
     invariant for TERM_DUMB: first_entry == 0  */
  if (! (current_term->flags & TERM_DUMB))
    {
#if 0
      while (entryno > 11)
	{
	  first_entry++;
	  entryno--;
	}
#else
      if (entryno > MENU_BOX_H - 1)
	{
	  first_entry += entryno - (MENU_BOX_H - 1);
	  entryno = MENU_BOX_H - 1;
	}
#endif
    }

  /* If the timeout was expired or wasn't set, force to show the menu
     interface. */
  if (grub_timeout < 0)
    show_menu = 1;
  
  /* If SHOW_MENU is false, don't display the menu until ESC is pressed.  */
  if (! show_menu)
    {
      /* Get current time.  */
      while ((time1 = getrtsecs ()) == 0xFF)
	;

      while (1)
	{
	  /* Check if ESC is pressed.  */
	  if (checkkey () != -1 && ASCII_CHAR (getkey ()) == '\e')
	    {
	      grub_timeout = -1;
	      show_menu = 1;
	      break;
	    }

	  /* If GRUB_TIMEOUT is expired, boot the default entry.  */
	  if (grub_timeout >=0
	      && (time1 = getrtsecs ()) != time2
	      && time1 != 0xFF)
	    {
	      if (grub_timeout <= 0)
		{
		  grub_timeout = -1;
		  goto boot_entry;
		}
	      
	      time2 = time1;
	      grub_timeout--;
	      
	      /* Print a message.  */
	      grub_printf ("\rPress `ESC' to enter the menu... %d   ",
			   grub_timeout);
	    }
	}
    }

  /* Only display the menu if the user wants to see it. */
  if (show_menu)
    {
      init_page ();
      setcursor (0);

      if (current_term->flags & TERM_DUMB)
	print_entries_raw (num_entries, first_entry, menu_entries);
      else
	print_border (MENU_BOX_Y - 1, MENU_BOX_H);

      grub_printf ("\nUse the %c and %c keys to highlight an entry.",
		   DISP_UP, DISP_DOWN);
      
      if (! auth && password)
	{
	  printf (" Press ENTER or \'b\' to boot.\n"
		"Press \'p\' to gain privileged control.");
	}
      else
	{
	  if (config_entries)
	    printf (" Press ENTER or \'b\' to boot.\n"
		    "Press \'e\' to edit the commands before booting, or \'c\' for a command-line.");
	  else
	    printf (" At a selected line, press \'e\' to\n"
		"edit, \'d\' to delete, or \'O\'/\'o\' to open a new line before/after. When done,\n"
		"press \'b\' to boot, \'c\' for a command-line, or ESC to go back to the main menu.");
	}

      if (current_term->flags & TERM_DUMB)
	grub_printf ("\n\nThe selected entry is %d ", entryno);
      else
	print_entries (first_entry, entryno, menu_entries);
    }

  /* XX using RT clock now, need to initialize value */
  while ((time1 = getrtsecs()) == 0xFF);

  temp_entryno = 0;
  while (1)
    {
      /* Initialize to NULL just in case...  */
      cur_entry = NULL;

      if (grub_timeout >= 0 && (time1 = getrtsecs()) != time2 && time1 != 0xFF)
	{
	  if (grub_timeout <= 0)
	    {
	      grub_timeout = -1;
	      break;
	    }

	  /* else not booting yet! */
	  time2 = time1;

	  if (current_term->flags & TERM_DUMB)
	      grub_printf ("\r    Entry %d will be booted automatically in %d seconds.   ", 
			   entryno, grub_timeout);
	  else
	    {
	      gotoxy (MENU_BOX_X - 2, MENU_BOX_H + 7);
	      grub_printf ("The highlighted entry will be booted automatically in %d seconds.    ",
			   grub_timeout);
	      gotoxy (MENU_BOX_E, MENU_BOX_Y + entryno);
	  }
	  
	  grub_timeout--;
	}

      /* Check for a keypress, however if TIMEOUT has been expired
	 (GRUB_TIMEOUT == -1) relax in GETKEY even if no key has been
	 pressed.  
	 This avoids polling (relevant in the grub-shell and later on
	 in grub if interrupt driven I/O is done).  */
      if (checkkey () >= 0 || grub_timeout < 0)
	{
	  /* Key was pressed, show which entry is selected before GETKEY,
	     since we're comming in here also on GRUB_TIMEOUT == -1 and
	     hang in GETKEY */
	  if (current_term->flags & TERM_DUMB)
	    grub_printf ("\r    Highlighted entry is %d: ", entryno);

	  c = /*ASCII_CHAR*/ (getkey ());

	  if (grub_timeout >= 0)
	    {
	      if (current_term->flags & TERM_DUMB)
		grub_putchar ('\r');
	      else
		gotoxy (MENU_BOX_X - 2, MENU_BOX_H + 7);
	      printf ("                                                                    ");
	      grub_timeout = -1;
	      fallback_entryno = -1;
	      if (! (current_term->flags & TERM_DUMB))
		gotoxy (MENU_BOX_E, MENU_BOX_Y + entryno);
	    }

	  /* We told them above (at least in SUPPORT_SERIAL) to use
	     '^' or 'v' so accept these keys.  */
	  if (c == KEY_UP/*16*/ || ((char)c) == '^')
	    {
	      temp_entryno = 0;
	      if (current_term->flags & TERM_DUMB)
		{
		  if (entryno > 0)
		      entryno--;
		}
	      else
		{
		  if (entryno > 0)
		    {
		      print_entry (MENU_BOX_Y + entryno, 0,
				   get_entry (menu_entries,
					      first_entry + entryno,
					      0));
		      entryno--;
		      print_entry (MENU_BOX_Y + entryno, 1,
				   get_entry (menu_entries,
					      first_entry + entryno,
					      0));
		    }
		  else if (first_entry > 0)
		    {
		      first_entry--;
		      print_entries (first_entry, entryno, menu_entries);
		    }
		  else	/* loop forward to END */
		    {
		      temp_entryno = num_entries - 1;
		      goto check_update;
		    }
		}
	    }
	  else if ((c == KEY_DOWN/*14*/ || ((char)c) == 'v')
		   /* && first_entry + entryno + 1 < num_entries */)
	    {
	      temp_entryno = 0;
	      if (current_term->flags & TERM_DUMB)
		{
		  if (first_entry + entryno + 1 < num_entries)
		      entryno++;
		}
	      else
		{
		  if (first_entry + entryno + 1 >= num_entries)
		    {
		      temp_entryno = 0;	/* loop backward to HOME */
		      goto check_update;
		    }
		  if (entryno < MENU_BOX_H - 1)
		    {
		      print_entry (MENU_BOX_Y + entryno, 0,
				   get_entry (menu_entries,
					      first_entry + entryno,
					      0));
		      entryno++;
		      print_entry (MENU_BOX_Y + entryno, 1,
				   get_entry (menu_entries,
					      first_entry + entryno,
					      0));
		    }
		  else if (num_entries > MENU_BOX_H + first_entry)
		    {
		      first_entry++;
		      print_entries (first_entry, entryno, menu_entries);
		    }
		}
	    }
	  else if (c == KEY_PPAGE/*7*/)
	    {
	      /* Page Up */
	      temp_entryno = 0;
	      first_entry -= MENU_BOX_H;
	      if (first_entry < 0)
		{
		  entryno += first_entry;
		  first_entry = 0;
		  if (entryno < 0)
		    entryno = 0;
		}
	      print_entries (first_entry, entryno, menu_entries);
	    }
	  else if (c == KEY_NPAGE/*3*/)
	    {
	      /* Page Down */
	      temp_entryno = 0;
	      first_entry += MENU_BOX_H;
	      if (first_entry + entryno + 1 >= num_entries)
		{
		  first_entry = num_entries - MENU_BOX_H;
		  if (first_entry < 0)
		    first_entry = 0;
		  entryno = num_entries - first_entry - 1;
		}
	      print_entries (first_entry, entryno, menu_entries);
	    }
	  else if ( ((char)c) >= '0' && ((char)c) <= '9')
	    {
	      temp_entryno *= 10;
	      temp_entryno += ((char)c) - '0';
//	      if (temp_entryno >= num_entries)
//		  temp_entryno = num_entries - 1;
	      if (temp_entryno >= num_entries)	/* too big an entryno */
	      {
		if ((char)c - '0' >= num_entries)
		  temp_entryno = 0;
	        else
		  temp_entryno = (char)c - '0';
	      }
	      if (temp_entryno != 0 || (char)c == '0')
	      {
check_update:
		  if (temp_entryno != first_entry + entryno)
		  {
		      first_entry = (temp_entryno / MENU_BOX_H) * MENU_BOX_H;
		      entryno = temp_entryno % MENU_BOX_H;
		      print_entries (first_entry, entryno, menu_entries);
		  }
	      }
	      if (temp_entryno * 10 >= num_entries)
		  temp_entryno = 0;
	    }
	  else if (c == KEY_HOME/*1*/)
	    {
	      temp_entryno = 0;
	      goto check_update;
	    }
	  else if (c == KEY_END/*5*/)
	    {
	      temp_entryno = num_entries - 1;
	      goto check_update;
	    }
	  else
	      temp_entryno = 0;

	  gotoxy (MENU_BOX_E - 8, MENU_BOX_Y - 2);
	  grub_printf ("%d  ", first_entry + entryno);
	  gotoxy (MENU_BOX_E, MENU_BOX_Y + entryno);

	  if (config_entries)
	    {
	      if ((((char)c) == '\n') || (((char)c) == '\r') || (((char)c) == 'b'/*6*/))
		break;
	    }
	  else
	    {
	      if ((((char)c) == 'd') || (((char)c) == 'o') || (((char)c) == 'O'))
		{
		  if (! (current_term->flags & TERM_DUMB))
		    print_entry (MENU_BOX_Y + entryno, 0,
				 get_entry (menu_entries,
					    first_entry + entryno,
					    0));

		  /* insert after is almost exactly like insert before */
		  if (((char)c) == 'o')
		    {
		      /* But `o' differs from `O', since it may causes
			 the menu screen to scroll up.  */
		      if (entryno < MENU_BOX_H - 1 || (current_term->flags & TERM_DUMB))
			entryno++;
		      else
			first_entry++;
		      
		      c = 'O';
		    }

		  cur_entry = get_entry (menu_entries,
					 first_entry + entryno,
					 0);

		  if (((char)c) == 'O')
		    {
		      grub_memmove (cur_entry + 2, cur_entry,
				    ((int) heap) - ((int) cur_entry));

		      cur_entry[0] = ' ';
		      cur_entry[1] = 0;

		      heap += 2;

		      num_entries++;
		    }
		  else if (num_entries > 0)
		    {
		      char *ptr = get_entry(menu_entries,
					    first_entry + entryno + 1,
					    0);

		      grub_memmove (cur_entry, ptr,
				    ((int) heap) - ((int) ptr));
		      heap -= (((int) ptr) - ((int) cur_entry));

		      num_entries--;

		      if (entryno >= num_entries)
			entryno--;
		      if (first_entry && num_entries < MENU_BOX_H + first_entry)
			first_entry--;
		    }

		  if (current_term->flags & TERM_DUMB)
		    {
		      grub_printf ("\n\n");
		      print_entries_raw (num_entries, first_entry,
					 menu_entries);
		      grub_printf ("\n");
		    }
		  else
		    print_entries (first_entry, entryno, menu_entries);
		}

	      cur_entry = menu_entries;
	      if (((char)c) == 27)
		return;
	      if (((char)c) == 'b')
		break;
	    }

	  if (! auth && password)
	    {
	      if (((char)c) == 'p')
		{
		  /* Do password check here! */
		  char entered[32];
		  char *pptr = password;

		  if (current_term->flags & TERM_DUMB)
		    grub_printf ("\r                                    ");
		  else
		    gotoxy (MENU_BOX_X - 2, MENU_BOX_H + 6);

		  /* Wipe out the previously entered password */
		  grub_memset (entered, 0, sizeof (entered));
		  prompt = "Password: ";
		  maxlen = sizeof (entered) - 1;
		  echo_char = '*';
		  readline = 0;
		  get_cmdline (entered);

		  while (! isspace (*pptr) && *pptr)
		    pptr++;

		  /* Make sure that PASSWORD is NUL-terminated.  */
		  *pptr++ = 0;

		  if (! check_password (entered, password, password_type))
		    {
		      char *new_file = config_file;
		      while (isspace (*pptr))
			pptr++;

		      /* If *PPTR is NUL, then allow the user to use
			 privileged instructions, otherwise, load
			 another configuration file.  */
		      if (*pptr != 0)
			{
			  while ((*(new_file++) = *(pptr++)) != 0)
			    ;

			  /* Make sure that the user will not have
			     authority in the next configuration.  */
			  auth = 0;
			  return;
			}
		      else
			{
			  /* Now the user is superhuman.  */
			  auth = 1;
			  goto restart;
			}
		    }
		  else
		    {
		      grub_printf ("Failed! Press any key to continue...");
		      getkey ();
		      goto restart;
		    }
		}
	    }
	  else
	    {
	      if (((char)c) == 'e')
		{
		  int new_num_entries = 0, i = 0;
		  char *new_heap;

		  if (config_entries)
		    {
		      new_heap = heap;
		      cur_entry = get_entry (config_entries,
					     first_entry + entryno,
					     1);
		    }
		  else
		    {
		      /* safe area! */
		      new_heap = heap + NEW_HEAPSIZE + 1;
		      cur_entry = get_entry (menu_entries,
					     first_entry + entryno,
					     0);
		    }

		  do
		    {
		      while ((*(new_heap++) = cur_entry[i++]) != 0);
		      new_num_entries++;
		    }
		  while (config_entries && cur_entry[i]);

		  /* this only needs to be done if config_entries is non-NULL,
		     but it doesn't hurt to do it always */
		  *(new_heap++) = 0;

		  if (config_entries)
		    run_menu (heap, NULL, new_num_entries, new_heap, 0);
		  else
		    {
		      cls ();
		      print_cmdline_message (0);

		      new_heap = heap + NEW_HEAPSIZE + 1;

		      saved_drive = boot_drive;
		      saved_partition = install_partition;
		      current_drive = GRUB_INVALID_DRIVE;

		      prompt = PACKAGE " edit> ";
		      maxlen = NEW_HEAPSIZE + 1;
		      echo_char = 0;
		      readline = 1;
		      if (! get_cmdline (new_heap))
			{
			  int j = 0;

			  /* get length of new command */
			  while (new_heap[j++])
			    ;

			  if (j < 2)
			    {
			      j = 2;
			      new_heap[0] = ' ';
			      new_heap[1] = 0;
			    }

			  /* align rest of commands properly */
			  grub_memmove (cur_entry + j, cur_entry + i,
					(int) heap - ((int) cur_entry + i));

			  /* copy command to correct area */
			  grub_memmove (cur_entry, new_heap, j);

			  heap += (j - i);
			}
		    }

		  goto restart;
		}
	      if (((char)c) == 'c')
		{
		  enter_cmdline (heap, 0);
		  goto restart;
		}
#ifdef GRUB_UTIL
	      if (((char)c) == 'q')
		{
		  /* The same as ``quit''.  */
		  stop ();
		}
#endif
	    }
	}
    }
  
  /* Attempt to boot an entry.  */
  
 boot_entry:
  
  cls ();
  setcursor (1);
//  /* if our terminal needed initialization, we should shut it down
//   * before booting the kernel, but we want to save what it was so
//   * we can come back if needed */
//  prev_term = current_term;
////  if (current_term->shutdown)
////    {
////      (*current_term->shutdown)();
////      current_term = term_table; /* assumption: console is first */
////    }
  
  while (1)
    {
      if (debug > 0)
      {
	if (config_entries)
		printf ("  Booting \'%s\'\n\n",
			get_entry (menu_entries, first_entry + entryno, 0));
	else
		printf ("  Booting command-list\n\n");
      }

      if (! cur_entry)
	cur_entry = get_entry (config_entries, first_entry + entryno, 1);

      /* Set CURRENT_ENTRYNO for the command "savedefault".  */
      current_entryno = first_entry + entryno;
      
      if (run_script (cur_entry, heap))
	{
	  if (fallback_entryno >= 0)
	    {
	      cur_entry = NULL;
	      first_entry = 0;
	      entryno = fallback_entries[fallback_entryno];
	      fallback_entryno++;
	      if (fallback_entryno >= MAX_FALLBACK_ENTRIES
		  || fallback_entries[fallback_entryno] < 0)
		fallback_entryno = -1;
	    }
	  else
	    break;
	}
      else
	break;
    }

//  /* if we get back here, we should go back to what our term was before */
//  current_term = prev_term;
//  if (current_term->startup)
//      /* if our terminal fails to initialize, fall back to console since
//       * it should always work */
//      if ((*current_term->startup)() == 0)
//          current_term = term_table; /* we know that console is first */
  show_menu = 1;
  goto restart;
}


static int
get_line_from_config (char *cmdline, int max_len, int read_from_file)
{
  int pos = 0, literal = 0, comment = 0;
  char c;  /* since we're loading it a byte at a time! */
  
  while (1)
    {
      if (read_from_file)
	{
	  if (! grub_read (&c, 1))
	    break;
	}
      else
	{
	  if (! read_from_preset_menu (&c, 1))
	    break;
	}

      /* Skip all carriage returns.  */
      if (c == '\r')
	continue;

      /* Replace tabs with spaces.  */
      if (c == '\t')
	c = ' ';

      /* The previous is a backslash, then...  */
      if (literal)
	{
	  /* If it is a newline, replace it with a space and continue.  */
	  if (c == '\n')
	    {
	      c = ' ';
	      
	      /* Go back to overwrite a backslash.  */
	      if (pos > 0)
		pos--;
	    }
	    
	  literal = 0;
	}
	  
      /* translate characters first! */
      if (c == '\\' && ! literal)
	literal = 1;

      if (comment)
	{
	  if (c == '\n')
	    comment = 0;
	}
      else if (! pos)
	{
	  if (c == '#')
	    comment = 1;
	  else if ((c != ' ') && (c != '\n'))
	    cmdline[pos++] = c;
	}
      else
	{
	  if (c == '\n')
	    break;

	  if (pos < max_len)
	    cmdline[pos++] = c;
	}
    }

  cmdline[pos] = 0;

  return pos;
}

//void
//reset (void);
static int config_len, menu_len, num_entries;
static char *config_entries, *menu_entries, *cur_entry;

static void
reset (void)
{
  count_lines = -1;
  config_len = 0;
  menu_len = 0;
  num_entries = 0;
  config_entries = (char *) init_free_mem_start;//mbi.drives_addr + mbi.drives_length;
  cur_entry = config_entries;
  menu_entries = (char *) MENU_BUF;
  init_config ();
}
  

/* This is the starting function in C.  */
void
cmain (void)
{
  char *kill_buf = (char *) KILL_BUF;

#ifdef GRUB_UTIL
  /* Initialize the environment for restarting Stage 2.  */
  grub_setjmp (restart_env);
#endif /* GRUB_UTIL */
  
  /* Initialize the kill buffer.  */
  *kill_buf = 0;

#ifndef GRUB_UTIL
  debug = debug_boot + 1;
  pxe_detect();
#endif /* ! GRUB_UTIL */
  
  /* Never return.  */
  for (;;)
    {
      extern int use_config_file;
      int is_opened, is_preset;
      int i;

      reset ();
      
      /* Here load the configuration file.  */
      
//#ifdef GRUB_UTIL
      if (use_config_file)
//#endif /* GRUB_UTIL */
	{
	  char *default_file = (char *) DEFAULT_FILE_BUF;
	  
	  /* Get a saved default entry if possible.  */
	  saved_entryno = 0;
	  if (*config_file)
	  {
	    *default_file = 0;	/* initialise default_file */
	    grub_strncat (default_file, config_file, DEFAULT_FILE_BUFLEN);
	    for (i = grub_strlen (default_file); i >= 0; i--)
	      if (default_file[i] == '/')
	        {
		  //i++;
		  break;
	        }
	    default_file[++i] = 0;
	    grub_strncat (default_file + i, "default", DEFAULT_FILE_BUFLEN - i);
	    if (debug > 1)
		grub_printf("Open %s ... ", default_file);
//	i=grub_open (default_file);
//	printf("default_file ok=%s\n", default_file);
//	for (;;);
      DEBUG_SLEEP

	    if (grub_open (default_file))
	      {
	        char buf[10]; /* This is good enough.  */
	        char *p = buf;
	        int len;
	      
		if (debug > 1)
			grub_printf("Read file: ", default_file);
	        len = grub_read (buf, sizeof (buf));
		if (debug > 1)
			grub_printf("len=%d\n", len);
	        if (len > 0)
		  {
		    buf[sizeof (buf) - 1] = 0;
		    safe_parse_maxint (&p, &saved_entryno);
		  }

	        grub_close ();
	      }
	    else if (debug > 1)
		grub_printf("failure.\n", default_file);
      DEBUG_SLEEP
	  }
	  errnum = ERR_NONE;
	  
#ifdef GRUB_UTIL
	  do
#endif /* GRUB_UTIL */
	    {
	      /* STATE 0:  Before any title command.
		 STATE 1:  In a title command.
		 STATE >1: In a entry after a title command.  */
	      int state = 0, prev_config_len = 0, prev_menu_len = 0;
	      char *cmdline;

	      is_preset = is_opened = 0;
	      /* Try command-line menu first if it is specified. */
	      if (preset_menu == (char *)0x0800 && ! *config_file)
		{
		  is_opened = is_preset = open_preset_menu ();
		}
	      if (! is_opened)
		{
		  /* Try config_file */
		  if (*config_file)
			is_opened = grub_open (config_file);
		}
	      errnum = ERR_NONE;
	      if (! is_opened)
		{
		  /* Try the preset menu. This will succeed at most once,
		   * because close_preset_menu disables the preset menu.  */
		  is_opened = is_preset = open_preset_menu ();
		}

	      if (! is_opened)
		break;

	      /* This is necessary, because the menu must be overrided.  */
	      reset ();
	      
	      cmdline = (char *) CMDLINE_BUF;
	      while (get_line_from_config (cmdline, NEW_HEAPSIZE,
					   ! is_preset))
		{
		  struct builtin *builtin;
		  
		  /* Get the pointer to the builtin structure.  */
		  builtin = find_command (cmdline);
		  errnum = 0;
		  if (! builtin)
		    /* Unknown command. Just skip now.  */
		    continue;
		  
		  if (builtin->flags & BUILTIN_TITLE)
		    {
		      char *ptr;
		      
		      /* the command "title" is specially treated.  */
		      if (state > 1)
			{
			  /* The next title is found.  */
			  num_entries++;
			  config_entries[config_len++] = 0;
			  prev_menu_len = menu_len;
			  prev_config_len = config_len;
			}
		      else
			{
			  /* The first title is found.  */
			  menu_len = prev_menu_len;
			  config_len = prev_config_len;
			}
		      
		      /* Reset the state.  */
		      state = 1;
		      
		      /* Copy title into menu area.  */
		      ptr = skip_to (1, cmdline);
		      while ((menu_entries[menu_len++] = *(ptr++)) != 0)
			;
		    }
		  else if (! state)
		    {
		      /* Run a command found is possible.  */
		      if (builtin->flags & BUILTIN_MENU)
			{
#if 0
			  extern int commandline_func (char *arg, int flags);
			  char *arg = (builtin->func) == commandline_func ? cmdline : skip_to (1, cmdline);
			  (builtin->func) (arg, BUILTIN_MENU);
			  errnum = 0;
#endif
			  char *ptr = cmdline;
			  /* Copy menu-specific commands to config area.  */
			  while ((config_entries[config_len++] = *ptr++) != 0);
			  prev_config_len = config_len;
			}
		      else
			/* Ignored.  */
			continue;
		    }
		  else
		    {
		      char *ptr = cmdline;
		      
		      /* state == 1 means it is immediately after a TITLE, and
		       * num_entries == 0 means the TITLE is the first one.  */
		      if (num_entries == 0 && state == 1)
			{
			  /* Finish the menu-specific commands.  */
			  config_entries[config_len++] = 0;
			}
		      state++;
		      /* Copy config file data to config area.  */
		      while ((config_entries[config_len++] = *ptr++) != 0)
			;
		    }
		} /* while */
	      
	      /* file must be closed here, because the menu-specific commands
	       * below may also use the GRUB_OPEN command.  */
	      if (is_preset)
		close_preset_menu ();
	      else
		grub_close ();
	      
	      if (state > 1)
		{
		  /* Finish the last entry.  */
		  num_entries++;
		  config_entries[config_len++] = 0;
		}
	      else// if (state)
		{
		  menu_len = prev_menu_len;
		  config_len = prev_config_len;
		}
	      //else
		///* Finish the menu-specific commands.  */
		//config_entries[config_len++] = 0;
	      
	      menu_entries[menu_len++] = 0;
	      config_entries[config_len++] = 0;
	      grub_memmove (config_entries + config_len, menu_entries,
			    menu_len);
	      menu_entries = config_entries + config_len;

      /* Run menu-specific commands before any other menu entry commands.  */
      
{
  char *old_entry;
  char *heap = menu_entries + menu_len;

#ifndef GRUB_UTIL
  int old_debug = 1;
#endif /* ! GRUB_UTIL */
      
  /* Initialize the data.  */
  extern void init_cmdline (void);
  init_cmdline ();

  while (1)
    {
      struct builtin *builtin;
      char *arg;

      /* Copy the first string in CUR_ENTRY to HEAP.  */
      old_entry = cur_entry;
      while (*cur_entry++)
	;

      grub_memmove (heap, old_entry, (int) cur_entry - (int) old_entry);
      if (! *heap)
	{
	  /* If there is no more command in SCRIPT...  */

	  /* If no kernel is loaded, just exit successfully.  */
	  if (kernel_type == KERNEL_TYPE_NONE)
	    break;

	  /* Otherwise, the command boot is run implicitly.  */
	  grub_memmove (heap, "boot", 5);
	}

      /* Find a builtin.  */
      builtin = find_command (heap);
      if (! builtin)
	{
	  grub_printf ("%s\n", old_entry);
	  continue;
	}

/* now we do not echo menu-specific commands */
//      if (! (builtin->flags & BUILTIN_NO_ECHO))
//	grub_printf ("%s\n", old_entry);

      /* If BUILTIN cannot be run in the menu, skip it.  */
      if (! (builtin->flags & BUILTIN_MENU))
	{
	  continue;
	}

#if 0
      /* Invalidate the cache, because the user may exchange removable
	 disks.  */
      buf_drive = -1;
#endif

      /* find && and || */

      for (arg = skip_to (0, heap); *arg != 0; arg = skip_to (0, arg))
      {
	struct builtin *builtin1;
	int ret;
	char *arg1;
	arg1 = arg;
        if (*arg == '&' && arg[1] == '&' && (arg[2] == ' ' || arg[2] == '\t'))
        {
		/* handle the AND operator */
		arg = skip_to (0, arg);
		builtin1 = find_command (arg);
		if (! builtin1 || ! (builtin1->flags & BUILTIN_MENU))
		{
			errnum = ERR_UNRECOGNIZED;
			goto next;
		}

		*arg1 = 0;
		ret = (builtin->func) (skip_to (1, heap), BUILTIN_MENU);
		*arg1 = '&';
		if (ret)
		{
			arg = skip_to (1, arg);
			(builtin1->func) (arg, BUILTIN_MENU);
		}
		goto next;
	} else if (*arg == '|' && arg[1] == '|' && (arg[2] == ' ' || arg[2] == '\t'))
	{
		/* handle the OR operator */
		arg = skip_to (0, arg);
		builtin1 = find_command (arg);
		if (! builtin1 || ! (builtin1->flags & BUILTIN_MENU))
		{
			errnum = ERR_UNRECOGNIZED;
			goto next;
		}

		*arg1 = 0;
		ret = (builtin->func) (skip_to (1, heap), BUILTIN_MENU);
		*arg1 = '|';
		if (! ret)
		{
			arg = skip_to (1, arg);
			(builtin1->func) (arg, BUILTIN_MENU);
		}
		goto next;
	}
      }
      
      /* Run BUILTIN->FUNC.  */
      extern int commandline_func (char *arg1, int flags);
      arg = (builtin->func) == commandline_func ? heap : skip_to (1, heap);
      (builtin->func) (arg, BUILTIN_MENU);

      /* if the INSERT key was pressed at startup, debug is not allowed to be turned off. */
#ifndef GRUB_UTIL
      if (debug_boot)
	if ((unsigned int)debug < 2)	/* debug == 0 or 1 */
	{
	    old_debug = debug;	/* save the new debug in old_debug */
	    debug = 2;
	}
#endif /* ! GRUB_UTIL */

next:
      DEBUG_SLEEP
      if (! *old_entry)
	break;
    }
  //config_entries = cur_entry; /* config file data begins here */
#ifndef GRUB_UTIL
    if (debug_boot)
    {
	debug = old_debug;
	grub_printf ("\n\nEnd of menu init commands. Press any key to enter command-line or run menu...", old_entry);
    }
#endif /* ! GRUB_UTIL */
      DEBUG_SLEEP
}
		errnum = 0;

	      /* Make sure that all fallback entries are valid.  */
	      if (fallback_entryno >= 0)
		{
		  for (i = 0; i < MAX_FALLBACK_ENTRIES; i++)
		    {
		      if (fallback_entries[i] < 0)
			break;
		      if (fallback_entries[i] >= num_entries)
			{
			  grub_memmove (fallback_entries + i,
					fallback_entries + i + 1,
					((MAX_FALLBACK_ENTRIES - i - 1)
					 * sizeof (int)));
			  i--;
			}
		    }

		  if (fallback_entries[0] < 0)
		    fallback_entryno = -1;
		}
	      /* Check if the default entry is present. Otherwise reset
		 it to fallback if fallback is valid, or to DEFAULT_ENTRY 
		 if not.  */
	      if (default_entry >= num_entries)
		{
		  if (fallback_entryno >= 0)
		    {
		      default_entry = fallback_entries[0];
		      fallback_entryno++;
		      if (fallback_entryno >= MAX_FALLBACK_ENTRIES
			  || fallback_entries[fallback_entryno] < 0)
			fallback_entryno = -1;
		    }
		  else
		    default_entry = 0;
		}
	    }
#ifdef GRUB_UTIL
	  while (is_preset);
#endif /* GRUB_UTIL */
	}

      /* go ahead and make sure the terminal is setup */
      if (current_term->startup)
	(*current_term->startup)();

      if (! num_entries)
	{
	  /* If no acceptable config file, goto command-line, starting
	     heap from where the config entries would have been stored
	     if there were any.  */
	  enter_cmdline (config_entries, 1);
	}
      else
	{
	  /* Run menu interface.  */
	  run_menu (menu_entries, cur_entry, num_entries,
		    menu_entries + menu_len, default_entry);
	}
    }
}
