/* edit.h -- Data structures for the editor (buffers, marks, etc...)
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
   along with Jade; see the file COPYING.	If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifndef _EDIT_H
#define _EDIT_H


/* Line structure -- an array of these is in the TX->tx_Lines */
typedef struct LINE {
    u_char	   *ln_Line;
    long	    ln_Strlen;	/* includes '\0' */
} LINE;


/* Each bookmark has one of these in the tx_Marks list */
typedef struct _Mark {
    VALUE	    mk_Car;
#define mk_Flags mk_Car

    /* When the file is resident this node is linked into its tx_Marks list,
       otherwise it's in a list of all non-resident marks.  */
    struct _Mark   *mk_Next;

    /* next allocated MARK  */
    struct _Mark   *mk_NextAlloc;

    VALUE	    mk_Pos;

    /* This union tells me where to look for the file this mark is in.
       if (mk_Resident == 0) then the file (mk_File.name) has to be loaded and
       used. Otherwise (mk_File.tx) is used.  */
    union {
	VALUE		name;
	/* this TX should not be marked for gc */
	struct _TX     *tx;
    }		    mk_File;
} Mark;

#define MKFF_RESIDENT (1 << CELL8_TYPE_BITS)


/* A buffer, strangely called `TX' */
typedef struct _TX {
    VALUE	    tx_Car;
#define tx_Flags tx_Car

    struct _TX	   *tx_Next;
    Mark	   *tx_MarkChain;
    LINE	   *tx_Lines;
    long	    tx_NumLines, tx_TotalLines;	/* text-lines, array-length */

    /* line numbers of `narrowed' region */
    long	    tx_LogicalStart, tx_LogicalEnd;
    long	    tx_LastLogicalStart, tx_LastLogicalEnd;

    VALUE	    tx_FileName;
    VALUE	    tx_BufferName;
    VALUE	    tx_ModeName;
    VALUE	    tx_MinorModeNameList;
    VALUE	    tx_MinorModeNameString;

    int		    tx_Changes;
    int		    tx_LastSaveChanges;	 /* changes at last save (any type) */
    int		    tx_ProperSaveChanges; /* changes at last `proper' save */
    int		    tx_AutoSaveInterval; /* seconds between saves */
    long	    tx_LastSaveTime;	 /* time at last save (auto or user) */

    int		    tx_TabSize;

    /* Section of buffer which may have changed since last refresh.  */
    VALUE	    tx_ModStart, tx_ModEnd;
    /* How many more lines in the above area than at the last refresh.	*/
    long	    tx_ModDelta;
    /* `tx_Changes' at last refresh.  */
    long	    tx_LastChanges;

    VALUE	    tx_LocalVariables; /* alist of (SYMBOL . VALUE) */
    VALUE	    tx_GlyphTable;

    /* Undo information */
    VALUE	    tx_UndoList;
    VALUE	    tx_ToUndoList;
    VALUE	    tx_UndoneList;

    /* Saved state for buffers which are not being displayed.  */
    VALUE	    tx_SavedCPos;
    VALUE	    tx_SavedWPos;
    VALUE	    tx_SavedBlockPos[2];
    char	    tx_SavedBlockStatus;
} TX;

/* No modifications to file */
#define TXFF_RDONLY		(1 << (CELL8_TYPE_BITS + 0))

/* No mod flag, buffer never killed. */
#define TXFF_SPECIAL		(1 << (CELL8_TYPE_BITS + 1))

/* *All* buffer has changed. */
#define TXFF_REFRESH_ALL	(1 << (CELL8_TYPE_BITS + 2))

/* No recording of undo information */
#define TXFF_NO_UNDO		(1 << (CELL8_TYPE_BITS + 3))

/* Views displaying this buffer need to update their status lines. */
#define TXFF_REFRESH_STATUS	(1 << (CELL8_TYPE_BITS + 4))


/* Each view in a window is like this */
typedef struct _VW
{
    VALUE	    vw_Car;
#define vw_Flags vw_Car

    struct _VW	   *vw_Next;
    TX		   *vw_Tx;
    struct _WIN	   *vw_Win;
    struct _VW	   *vw_NextView;	/* for w_ViewList */

    /* Cursor positioning data.  */
    VALUE	    vw_CursorPos;
    u_long	    vw_LastCursorOffset; /* number of glyphs from col 0 */
    VALUE	    vw_LastCursorPos;
    u_long	    vw_LastCursorChanges;
    TX		   *vw_LastCursorTx;

    VALUE	    vw_DisplayOrigin;
    VALUE	    vw_LastDisplayOrigin;

    VALUE	    vw_BlockS, vw_BlockE;
    VALUE	    vw_LastBlockS, vw_LastBlockE;
    /* 0=block marked, 1=start marked, 2=end marked, -1=none marked */
    char	    vw_BlockStatus, vw_LastBlockStatus;

    int		    vw_MaxX, vw_MaxY;	/* COLS,ROWS of pane, not including
					   status line at bottom */
    int		    vw_WidthPix, vw_HeightPix;
    int		    vw_LeftPix, vw_RightPix;
    int		    vw_TopPix, vw_BottomPix;

    TX		   *vw_LastRefTx;
    short	    vw_DeferRefresh;

    u_char	   *vw_StatusBuf;
#define STATUS_BUFSIZ 128

#ifndef NOSCRLBAR
    ScrollBar	    vw_SBar;
#endif

    /* List of buffers accessible in this window.  This is not used by the
       C code at all; access is via the `buffer-list' variable.  */
    VALUE	    vw_BufferList;

    short	    vw_XStepRatio, vw_YStepRatio;
    short	    vw_XStep, vw_YStep;
    u_short	    vw_MaxScroll;
} VW;

/* mark rectangular blocks */
#define VWFF_RECTBLOCKS		(1 << (CELL8_TYPE_BITS + 0))

/* full redraw next time */
#define VWFF_FORCE_REFRESH	(1 << (CELL8_TYPE_BITS + 1))

/* redraw the block */
#define VWFF_REFRESH_BLOCK	(1 << (CELL8_TYPE_BITS + 2))

/* redraw the status line */
#define VWFF_REFRESH_STATUS	(1 << (CELL8_TYPE_BITS + 3))

/* view is of a minibuffer */
#define VWFF_MINIBUF		(1 << (CELL8_TYPE_BITS + 4))

/* status text set by user */
#define VWFF_CUSTOM_STATUS	(1 << (CELL8_TYPE_BITS + 5))


/* Each window is represented by one of these */
typedef struct _WIN {
    VALUE w_Car;
#define w_Flags w_Car

    struct _WIN *w_Next;

    VW *w_ViewList;
    VW *w_CurrVW;
    VW *w_MiniBuf;

    int w_ViewCount;

    /* Data that the window-system needs.  */
    W_WindowSys w_WindowSys;

    u_char *w_Message;
    u_long w_MessageLen;
    u_long w_LastClickMics;

    int w_MaxX, w_MaxY;			/* COLS,ROWS in whole window */
    int w_WidthPix, w_HeightPix;	/* width,height in pixels of window */
    int w_LeftPix, w_RightPix;		/* Pixel position of top left corner */
    int w_TopPix, w_BottomPix;		/*  "      "   of bottom right */

    VALUE w_FontName;
    short w_FontStart;
    short w_FontX, w_FontY;
} WIN;

/* refresh whole window */
#define WINFF_FORCE_REFRESH	(1 << (CELL8_TYPE_BITS + 0))

/* window is iconified */
#define WINFF_SLEEPING		(1 << (CELL8_TYPE_BITS + 1))

/* a message is currently displayed in the minibuffer */
#define WINFF_MESSAGE		(1 << (CELL8_TYPE_BITS + 2))

/* True when the minibuffer in WIN is in use. */
#define MINIBUFFER_ACTIVE_P(win) \
    ((win)->w_MiniBuf->vw_Tx != mb_unused_buffer)

#define CURS_ON	 1
#define CURS_OFF 0

#endif /* _EDIT_H */
