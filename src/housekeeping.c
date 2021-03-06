/* housekeeping.c -- Generally editor fiddly stuff
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
   along with Jade; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "jade.h"
#include <assert.h>


/* The next few routines deal with updating the various references to
   coordinates throughout the views after chunks have been deleted and
   inserted.  */

void
adjust_marks_add_x(Lisp_Buffer *tx, intptr_t addx,
		   intptr_t xpos, intptr_t ypos)
{
    Lisp_View *thisvw;
    Lisp_Mark *thismark;

#define UPD(p)						\
    do {						\
	if(p && VROW(p) == ypos && VCOL(p) >= xpos)	\
	    (p) = make_pos(VCOL(p) + addx, VROW(p));	\
    } while(0)

    for(thisvw = view_chain; thisvw; thisvw = thisvw->next)
    {
	if(thisvw->tx == tx)
	{
	    UPD(thisvw->cursor_pos);
            if(!(thisvw->car & VWFF_RECTBLOCKS))
	    {
		UPD(thisvw->block_start);
		UPD(thisvw->block_end);
	    }
	}
    }

    for(thismark = tx->mark_chain; thismark; thismark = thismark->next)
	UPD(thismark->pos);

    UPD(tx->saved_cursor_pos);
    UPD(tx->saved_display_origin);
    UPD(tx->saved_block[0]);
    UPD(tx->saved_block[1]);

    adjust_extents_add_cols(tx->global_extent, addx, xpos, ypos);

    {
	struct cached_extent *ce = tx->extent_cache;
	int i;
	for(i = 0; i < EXTENT_CACHE_SIZE; i++, ce++)
	{
	    if(ce->extent && ce->pos.row == ypos && ce->pos.col >= xpos)
		ce->pos.col += addx;
	}
    }

#undef UPD
}

void
adjust_marks_sub_x(Lisp_Buffer *tx, intptr_t subx,
		   intptr_t xpos, intptr_t ypos)
{
    Lisp_View *thisvw;
    Lisp_Mark *thismark;

#define UPD(p)						\
    do {						\
	if(p && VROW(p) == ypos && VCOL(p) >= xpos)	\
	{						\
	    intptr_t col = VCOL(p) - subx;		\
	    (p) = make_pos(MAX(col, xpos), VROW(p));	\
	}						\
    } while(0)

    for(thisvw = view_chain; thisvw; thisvw = thisvw->next)
    {
	if(thisvw->tx == tx)
	{
	    UPD(thisvw->cursor_pos);
            if(!(thisvw->car & VWFF_RECTBLOCKS))
	    {
		UPD(thisvw->block_start);
		UPD(thisvw->block_end);
	    }
	}
    }

    for(thismark = tx->mark_chain; thismark; thismark = thismark->next)
	UPD(thismark->pos);

    UPD(tx->saved_cursor_pos);
    UPD(tx->saved_display_origin);
    UPD(tx->saved_block[0]);
    UPD(tx->saved_block[1]);

    adjust_extents_sub_cols(tx->global_extent, subx, xpos, ypos);

    {
	struct cached_extent *ce = tx->extent_cache;
	int i;
	for(i = 0; i < EXTENT_CACHE_SIZE; i++, ce++)
	{
	    if(ce->extent && ce->pos.row == ypos && ce->pos.col >= xpos)
		ce->pos.col -= subx;
	}
    }

#undef UPD
}

/*
 * Whole lines only please
 */
void
adjust_marks_add_y(Lisp_Buffer *tx, intptr_t addy, intptr_t ypos)
{
    Lisp_View *thisvw;
    Lisp_Mark *thismark;

#define UPD(p)						\
    do {						\
	if(p && VROW(p) >= ypos)			\
	    (p) = make_pos(VCOL(p), VROW(p) + addy);	\
    } while(0)

#define UPD_Y(y)				\
    do {					\
	if(y >= ypos)				\
	    y += addy;				\
    } while(0)

    for(thisvw = view_chain; thisvw; thisvw = thisvw->next)
    {
	if(thisvw->tx == tx)
	{
	    UPD(thisvw->cursor_pos);
	    UPD(thisvw->block_start);
	    UPD(thisvw->block_end);
	    if(thisvw != curr_vw)
		UPD(thisvw->display_origin);
	}
    }
    for(thismark = tx->mark_chain; thismark; thismark = thismark->next)
	UPD(thismark->pos);


    if(tx->logical_start > ypos)
	tx->logical_start += addy;
    UPD_Y(tx->logical_end);

    UPD(tx->saved_cursor_pos);
    UPD(tx->saved_display_origin);
    UPD(tx->saved_block[0]);
    UPD(tx->saved_block[1]);

    adjust_extents_add_rows(tx->global_extent, addy, ypos);

    {
	struct cached_extent *ce = tx->extent_cache;
	int i;
	for(i = 0; i < EXTENT_CACHE_SIZE; i++, ce++)
	{
	    if(ce->extent && ce->pos.row >= ypos)
		ce->pos.row += addy;
	}
    }

#undef UPD
#undef UPD_Y
}

/*
 * Whole lines only please
 */
void
adjust_marks_sub_y(Lisp_Buffer *tx, intptr_t suby, intptr_t ypos)
{
    Lisp_View *thisvw;
    Lisp_Mark *thismark;

#define UPD_Y(y)				\
    do {					\
	if(y > ypos)				\
	{					\
	    if((y -= suby) < ypos)		\
		y = ypos;			\
	}					\
    } while(0)

#define UPD(p)					\
    do {					\
	if(p && VROW(p) >= ypos)		\
	{					\
	    intptr_t row = VROW(p) - suby;	\
	    if(row < ypos)			\
		(p) = make_pos(0, ypos);	\
	    else				\
		(p) = make_pos(VCOL(p), row);	\
	}					\
    } while(0)

#define UPD1(p)						\
    do {						\
	if(p && VROW(p) >= ypos)			\
	{						\
	    intptr_t row = VROW(p) - suby;		\
	    (p) = make_pos(VCOL(p), MAX(row, ypos));	\
	}						\
    } while(0)

    for(thisvw = view_chain; thisvw; thisvw = thisvw->next)
    {
	if(thisvw->tx == tx)
	{
	    UPD(thisvw->cursor_pos);
            if(thisvw->car & VWFF_RECTBLOCKS)
	    {
		UPD1(thisvw->block_start);
		UPD1(thisvw->block_end);
	    }
            else
	    {
                UPD(thisvw->block_start);
                UPD(thisvw->block_end);
	    }
	    if(thisvw != curr_vw)
		UPD1(thisvw->display_origin);
	}
    }
    for(thismark = tx->mark_chain; thismark; thismark = thismark->next)
	UPD(thismark->pos);

    UPD_Y(tx->logical_start);
    UPD_Y(tx->logical_end);

    UPD(tx->saved_cursor_pos);
    UPD(tx->saved_display_origin);
    UPD(tx->saved_block[0]);
    UPD(tx->saved_block[1]);

    adjust_extents_sub_rows(tx->global_extent, suby, ypos);

    {
	struct cached_extent *ce = tx->extent_cache;
	int i;
	for(i = 0; i < EXTENT_CACHE_SIZE; i++, ce++)
	{
	    if(ce->extent && ce->pos.row >= ypos)
		ce->pos.row = MAX(ypos, ce->pos.row - suby);
	}
    }

#undef UPD_Y
#undef UPD
#undef UPD1
}

/*
 * Use when splitting a line into 2, cursor should be at position of split
 */
void
adjust_marks_split_y(Lisp_Buffer *tx, intptr_t xpos, intptr_t ypos)
{
    Lisp_View *thisvw;
    Lisp_Mark *thismark;

#define UPD_Y(y)				\
    do {					\
	if(y > ypos)				\
	    y++;				\
    } while(0)

#define UPD(p)							\
    do {							\
	if(p && (VROW(p) > ypos					\
		 || (VROW(p) == ypos && VCOL(p) >= xpos)))	\
	{							\
	    (p) = make_pos((VROW(p) == ypos && VCOL(p) >= xpos)	\
			   ? VCOL(p) - xpos : VCOL(p),		\
			   VROW(p) + 1);			\
	}							\
    } while(0)

#define UPD1(p)						\
    do {						\
	if(p && VROW(p) > ypos)				\
	    (p) = make_pos(VCOL(p), VROW(p) + 1);	\
    } while(0)

    for(thisvw = view_chain; thisvw; thisvw = thisvw->next)
    {
	if(thisvw->tx == tx)
	{
	    UPD(thisvw->cursor_pos);
            if(thisvw->car & VWFF_RECTBLOCKS)
	    {
		UPD1(thisvw->block_start);
		UPD1(thisvw->block_end);
	    }
            else
	    {
                UPD(thisvw->block_start);
                UPD(thisvw->block_end);
	    }
	    if(thisvw != curr_vw)
		UPD1(thisvw->display_origin);
	}
    }
    for(thismark = tx->mark_chain; thismark; thismark = thismark->next)
	UPD(thismark->pos);

    UPD_Y(tx->logical_start);
    UPD_Y(tx->logical_end);

    UPD(tx->saved_cursor_pos);
    UPD(tx->saved_display_origin);
    UPD(tx->saved_block[0]);
    UPD(tx->saved_block[1]);

    adjust_extents_split_row(tx->global_extent, xpos, ypos);

    {
	struct cached_extent *ce = tx->extent_cache;
	int i;
	for(i = 0; i < EXTENT_CACHE_SIZE; i++, ce++)
	{
	    if(ce->extent
	       && (ce->pos.row > ypos
		   || (ce->pos.row == ypos && ce->pos.col >= xpos)))
	    {
		if(ce->pos.row == ypos)
		    ce->pos.col -= xpos;
		ce->pos.row++;
	    }
	}
    }

#undef UPD_Y
#undef UPD
#undef UPD1
}

/*
 * Use when compacting 2 adjacent lines into one
 */
void
adjust_marks_join_y(Lisp_Buffer *tx, intptr_t xpos, intptr_t ypos)
{
    Lisp_View *thisvw;
    Lisp_Mark *thismark;

#define UPD_Y(y)				\
    do {					\
	if(y > ypos)				\
	    y--;				\
    } while(0)

#define UPD(p)								\
    do {								\
	if(p && VROW(p) > ypos)						\
	    (p) = make_pos(VCOL(p) + ((VROW(p) == ypos + 1) ? xpos : 0),\
			   VROW(p) - 1);				\
    } while(0)

#define UPD1(p)						\
    do {						\
	if(p && VROW(p) > ypos)				\
	    (p) = make_pos(VCOL(p), VROW(p) - 1);	\
    } while(0)

    for(thisvw = view_chain; thisvw; thisvw = thisvw->next)
    {
	if(thisvw->tx == tx)
	{
	    UPD(thisvw->cursor_pos);
            if(thisvw->car & VWFF_RECTBLOCKS)
	    {
		UPD1(thisvw->block_start);
		UPD1(thisvw->block_end);
	    }
            else
	    {
                UPD(thisvw->block_start);
                UPD(thisvw->block_end);
	    }
	    if(thisvw != curr_vw)
		UPD1(thisvw->display_origin);
	}
    }
    for(thismark = tx->mark_chain; thismark; thismark = thismark->next)
	UPD(thismark->pos);

    UPD_Y(tx->logical_start);
    UPD_Y(tx->logical_end);

    UPD(tx->saved_cursor_pos);
    UPD(tx->saved_display_origin);
    UPD(tx->saved_block[0]);
    UPD(tx->saved_block[1]);

    adjust_extents_join_rows(tx->global_extent, xpos, ypos);

    {
	struct cached_extent *ce = tx->extent_cache;
	int i;
	for(i = 0; i < EXTENT_CACHE_SIZE; i++, ce++)
	{
	    if(ce->extent && ce->pos.row > ypos)
	    {
		if(ce->pos.row == ypos + 1)
		    ce->pos.col += xpos;
		ce->pos.row--;
	    }
	}
    }

#undef UPD
#undef UPD2
}


/* Miscellaneous */

/* This makes all views of this file have their cursor at the top of the
   file, it also refreshes each view. */
void
reset_all_views(Lisp_Buffer *tx)
{
    Lisp_View *thisvw;
    for(thisvw = view_chain; thisvw; thisvw = thisvw->next)
    {
	if(thisvw->tx == tx)
	{
	    thisvw->cursor_pos = Fstart_of_buffer(rep_VAL(tx), Qnil);
	    thisvw->display_origin = thisvw->cursor_pos;
	    thisvw->block_state = -1;
	}
	tx->saved_cursor_pos = make_pos(0, 0);
	tx->saved_display_origin = tx->saved_cursor_pos;
	tx->saved_block_state = -1;
    }
}
