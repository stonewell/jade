/* keys.h -- Event structures
   Copyright (C) 1993, 1994 John Harper <john@dcs.warwick.ac.uk>
   $Id$

   This file is part of Jade.

   Jade is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   Jade is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Jade; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#define EV_TYPE_KEYBD	0x00000001
#define EV_TYPE_MOUSE	0x00000002
#define EV_TYPE_MASK	0x00000003

#define EV_MOD_SHIFT	0x00000004
#define EV_MOD_CTRL	0x00000008
#define EV_MOD_MOD1	0x00000010
#define EV_MOD_BUTTON1	0x00000020
#define EV_MOD_BUTTON2	0x00000040
#define EV_MOD_BUTTON3	0x00000080
#define EV_MOD_MOD2	0x00000100
#define EV_MOD_MOD3	0x00000200
#define EV_MOD_MOD4	0x00000400
#define EV_MOD_MOD5	0x00000800
#define EV_MOD_BUTTON4	0x00001000
#define EV_MOD_BUTTON5	0x00002000

#define EV_MOD_MASK	0xfffffffc

#define EV_MOD_META	EV_MOD_MOD1
#define EV_MOD_LMB 	EV_MOD_BUTTON1
#define EV_MOD_MMB	EV_MOD_BUTTON2
#define EV_MOD_RMB	EV_MOD_BUTTON3

/* For EV_TYPE_KEYBD the code is like this,
   X11:
    Standard Keysym's
   Amiga:
    Normal *raw* keycodes  */

/* EV_CODE for EV_TYPE_MOUSE events */
#define EV_CODE_MOUSE_CLICK1 1
#define EV_CODE_MOUSE_CLICK2 2
#define EV_CODE_MOUSE_MOVE   3
#define EV_CODE_MOUSE_UP     4

/* A `key' is a vector of length 3, it's data is as follows,  */
#define KEY_CODE 0		/* code, i.e. keysym */
#define KEY_MODS 1		/* type & mods */
#define KEY_COMMAND 2		/* command to evaluate */
#define KEY_SIZE 3
