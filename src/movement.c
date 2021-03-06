/* movement.c -- Positioning the cursor
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

#include "jade.h"
#include <ctype.h>


DEFSYM(next_screen_context_lines, "next-screen-context-lines"); /*
::doc:next-screen-context-lines::
This variable controls the number of lines of "overlap" when scrolling
by screenfuls of text.
::end:: */

DEFUN("goto", Fgoto, Sgoto, (repv pos), rep_Subr1) /*
::doc:goto::
goto POSITION

Set the cursor position in the current window to the character position
POSITION.
::end:: */
{
    Lisp_View *vw = curr_vw;
    rep_DECLARE1(pos, POSP);
    if(check_line(vw->tx, pos))
    {
	vw->cursor_pos = pos;
	return(pos);
    }
    else
	return(Qnil);
}

DEFUN("goto-glyph", Fgoto_glyph, Sgoto_glyph, (repv pos), rep_Subr1) /*
::doc:goto-glyph::
goto-glyph POSITION

Set the cursor position in the current window to the glyph position POSITION.
::end:: */
{
    Lisp_View *vw = curr_vw;
    rep_DECLARE1(pos, POSP);
    if(check_line(vw->tx, pos))
    {
	vw->cursor_pos = make_pos(char_col(vw->tx, VCOL(pos), VROW(pos)),
				    VROW(pos));
	return(pos);
    }
    else
	return(Qnil);
}

DEFUN_INT("center-display", Fcenter_display, Scenter_display, (repv vw, repv arg), rep_Subr2, "\nP") /*
::doc:center-display::
center-display [VIEW] [ARG]

When ARG is nil arrange it so that the line that the cursor is on is
displayed in the middle of the view (if possible).

If ARG is non-nil it should be a number, negative numbers mean that many
lines from the bottom of the view, other numbers count from the top of the
view.
::end:: */
{
    intptr_t offset;
    intptr_t col, row;

    if(!VIEWP(vw))
	vw = rep_VAL(curr_vw);

    if(rep_NILP(arg) || rep_CONSP(arg))
	offset = VVIEW(vw)->height / 2;
    else if(rep_SYMBOLP(arg))
	offset = VVIEW(vw)->height - 1;
    else if(rep_INTP(arg))
    {
	if(rep_INT(arg) < 0)
	    offset = VVIEW(vw)->height + rep_INT(arg);
	else
	    offset = rep_INT(arg);
    }
    else
	offset = 0;

    if(!skip_glyph_rows_backwards(VVIEW(vw), offset,
				  VCOL(VVIEW(vw)->cursor_pos),
				  VROW(VVIEW(vw)->cursor_pos),
				  &col, &row))
    {
	col = 0;
	row = 0;
    }

    if(row < VVIEW(vw)->tx->logical_start)
	row = VVIEW(vw)->tx->logical_start;
    if(row >= VVIEW(vw)->tx->logical_end)
	row = VVIEW(vw)->tx->logical_end - 1;
    VVIEW(vw)->display_origin = make_pos(col, row);
    return VVIEW(vw)->display_origin;
}

DEFUN_INT("next-screen", Fnext_screen, Snext_screen, (repv number), rep_Subr1, "p") /*
::doc:next-screen::
next-screen [NUMBER]

Move NUMBER (default: 1) screens forwards in the current window.
::end:: */
{
    intptr_t lines = (rep_INTP(number) ? rep_INT(number) : 1) * curr_vw->height;
    intptr_t col, row;
    repv context;
    if(lines < 0)
	return Fprev_screen(rep_MAKE_INT(-lines / curr_vw->height));
    context = Fsymbol_value(Qnext_screen_context_lines, Qt);
    if(rep_INTP(context) && lines > rep_INT(context) + 1)
	lines -= rep_INT(context);

    if(VROW(curr_vw->cursor_pos) == curr_vw->tx->logical_end - 1)
	return Qnil;
    else if(curr_vw->car & VWFF_AT_BOTTOM)
    {
	set_cursor_vertically(curr_vw, curr_vw->tx->logical_end - 1);
	return curr_vw->display_origin;
    }
    else if(skip_glyph_rows_forwards(curr_vw, lines,
				     VCOL(curr_vw->display_origin),
				     VROW(curr_vw->display_origin),
				     &col, &row))
    {
	curr_vw->display_origin = make_pos(col, row);
	if(POS_GREATER_P(curr_vw->display_origin, curr_vw->cursor_pos))
	    set_cursor_vertically(curr_vw, VROW(curr_vw->display_origin));
	return curr_vw->display_origin;
    }
    else
	return Qnil;
}

DEFUN_INT("prev-screen", Fprev_screen, Sprev_screen, (repv number), rep_Subr1, "p") /*
::doc:prev-screen::
prev-screen [NUMBER]

Move NUMBER (default: 1) screens backwards in the current window.
::end:: */
{
    intptr_t lines = (rep_INTP(number) ? rep_INT(number) : 1) * curr_vw->height;
    intptr_t col, row;
    repv context, new_origin;
    if(lines < 0)
	return Fnext_screen(rep_MAKE_INT(-lines / curr_vw->height));

    context = Fsymbol_value(Qnext_screen_context_lines, Qt);
    if(rep_INTP(context) && lines > rep_INT(context) + 1)
	lines -= rep_INT(context);
    if(skip_glyph_rows_backwards(curr_vw, lines,
				 VCOL(curr_vw->display_origin),
				 VROW(curr_vw->display_origin),
				 &col, &row))
	new_origin = make_pos(col, row);
    else if(VROW(curr_vw->display_origin) != curr_vw->tx->logical_start)
	new_origin = make_pos(0, curr_vw->tx->logical_start);
    else if(VROW(curr_vw->cursor_pos) != curr_vw->tx->logical_start)
    {
	set_cursor_vertically(curr_vw, curr_vw->tx->logical_start);
	return curr_vw->display_origin;
    }
    else
	return Qnil;

    /* Now fix the cursor position. */
    if(skip_glyph_rows_forwards(curr_vw, curr_vw->height - 1,
				VCOL(new_origin), VROW(new_origin),
				&col, &row))
    {
	intptr_t curs_offset = get_cursor_column(curr_vw);
	if(VROW(curr_vw->cursor_pos) > row
	   || (VROW(curr_vw->cursor_pos) == row
	       && curs_offset > col))
	{
	    /* TODO: the column isn't correct */
	    curr_vw->cursor_pos = make_pos(col, row);
	}
    }

    curr_vw->display_origin = new_origin;
    return curr_vw->display_origin;
}

DEFUN_INT("end-of-buffer", Fend_of_buffer, Send_of_buffer,
	  (repv tx, repv irp), rep_Subr2, "!@") /*
::doc:end-of-buffer::
end-of-buffer [BUFFER] [IGNORE-RESTRICTION-P]

Return the position of the last character in BUFFER. Unless
IGNORE-RESTRICTION-P is non-nil the position returned is the end
of the buffer's restriction.
::end:: */
{
    if(!BUFFERP(tx))
	tx = rep_VAL(curr_vw->tx);
    if(!rep_NILP(irp))
    {
	intptr_t x, y;
	y = VBUFFER(tx)->line_count - 1;
	x = VBUFFER(tx)->lines[y].ln_Strlen - 1;
	return make_pos(x, y);
    }
    else
	return Frestriction_end(tx);
}

DEFUN_INT("start-of-buffer", Fstart_of_buffer, Sstart_of_buffer,
	  (repv tx, repv irp), rep_Subr2, "!@") /*
::doc:start-of-buffer::
start-of-buffer [BUFFER] [IGNORE-RESTRICTION-P]

Return the position of the start of the buffer. Unless
IGNORE-RESTRICTION-P is non-nil the position returned is the start
of the buffer's restriction.
::end:: */
{
    if(!rep_NILP(irp))
	return make_pos(0, 0);
    else
	return Frestriction_start(tx);
}

DEFUN_INT("end-of-line", Fend_of_line, Send_of_line,
	  (repv pos, repv tx), rep_Subr2, "@") /*
::doc:end-of-line::
end-of-line [POS] [BUFFER]

Return the position of the last character in the line pointed to by POS (or
the cursor).
::end:: */
{
    if(!BUFFERP(tx))
	tx = rep_VAL(curr_vw->tx);
    if(!POSP(pos))
	pos = get_buffer_cursor(VBUFFER(tx));
    if(VROW(pos) < VBUFFER(tx)->line_count)
	return make_pos(VBUFFER(tx)->lines[VROW(pos)].ln_Strlen - 1, VROW(pos));
    else
	return Qnil;
}

DEFUN_INT("start-of-line", Fstart_of_line, Sstart_of_line,
	  (repv pos), rep_Subr1, "@") /*
::doc:start-of-line::
start-of-line [POS]

Return the position of the first character in the line pointed to by POS
(or the cursor).
::end:: */
{
    if(!POSP(pos))
	pos = curr_vw->cursor_pos;
    if(VCOL(pos) != 0)
	return make_pos(0, VROW(pos));
    else
	return pos;
}

DEFUN_INT("forward-line", Fforward_line, Sforward_line,
	  (repv lines, repv pos), rep_Subr2, "@p") /*
::doc:forward-line::
forward-line [NUMBER] [POS]

Return the position of the NUMBER'th (by default the next) line below
that pointed to by POS (or the cursor).

Negative NUMBERs move backwards, if the first line is passed (i.e. a negative
line number is made) nil is returned.
::end:: */
{
    intptr_t row;
    if(!POSP(pos))
	pos = curr_vw->cursor_pos;
    row = VROW(pos) + (rep_INTP(lines) ? rep_INT(lines) : 1);
    if(row < 0)
	return Qnil;
    else
	return make_pos(VCOL(pos), row);
}

DEFUN_INT("forward-char", Fforward_char, Sforward_char,
	  (repv count, repv pos, repv tx), rep_Subr3, "@p") /*
::doc:forward-char::
forward-char [COUNT] [POS] [BUFFER]

Returns the position of the character COUNT characters (by default the next)
after POS (or the cursor). Negative COUNTs move backwards. If either the
beginning or the end of the buffer is passed, nil is returned.
::end:: */
{
    intptr_t dist;
    Pos tem;
    if(!BUFFERP(tx))
	tx = rep_VAL(curr_vw->tx);
    if(!POSP(pos))
	pos = get_buffer_cursor(VBUFFER(tx));
    else
    {
	if(!check_pos(VBUFFER(tx), pos))
	    return 0;
    }
    dist = rep_INTP(count) ? rep_INT(count) : 1;
    if(dist == 0)
	return pos;
    COPY_VPOS(&tem, pos);
    if((dist > 0 && forward_char(dist, VBUFFER(tx), &tem))
       || backward_char(-dist, VBUFFER(tx), &tem))
	return COPY_POS(&tem);
    else
	return Qnil;
}

DEFUN_INT("forward-tab", Fforward_tab, Sforward_tab,
	  (repv num, repv pos, repv size), rep_Subr3, "@p") /*
::doc:forward-tab::
forward-tab [COUNT] [POS] [TAB-SIZE]

Return the glyph position of the COUNT'th next tab stop to the right of
the character position POS (or the cursor). COUNT is assumed 1 when
undefined; negative values move towards the left hand side of the screen.
::end:: */
{
    int tabs = rep_INTP(num) ? rep_INT(num) : 1;
    Lisp_View *vw = curr_vw;
    int tabsize = rep_INTP(size) ? rep_INT(size) : vw->tx->tab_size;
    intptr_t col;
    if(!POSP(pos))
    {
	pos = curr_vw->cursor_pos;
	col = get_cursor_column(vw);
    }
    else
	col = VCOL(pos);
    while(tabs > 0)
    {
	col = ((col / tabsize) + 1) * tabsize;
	tabs--;
    }
    while(tabs < 0)
    {
	col = (((col - 1) / tabsize)) * tabsize;
	tabs++;
    }
    if(col >= 0)
	return make_pos(col, VROW(pos));
    else
	return(Qnil);
}

DEFSTRING(no_brac, "No matching bracket");
DEFSTRING(no_open_brac, "No opening bracket");

static int
find_matching_bracket(Pos *pos, Lisp_Buffer *tx, char esc)
{
#define NUM_BRAC_TYPES 10
    static char bracs[] =
    {
	'{', '}',
	'(', ')',
	'[', ']',
	'`', '\'',
	'<', '>'
    };

/* Test for an escape character preceding COL in the string LINE. Beware
   that COL is referenced more than once, so no side effects please!   */
#define TST_ESC(line, col) ((col) > 0 && (line)[(col)-1] == esc)

    LINE *line = tx->lines + PROW(pos);	/* safe */
    if(PCOL(pos) < line->ln_Strlen)
    {
	char startc = line->ln_Line[PCOL(pos)];
	intptr_t i;
	for(i = 0; i < NUM_BRAC_TYPES; i++)
	{
	    if(startc == bracs[i])
		break;
	}
	if(!TST_ESC(line->ln_Line, PCOL(pos)) && (i < NUM_BRAC_TYPES))
	{
	    intptr_t x = PCOL(pos);
	    intptr_t y = PROW(pos);
	    int braccount = 1;
	    bool found = false;
	    if(i & 1)
	    {
		/* search backwards */
		char endc = bracs[i - 1];
		while(!found)
		{
		    char c;
		    if(--x < 0)
		    {
			if(--y < tx->logical_start)
			{
			    Fsignal(Qerror, rep_LIST_1(rep_VAL(&no_brac)));
			    return(false);
			}
			line--;
			x = line->ln_Strlen - 1;
		    }
		    c = line->ln_Line[x];
		    if(c == startc)
		    {
			if(!TST_ESC(line->ln_Line, x))
			    braccount++;
		    }
		    else if(c == endc)
		    {
			if(!TST_ESC(line->ln_Line, x) && !(--braccount))
			    found = true;
		    }
		}
	    }
	    else
	    {
		/* search forwards */
		char endc = bracs[i + 1];
		while(!found)
		{
		    char c;
		    if(++x >= line->ln_Strlen)
		    {
			if(++y >= tx->logical_end)
			{
			    Fsignal(Qerror, rep_LIST_1(rep_VAL(&no_brac)));
			    return(false);
			}
			line++;
			x = 0;
		    }
		    c = line->ln_Line[x];
		    if(c == startc)
		    {
			if(!TST_ESC(line->ln_Line, x))
			    braccount++;
		    }
		    else if(c == endc)
		    {
			if(!TST_ESC(line->ln_Line, x) && !(--braccount))
			    found = true;
		    }
		}
	    }
	    PCOL(pos) = x;
	    PROW(pos) = y;
	    return true;
	}
    }
    Fsignal(Qerror, rep_LIST_1(rep_VAL(&no_open_brac)));
    return false;
}

DEFUN_INT("find-matching-bracket", Ffind_matching_bracket,
	  Sfind_matching_bracket, (repv pos, repv tx, repv esc),
	  rep_Subr3, "!@") /*
::doc:find-matching-bracket::
find-matching-bracket [POS] [BUFFER] [ESCAPE-CHAR]

Find a bracket matching the one at POS (or the cursor). The things that match
each other are,  { }, ( ), [ ], ` ', < >. POS is altered.
Brackets preceded by ESCAPE-CHAR (`\' by default) are not counted.
::end:: */
{
    char esc_char = rep_CHAR_8BIT_P(esc) ? rep_CHAR_VALUE(esc) : '\\';
    Pos tem;
    if(!BUFFERP(tx))
	tx = rep_VAL(curr_vw->tx);
    if(!POSP(pos))
	pos = get_buffer_cursor(VBUFFER(tx));
    else
    {
	if(!check_pos(VBUFFER(tx), pos))
	    return 0;
    }
    COPY_VPOS(&tem, pos);
    if(find_matching_bracket(&tem, VBUFFER(tx), esc_char))
	return COPY_POS(&tem);
    return(Qnil);
}

DEFUN("raw-mouse-pos", Fraw_mouse_pos, Sraw_mouse_pos, (void), rep_Subr0) /*
::doc:raw-mouse-pos::
raw-mouse-pos

Return the glyph position of the mouse, relative to the current window.
::end:: */
{
    repv pos = sys_get_mouse_pos(curr_win);
    if(pos != 0)
	return pos;
    else
	return Qnil;
}

void
movement_init(void)
{
    rep_ADD_SUBR(Sgoto);
    rep_ADD_SUBR(Sgoto_glyph);
    rep_ADD_SUBR_INT(Scenter_display);
    rep_ADD_SUBR_INT(Snext_screen);
    rep_ADD_SUBR_INT(Sprev_screen);
    rep_ADD_SUBR_INT(Send_of_buffer);
    rep_ADD_SUBR_INT(Sstart_of_buffer);
    rep_ADD_SUBR_INT(Send_of_line);
    rep_ADD_SUBR_INT(Sstart_of_line);
    rep_ADD_SUBR_INT(Sforward_line);
    rep_ADD_SUBR_INT(Sforward_char);
    rep_ADD_SUBR_INT(Sforward_tab);
    rep_ADD_SUBR_INT(Sfind_matching_bracket);
    rep_ADD_SUBR(Sraw_mouse_pos);

    rep_INTERN_SPECIAL(next_screen_context_lines);
    Fset (Qnext_screen_context_lines, rep_MAKE_INT(2));
}
