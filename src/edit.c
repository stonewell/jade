/* edit.c -- Editing buffers
   Copyright (C) 1993, 1994 John Harper <john@dcs.warwick.ac.uk>
   $Id$

   This file is part of Jade.

   Jade is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   Jade is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Jade; see the file COPYING.	If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "jade.h"
#include "jade_protos.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef NEED_MEMORY_H
# include <memory.h>
#endif

/* The maximum number of unused line entries at the end of the
   array of lines in each buffer. When allocating new line arrays
   allocate half this many extra lines (in case it grows _or_ shrinks). */
#define MAX_SPARE_LINES 32
#define ALLOC_SPARE_LINES (MAX_SPARE_LINES / 2)

/* Strings stored in LINEs are allocated using stringmem.c, this
   means that small strings are always allocated in chunks of GRAIN
   (currently eight) bytes. So it makes sense to allocate lines in
   multiples of this number; allowing many unnecessary re-allocations
   to be avoided. */

/* For a piece of memory of size X, this is the number of bytes we'll
   actually ask to be allocated. This assumes that GRAIN is some power
   of two. */
#define LINE_BUF_SIZE(x) ROUND_UP_INT(x, GRAIN)

/* Allocate a chunk of memory to store a string of size X (including
   terminating zero). */
#define ALLOC_LINE_BUF(x) str_alloc(LINE_BUF_SIZE(x))

/* Free something allocated with the previous macro. */
#define FREE_LINE_BUF(p)  str_free(p)

/* Allocate a buffer to contain N LINE structures. */
#define ALLOC_LL(n)   mymalloc(sizeof(LINE) * (n))

/* Free one of the above */
#define FREE_LL(l)    myfree(l)

_PR bool clear_line_list(TX *);
_PR void kill_line_list(TX *);
_PR LINE *resize_line_list(TX *, long, long);
_PR u_char *alloc_line_buf(long length);
_PR bool insert_gap(TX *, long, VALUE);
_PR VALUE insert_bytes(TX *, const u_char *, long, VALUE);
_PR VALUE insert_string(TX *, const u_char *, long, VALUE);
_PR VALUE delete_chars(TX *, VALUE, long);
_PR VALUE delete_section(TX *, VALUE, VALUE);
_PR bool pad_pos(TX *, VALUE);
_PR bool pad_cursor(VW *);
_PR void order_pos(VALUE *, VALUE *);
_PR bool check_section(TX *, VALUE *, VALUE *);
_PR VALUE check_pos(TX *, VALUE);
_PR bool check_line(TX *, VALUE);
_PR long section_length(TX *, VALUE, VALUE);
_PR void copy_section(TX *, VALUE, VALUE, u_char *);
_PR bool pos_in_block(VW *, long, long);
_PR bool cursor_in_block(VW *);
_PR bool page_in_block(VW *);
_PR short line_in_block(VW *, long);
_PR void order_block(VW *);
_PR void set_block_refresh(VW *);
_PR bool read_only(TX *);

/* Makes buffer TX empty (null string in first line) */
bool
clear_line_list(TX *tx)
{
    if(tx->tx_Lines)
	kill_line_list(tx);
    tx->tx_Lines = ALLOC_LL(ALLOC_SPARE_LINES);
    if(tx->tx_Lines)
    {
	tx->tx_Lines[0].ln_Line = ALLOC_LINE_BUF(1);
	if(tx->tx_Lines[0].ln_Line)
	{
	    tx->tx_Lines[0].ln_Line[0] = 0;
	    tx->tx_Lines[0].ln_Strlen = 1;
	}
	else
	    tx->tx_Lines[0].ln_Strlen = 0;
	tx->tx_NumLines = 1;
	tx->tx_TotalLines = ALLOC_SPARE_LINES;
	tx->tx_LogicalStart = 0;
	tx->tx_LogicalEnd = 1;
	return(TRUE);
    }
    return(FALSE);
}

/*
 * deallocates all lines and their list
 */
void
kill_line_list(TX *tx)
{
    if(tx->tx_Lines)
    {
	LINE *line;
	long i;
	for(i = 0, line = tx->tx_Lines; i < tx->tx_NumLines; i++, line++)
	{
	    if(line->ln_Strlen)
		FREE_LINE_BUF(line->ln_Line);
	}
	FREE_LL(tx->tx_Lines);
	tx->tx_Lines = NULL;
	tx->tx_NumLines = 0;
	tx->tx_TotalLines = 0;
    }
}

/*
 * deallocates some lines (but not the list)
 */
static void
kill_some_lines(TX *tx, long start, long number)
{
    LINE *line = tx->tx_Lines + start;
    long i;
    for(i = 0; i < number; i++, line++)
    {
	if(line->ln_Strlen)
	{
	    FREE_LINE_BUF(line->ln_Line);
	    line->ln_Strlen = 0;
	    line->ln_Line = NULL;
	}
    }
}

/* Creates blank entries or removes existing lines starting from line
   WHERE. CHANGE is the number of lines to insert, negative numbers mean
   delete that number of lines starting at the cursor line. If lines are
   deleted the actual text is also deleted.
   NOTE: A line list of zero lines is not allowed. */
LINE *
resize_line_list(TX *tx, long change, long where)
{
    long newsize = tx->tx_NumLines + change;
    if(newsize <= 0)
	return NULL;
    if(tx->tx_Lines != NULL
       && newsize <= tx->tx_TotalLines
       && (tx->tx_TotalLines - newsize) <= MAX_SPARE_LINES)
    {
	/* Just use/create slack in the line array */
	if(change > 0)
	{
	    /* Make some space */
	    memmove(tx->tx_Lines + where + change,
		    tx->tx_Lines + where,
		    (tx->tx_NumLines - where) * sizeof(LINE));
	}
	else if(change < 0)
	{
	    /* Lose some lines */
	    kill_some_lines(tx, where, -change);
	    memmove(tx->tx_Lines + where,
		    tx->tx_Lines + where - change,
		    (tx->tx_NumLines - where + change) * sizeof(LINE));
	}
    }
    else
    {
	LINE *newlines = ALLOC_LL(newsize + ALLOC_SPARE_LINES);
	if(newlines == 0)
	    return FALSE;
	if(tx->tx_Lines)
	{
	    memcpy(newlines, tx->tx_Lines, where * sizeof(LINE));
	    if(change > 0)
	    {
		memcpy(newlines + where + change,
		       tx->tx_Lines + where,
		       (tx->tx_NumLines - where) * sizeof(LINE));
	    }
	    else
	    {
		memcpy(newlines + where,
		       tx->tx_Lines + where - change,
		       (tx->tx_NumLines - where + change) * sizeof(LINE));
		kill_some_lines(tx, where, -change);
	    }
	    FREE_LL(tx->tx_Lines);
	}
	tx->tx_Lines = newlines;
	tx->tx_TotalLines = newsize + ALLOC_SPARE_LINES;
    }
    if(change > 0)
	memset(tx->tx_Lines + where, 0, change * sizeof(LINE));
    tx->tx_NumLines = newsize;
    return tx->tx_Lines;
}

u_char *
alloc_line_buf(long length)
{
    return ALLOC_LINE_BUF(length);
}

/* Inserts LEN characters of `space' at pos. The gap will be filled
   with random garbage. */
bool
insert_gap(TX *tx, long len, VALUE pos)
{
    LINE *line = tx->tx_Lines + VROW(pos);
    long new_length = line->ln_Strlen + len;
    if(LINE_BUF_SIZE(new_length) == LINE_BUF_SIZE(line->ln_Strlen))
    {
	/* Absorb the insertion in the current buffer */
	memmove(line->ln_Line + VCOL(pos) + len,
		line->ln_Line + VCOL(pos), line->ln_Strlen - VCOL(pos));
    }
    else
    {
	/* Need to allocate a new buffer */
	u_char *newline = ALLOC_LINE_BUF(new_length);
	if(newline != NULL)
	{
	    if(line->ln_Strlen != 0)
	    {
		memcpy(newline, line->ln_Line, VCOL(pos));
		memcpy(newline + VCOL(pos) + len, line->ln_Line + VCOL(pos),
		       line->ln_Strlen - VCOL(pos));
		FREE_LINE_BUF(line->ln_Line);
	    }
	    else
		newline[len] = 0;
	    line->ln_Line = newline;
	}
	else
	{
	    mem_error();
	    return FALSE;
	}
    }
    line->ln_Strlen += len;
    adjust_marks_add_x(tx, len, VCOL(pos), VROW(pos));
    return TRUE;
}

/* Inserts a piece of memory into the current line at POS.
   No line-breaking is performed, the TEXTLEN bytes of TEXT are simply
   inserted into the current line. Returns the position of the character
   after the end of the inserted text. */
VALUE
insert_bytes(TX *tx, const u_char *text, long textLen, VALUE pos)
{
    LINE *line = tx->tx_Lines + VROW(pos);
    if(insert_gap(tx, textLen, pos))
    {
	memcpy(line->ln_Line + VCOL(pos), text, textLen);
	return make_pos(VCOL(pos) + textLen, VROW(pos));
    }
    else
	return LISP_NULL;
}

/* Inserts a string, this routine acts on any '\n' characters that it
   finds. */
VALUE
insert_string(TX *tx, const u_char *text, long textLen, VALUE pos)
{
    const u_char *eol;
    Pos tpos = *VPOS(pos);
    while((eol = memchr(text, '\n', textLen)))
    {
	long len = eol - text;
	if(PCOL(&tpos) != 0)
	{
	    if(len > 0)
	    {
		LINE *line = tx->tx_Lines + PROW(&tpos);
		if(insert_gap(tx, len, VAL(&tpos)))
		{
		    memcpy(line->ln_Line + PCOL(&tpos), text, len);
		    PCOL(&tpos) += len;
		}
		else
		    goto abort;
	    }
	    /* Split line at TPOS */
	    if(resize_line_list(tx, +1, PROW(&tpos) + 1))
	    {
		LINE *line = tx->tx_Lines + PROW(&tpos);

		/* First do the new line */
		line[1].ln_Line = ALLOC_LINE_BUF(line[0].ln_Strlen
						 - PCOL(&tpos));
		if(line[1].ln_Line != NULL)
		{
		    line[1].ln_Strlen = line[0].ln_Strlen - PCOL(&tpos);
		    memcpy(line[1].ln_Line, line[0].ln_Line + PCOL(&tpos),
			   line[1].ln_Strlen - 1);
		    line[1].ln_Line[line[1].ln_Strlen - 1] = 0;
		}
		else
		    goto abort;

		/* Then chop the end off the old one */
		if(LINE_BUF_SIZE(PCOL(&tpos) + 1)
		   == LINE_BUF_SIZE(line[0].ln_Strlen))
		{
		    /* Use the old buffer */
		    line[0].ln_Strlen = PCOL(&tpos) + 1;
		    line[0].ln_Line[line[0].ln_Strlen - 1] = 0;
		}
		else
		{
		    /* Allocate a new buffer */
		    u_char *new = ALLOC_LINE_BUF(PCOL(&tpos) + 1);
		    if(new != NULL)
		    {
			memcpy(new, line[0].ln_Line, PCOL(&tpos));
			new[PCOL(&tpos)] = 0;
			FREE_LINE_BUF(line[0].ln_Line);
			line[0].ln_Line = new;
			line[0].ln_Strlen = PCOL(&tpos) + 1;
		    }
		    else
			goto abort;
		}
		adjust_marks_split_y(tx, PCOL(&tpos), PROW(&tpos));
		PCOL(&tpos) = 0;
		PROW(&tpos)++;
	    }
	    else
		goto abort;
	}
	else
	{
	    u_char *copy;
	    if(!resize_line_list(tx, +1, PROW(&tpos)))
		goto abort;
	    copy = ALLOC_LINE_BUF(len + 1);
	    if(copy == NULL)
		goto abort;
	    memcpy(copy, text, len);
	    copy[len] = 0;
	    tx->tx_Lines[PROW(&tpos)].ln_Strlen = len + 1;
	    tx->tx_Lines[PROW(&tpos)].ln_Line = copy;
	    adjust_marks_add_y(tx, +1, PROW(&tpos));
	    PROW(&tpos)++;
	}
	textLen -= len + 1;
	text = eol + 1;
    }
    if(textLen > 0)
    {
	LINE *line = tx->tx_Lines + PROW(&tpos);
	if(insert_gap(tx, textLen, VAL(&tpos)))
	{
	    memcpy(line->ln_Line + PCOL(&tpos), text, textLen);
	    PCOL(&tpos) += textLen;
	}
	else
	abort:
	    return LISP_NULL;
    }

    {
	VALUE end = make_pos(PCOL(&tpos), PROW(&tpos));
	undo_record_insertion(tx, pos, end);
	flag_insertion(tx, pos, end);
	return end;
    }
}

/* Deletes some SIZE bytes from line at POS. Returns POS if okay.
   This won't delete past the end of the line at POS. */
VALUE
delete_chars(TX *tx, VALUE pos, long size)
{
    LINE *line = tx->tx_Lines + VROW(pos);
    if(line->ln_Strlen)
    {
	long new_length;
	if(size >= line->ln_Strlen - VCOL(pos))
	    size = line->ln_Strlen - VCOL(pos) - 1;
	if(size <= 0)
	    return LISP_NULL;
	new_length = line->ln_Strlen - size;
	if(LINE_BUF_SIZE(new_length) == LINE_BUF_SIZE(line->ln_Strlen))
	{
	    /* Absorb the deletion */
	    memmove(line->ln_Line + VCOL(pos),
		    line->ln_Line + VCOL(pos) + size,
		    line->ln_Strlen - (VCOL(pos) + size));
	}
	else
	{
	    /* Allocate a new line */
	    u_char *new_line = ALLOC_LINE_BUF(new_length);
	    if(new_line == NULL)
	    {
		mem_error();
		return LISP_NULL;
	    }
            memcpy(new_line, line->ln_Line, VCOL(pos));
            memcpy(new_line + VCOL(pos), line->ln_Line + VCOL(pos) + size,
                   line->ln_Strlen - VCOL(pos) - size);
	    FREE_LINE_BUF(line->ln_Line);
	    line->ln_Line = new_line;
	}
	line->ln_Strlen -= size;
	adjust_marks_sub_x(tx, size, VCOL(pos), VROW(pos));
	return pos;
    }
    return LISP_NULL;
}

/* Deletes from START to END; returns END if okay. */
VALUE
delete_section(TX *tx, VALUE start, VALUE end)
{
    undo_record_deletion(tx, start, end);
    if(VROW(end) == VROW(start))
    {
	delete_chars(tx, start, VCOL(end) - VCOL(start));
	flag_deletion(tx, start, end);
    }
    else
    {
	long middle_lines;
	bool joinflag = FALSE;
	Pos tstart = *VPOS(start), tend = *VPOS(end);
	if(PCOL(&tstart) != 0)
	{
	    long start_col = (tx->tx_Lines[PROW(&tstart)].ln_Strlen
			      - PCOL(&tstart) - 1);
	    if(start_col != 0)
		delete_chars(tx, VAL(&tstart), start_col);
	    PCOL(&tstart) = 0;
	    PROW(&tstart)++;
	    joinflag = TRUE;
	}
	middle_lines = PROW(&tend) - PROW(&tstart);
	if(middle_lines != 0)
	{
	    if(!resize_line_list(tx, -middle_lines, PROW(&tstart)))
		mem_error();
	    adjust_marks_sub_y(tx, middle_lines, PROW(&tstart));
	    PROW(&tend) = PROW(&tend) - middle_lines;
	}
	if(PCOL(&tend) != 0)
	    delete_chars(tx, VAL(&tstart), PCOL(&tend));
	if(joinflag && PROW(&tstart) != 0)
	{
	    PROW(&tstart)--;
	    PCOL(&tstart) = tx->tx_Lines[PROW(&tstart)].ln_Strlen - 1;

	    /* Join the two lines at TSTART */
	    if((PROW(&tstart) + 1) < tx->tx_LogicalEnd)
	    {
		LINE *line1 = tx->tx_Lines + PROW(&tstart);
		LINE *line2 = line1 + 1;
		if(line1->ln_Strlen == 1 || line2->ln_Strlen == 1)
		{
		    /* One (or both) of the lines being joined is
		       empty; so just use the other line */
		    if(line2->ln_Strlen == 1)
		    {
			u_char *tem = line1->ln_Line;
			line1->ln_Line = line2->ln_Line;
			line2->ln_Line = tem;
			line2->ln_Strlen = line1->ln_Strlen;
			line1->ln_Strlen = 1;
		    }
		}
		else
		{
		    /* Allocate a new line;
		       TODO: see if the join can be absorbed into one
		       of the existing lines.. */
		    int new_length = line1->ln_Strlen + line2->ln_Strlen - 1;
		    u_char *new_line = ALLOC_LINE_BUF(new_length);
		    if(new_line == NULL)
		    {
			mem_error();
			return LISP_NULL;
		    }
		    memcpy(new_line, line1->ln_Line, line1->ln_Strlen - 1);
		    memcpy(new_line + (line1->ln_Strlen - 1),
			   line2->ln_Line, line2->ln_Strlen);
		    FREE_LINE_BUF(line2->ln_Line);
		    line2->ln_Line = new_line;
		    line2->ln_Strlen = new_length;
		}
		resize_line_list(tx, -1, PROW(&tstart));
		adjust_marks_join_y(tx, PCOL(&tstart), PROW(&tstart));
	    }
	    else
		abort();		/* shouldn't happen :-) */
	}
	flag_deletion(tx, start, end);
    }
    return end;
}

/* Inserts spaces from end of line to pos */
bool
pad_pos(TX *tx, VALUE pos)
{
    if(VROW(pos) < tx->tx_LogicalEnd)
    {
	LINE *line = tx->tx_Lines + VROW(pos);
	if(line->ln_Strlen < (VCOL(pos) + 1))
	{
	    VALUE point = make_pos(line->ln_Strlen - 1, VROW(pos));
	    if(insert_gap(tx, VCOL(pos) - VCOL(point), point))
	    {
		undo_record_insertion(tx, point, pos);
		memset(line->ln_Line + VCOL(point), ' ',
		       VCOL(pos) - VCOL(point));
		return TRUE;
	    }
	    mem_error();
	    return FALSE;
	}
	return TRUE;
    }
    return FALSE;
}

bool
pad_cursor(VW *vw)
{
    return(pad_pos(vw->vw_Tx, vw->vw_CursorPos));
}

/* if end is before start then swap the two */
void
order_pos(VALUE *start, VALUE *end)
{
    if(POS_GREATER_P(*start, *end))
    {
	VALUE tem = *end;
	*end = *start;
	*start = tem;
    }
}

bool
check_section(TX *tx, VALUE *start, VALUE *end)
{
    order_pos(start, end);
    if((VROW(*start) >= tx->tx_LogicalEnd)
       || (VROW(*end) >= tx->tx_LogicalEnd)
       || (VROW(*start) < tx->tx_LogicalStart)
       || (VROW(*end) < tx->tx_LogicalStart))
    {
	cmd_signal(sym_invalid_area,
		   list_3(VAL(tx), *start, *end));
	return(FALSE);
    }
    if(VCOL(*start) >= tx->tx_Lines[VROW(*start)].ln_Strlen)
	*start = make_pos(tx->tx_Lines[VROW(*start)].ln_Strlen - 1,
			  VROW(*start));
    if(VCOL(*end) >= tx->tx_Lines[VROW(*end)].ln_Strlen)
	*end = make_pos(tx->tx_Lines[VROW(*end)].ln_Strlen - 1, VROW(*end));
    return TRUE;
}

VALUE
check_pos(TX *tx, VALUE pos)
{
    if(VROW(pos) >= tx->tx_LogicalEnd
       || VROW(pos) < tx->tx_LogicalStart)
    {
	cmd_signal(sym_invalid_pos, list_2(VAL(tx), pos));
	return FALSE;
    }
    if(VCOL(pos) >= tx->tx_Lines[VROW(pos)].ln_Strlen)
	pos = make_pos(tx->tx_Lines[VROW(pos)].ln_Strlen - 1, VROW(pos));
    return pos;
}

bool
check_line(TX *tx, VALUE pos)
{
    if((VROW(pos) >= tx->tx_LogicalEnd)
       || (VROW(pos) < tx->tx_LogicalStart)
       || (VCOL(pos) < 0))
    {
	cmd_signal(sym_invalid_pos, list_2(VAL(tx), pos));
	return FALSE;
    }
    return TRUE;
}

/* Returns the number of bytes needed to store a section, doesn't include
   a zero terminator but does include all newline chars. */
long
section_length(TX *tx, VALUE startPos, VALUE endPos)
{
    long linenum = VROW(startPos);
    LINE *line = tx->tx_Lines + linenum;
    long length;
    if(VROW(startPos) == VROW(endPos))
	length = VCOL(endPos) - VCOL(startPos);
    else
    {
	length = line->ln_Strlen - VCOL(startPos);
	linenum++;
	line++;
	while(linenum < VROW(endPos))
	{
	    length += line->ln_Strlen;
	    linenum++;
	    line++;
	}
	length += VCOL(endPos);
    }
    return length;
}

/* Copies a section to a buffer.
   end of copy does NOT have a zero appended to it. */
void
copy_section(TX *tx, VALUE startPos, VALUE endPos, u_char *buff)
{
    long linenum = VROW(startPos);
    LINE *line = tx->tx_Lines + linenum;
    long copylen;
    if(VROW(startPos) == VROW(endPos))
    {
	copylen = VCOL(endPos) - VCOL(startPos);
	memcpy(buff, line->ln_Line + VCOL(startPos), copylen);
	buff[copylen] = 0;
    }
    else
    {
	copylen = line->ln_Strlen - VCOL(startPos) - 1;
	memcpy(buff, line->ln_Line + VCOL(startPos), copylen);
	buff[copylen] = '\n';
	buff += copylen + 1;
	linenum++;
	line++;
	while(linenum < VROW(endPos))
	{
	    copylen = line->ln_Strlen - 1;
	    memcpy(buff, line->ln_Line, copylen);
	    buff[copylen] = '\n';
	    buff += copylen + 1;
	    linenum++;
	    line++;
	}
	memcpy(buff, line->ln_Line, VCOL(endPos));
    }
}

/* returns TRUE if the specified position is inside a block */
bool
pos_in_block(VW *vw, long col, long line)
{
    if((vw->vw_BlockStatus)
       || (line < VROW(vw->vw_BlockS))
       || (line > VROW(vw->vw_BlockE)))
	return FALSE;
    if(vw->vw_Flags & VWFF_RECTBLOCKS)
    {
	long start_col = glyph_col(vw->vw_Tx, VCOL(vw->vw_BlockS),
				   VROW(vw->vw_BlockS));

	long end_col = glyph_col(vw->vw_Tx, VCOL(vw->vw_BlockE),
				 VROW(vw->vw_BlockE));
	col = glyph_col(vw->vw_Tx, col, line);
	if(start_col < end_col)
	{
	    if((col < start_col) || (col >= end_col))
		return FALSE;
	}
	else
	{
	    if((col < end_col) || (col >= start_col))
		return FALSE;
	}
    }
    else
    {
	if(((line == VROW(vw->vw_BlockS)) && (col < VCOL(vw->vw_BlockS)))
	  || ((line == VROW(vw->vw_BlockE)) && (col >= VCOL(vw->vw_BlockE))))
	return FALSE;
    }
    return TRUE;
}

bool
cursor_in_block(VW *vw)
{
    return(pos_in_block(vw, VCOL(vw->vw_CursorPos), VROW(vw->vw_CursorPos)));
}


bool
page_in_block(VW *vw)
{
    if((vw->vw_BlockStatus)
      || ((VROW(vw->vw_BlockE) + 1) < VROW(vw->vw_DisplayOrigin))
      || (VROW(vw->vw_BlockS) > VROW(vw->vw_DisplayOrigin) + vw->vw_MaxY))
	return(FALSE);
    return(TRUE);
}

/*
 * these returns,
 *
 *	0   line not in block
 *	1   whole line in block
 *	2   start of line in block
 *	3   end of line in block
 *	4   middle of line in block
 *
 * note:
 *  this isn't very intelligent (but it works :-).
 *
 * now handles rectangular blocks (VWFF_RECTBLOCKS)
 */
short
line_in_block(VW *vw, long line)
{
    bool startin = FALSE;
    bool endin = FALSE;

    if((vw->vw_BlockStatus)
      || (line < VROW(vw->vw_BlockS))
      || (line > VROW(vw->vw_BlockE)))
	return 0;
    if(vw->vw_Flags & VWFF_RECTBLOCKS)
	return 4;
    if(line == VROW(vw->vw_BlockE))
	startin = TRUE;
    if(line == VROW(vw->vw_BlockS))
	endin = TRUE;
    if(startin)
    {
	if(endin)
	    return 4;
	return 2;
    }
    if(endin)
	return 3;
    return 1;
}

/*
 * makes sure that the marked block is valid
 */
void
order_block(VW *vw)
{
    if(!vw->vw_BlockStatus)
    {
	if(VROW(vw->vw_BlockS) > VROW(vw->vw_BlockE)
	   || (VROW(vw->vw_BlockS) == VROW(vw->vw_BlockE)
	       && VCOL(vw->vw_BlockS) > VCOL(vw->vw_BlockE)))
	{
	    VALUE tem = vw->vw_BlockE;
	    vw->vw_BlockE = vw->vw_BlockS;
	    vw->vw_BlockS = tem;
	}
    }
}

/*
 * Set up the refresh flags to refresh the block in the most efficient manner.
 */
void
set_block_refresh(VW *vw)
{
    vw->vw_Flags |= VWFF_REFRESH_BLOCK;
}

bool
read_only(TX *tx)
{
    if(tx->tx_Flags & TXFF_RDONLY)
    {
	VALUE tmp = cmd_symbol_value(sym_inhibit_read_only, sym_t);
	if(VOIDP(tmp) || NILP(tmp))
	{
	    cmd_signal(sym_buffer_read_only, LIST_1(VAL(tx)));
	    return(TRUE);
	}
    }
    return(FALSE);
}
