/* redisplay.c -- Experimental redisplay algorithm
   Copyright (C) 1998 John Harper <john@dcs.warwick.ac.uk>
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
#include "jade_protos.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#ifndef HAVE_ALLOCA
/* TODO: fix the need for alloca() */
# error You need alloca()!
#else
# include <alloca.h>
#endif

/* The upper bound on edit operations per window. Zero denotes unbounded. */
static int redisplay_max_d = 0;

#undef DEBUG

_PR glyph_buf *alloc_glyph_buf(int cols, int rows);
_PR void free_glyph_buf(glyph_buf *gb);
_PR void garbage_glyphs(WIN *w, int x, int y, int width, int height);
_PR void redisplay_init(void);


/* Glyph buffer basics */

/* Allocate a new glyph buffer and initialise it. */
glyph_buf *
alloc_glyph_buf(int cols, int rows)
{
    glyph_buf *g = malloc(SIZEOF_GLYPH_BUF(cols, rows));
    if(g == 0)
	abort();
    g->cols = cols;
    g->rows = rows;
    return g;
}

/* Return a glyph buffer to the heap. */
void
free_glyph_buf(glyph_buf *gb)
{
    free(gb);
}

/* Compute and return the hash code of line ROW in buffer G. */
static INLINE u_long
hash_glyph_row(glyph_buf *g, int row)
{
    u_long value = 0;
    int togo = g->cols;
    glyph_code *codes = GLYPH_BUF_CODES(g, row);
    glyph_attr *attrs = GLYPH_BUF_ATTRS(g, row);
    while(togo-- > 0)
    {
	/* Is this satisfactory? */
	value = (value * 33) + *codes++;
	value = (value * 33) + *attrs++;
    }
    return(value);
}

/* Hash every row in glyph buffer G. */
static INLINE void
hash_glyph_buf(glyph_buf *g)
{
    int i;
    for(i = 0; i < g->rows; i++)
	g->hashes[i] = hash_glyph_row(g, i);
}

/* Record that the WIDTH,HEIGHT rectangle of glyphs at X,Y in window W's
   glyph buffer has been mangled, and therefore will need to be redrawn. */
void
garbage_glyphs(WIN *w, int x, int y, int width, int height)
{
    glyph_buf *g = w->w_Content;
    if(x + width > g->cols)
	width = g->cols - x;
    if(y + height > g->rows)
	height = g->rows - y;
    while(height > 0)
    {
	memset(GLYPH_BUF_ATTRS(g, y) + x * sizeof(glyph_attr),
	       GA_Garbage, width);
	g->hashes[y] = hash_glyph_row(g, y);
	y++; height--;
    }
}

/* Returns true iff line L1 of glyph buffer G1 exactly matches line L2 of
   buffer G2 (matching both codes and attributes). The hash arrays should
   have been filled in. In this function L1 and L2 count from zero.. */
static INLINE bool
compare_lines(glyph_buf *g1, glyph_buf *g2, int line1, int line2)
{
    return (g1->hashes[line1] == g2->hashes[line2]
	    && memcmp(GLYPH_BUF_CODES(g1, line1),
		      GLYPH_BUF_CODES(g2, line2),
		      g1->cols * sizeof(glyph_code)) == 0
	    && memcmp(GLYPH_BUF_ATTRS(g1, line1),
		      GLYPH_BUF_ATTRS(g2, line2),
		      g1->cols * sizeof(glyph_attr)) == 0);
}


/* Screen primitives */

/* Copy N-LINES from SRC-LINE to DST-LINE (note that SRC-LINE and DST-LINE
   both count from one). Updates OLD-G to reflect this. In this function
   SRC-LINE and DST-LINE count from one.. */
static void
redisplay_do_copy(WIN *w, glyph_buf *old_g, glyph_buf *new_g,
		  int src_line, int dst_line, int n_lines)
{
    assert(src_line > 0 && dst_line > 0);
    COPY_GLYPHS(w, 0, src_line - 1, w->w_MaxX, n_lines, 0, dst_line - 1);
    memmove(GLYPH_BUF_CODES(old_g, dst_line-1),
	    GLYPH_BUF_CODES(old_g, src_line-1),
	    sizeof(glyph_code) * n_lines * old_g->cols);
    memmove(GLYPH_BUF_ATTRS(old_g, dst_line-1),
	    GLYPH_BUF_ATTRS(old_g, src_line-1),
	    sizeof(glyph_attr) * n_lines * old_g->cols);
    memmove(old_g->hashes + (dst_line-1),
	    old_g->hashes + (src_line-1),
	    sizeof(u_long) * n_lines);
}

/* Draw a single line of glyphs: LINE in NEW-G, given that the
   current contents of this line are contained in OLD-G at LINE.
   In this function LINE counts from one.. */
static void
redisplay_do_draw(WIN *w, glyph_buf *old_g, glyph_buf *new_g, int line)
{
    /* Draw LINE from NEW-G. OLD-G[LINE] _will_ reflect the currently
       displayed contents of LINE. */
    glyph_code *old_codes = GLYPH_BUF_CODES(old_g, line-1);
    glyph_code *new_codes = GLYPH_BUF_CODES(new_g, line-1);
    glyph_code *old_attrs = GLYPH_BUF_ATTRS(old_g, line-1);
    glyph_code *new_attrs = GLYPH_BUF_ATTRS(new_g, line-1);

    int prefix, suffix;

    assert(line > 0);

    /* First try to find a common prefix to the two lines */
    for(prefix = 0; prefix < old_g->cols; prefix++)
	if(old_codes[prefix] != new_codes[prefix]
	   || old_attrs[prefix] != new_attrs[prefix])
	    break;

    /* Then the suffix, unless all of the lines matched already */
    if(prefix == old_g->cols)
	return;
    suffix = old_g->cols - 1;
    while(1)			/* it's impossible to pass column zero */
    {
	if(old_codes[suffix] != new_codes[suffix]
	   || old_attrs[suffix] != new_attrs[suffix])
	{
	    suffix++;
	    break;
	}
	suffix--;
    }

    /* So we have PREFIX glyphs in common at the start of the line, and
       SUFFIX in common at the end. Just draw the bit inbetween,
       taking care to detect attribute changes. */
    while(prefix < suffix)
    {
	glyph_attr attr = new_attrs[prefix];
	int end;

	for(end = prefix; end < suffix; end++)
	    if(new_attrs[end] != attr)
		break;

	DRAW_GLYPHS(w, prefix, line-1, attr, new_codes + prefix, end - prefix);
	prefix = end;
    }
}


/* Glyph buffer diffing

   This uses the algorithm described in Chapter 3 of:

	@Book{Miller:1987:STS,
	  author =       "Webb Miller",
	  title =        "A Software Tools Sampler",
	  year =         "1987",
	  publisher =    "Prentice-Hall, Inc.",
	  address =      "Englewood Cliffs, New Jersey",
	}

   and the paper:

	@Article{Miller:1985:FCP,
	  author =       "Webb Miller and Eugene W. Myers",
	  title =        "A File Comparison Program",
	  journal =      "Software---Practice and Experience",
	  volume =       "15",
	  number =       "11",
	  year =         "1985",
	  month =        nov,
	  pages =        "1025--1040",
	}

   Apparently GNU diff(1) also uses it. Anyway, the procedure for
   displaying a glyph buffer given the current contents of the display
   (in a second glyph buffer) is as follows:

	1. Use the fcomp algorithm from the references above to
	   compute a minimal edit script to change the old buffer
	   into the new buffer. The script consists of a sequence
	   of INSERT line or DELETE line operations.

	2. Use the edit script to produce an array of ``links'' from
	   the new buffer to the old buffer. Each line has a single
	   link, either the row in the old buffer that should be
	   displayed at this line, or -1 meaning it has to be
	   drawn from scratch.

	3. Use the link array to update the display, this is done
	   in two passes, copies (i.e. scrolling) followed by
	   drawing. The copy pass updates the link array when
	   it munges displayed lines.

   I can only see one obvious shortcoming (apart from the overall
   complexity), step (3) may not find the optimal COPY sequence. It
   works strictly from the top of the link array to the bottom,
   possibly preventing copies lower down.. */

/* The two types of edit operation. */
enum Edit_Op {
    Edit_Insert = 0,
    Edit_Delete
};

/* One operation in the edit script. */
struct edit_script {
    struct edit_script *link;		/* previous edit instruction */
    enum Edit_Op op;			/* insertion or deletion? */
    u_long line1;			/* line number in buffer 1 */
    u_long line2;			/* line number in buffer 2 */
};

#ifdef DEBUG
/* Print the unprocessed edit script. */
static void
dump_script(struct edit_script *start)
{
    fprintf(stderr, "script (reversed) = {\n");
    while(start != 0)
    {
	fprintf(stderr, "    { %s, L1=%ld L2=%ld }\n",
		start->op == Edit_Insert ? "Insert" : "Delete",
		start->line1, start->line2);
	start = start->link;
    }
    fprintf(stderr, "}\n");
}
#endif

/* Use the constructed minimal edit script to patch W from the current
   contents as depicted by OLD-G to the desired contents as shown by NEW-G.
   POINT should be the (currently reversed) edit script. */
static void
execute_script(WIN *w, glyph_buf *old_g, glyph_buf *new_g,
	       struct edit_script *point)
{
    struct edit_script *lookahead, *behind;
    int *links;				/* zeroth element unused */
    int current1 = 1, last2 = 1, i;

    /* First of all, reverse the script */
    lookahead = point;
    point = 0;
    while(lookahead != 0)
    {
	behind = point;
	point = lookahead;
	lookahead = lookahead->link;
	point->link = behind;		/* flip the pointer */
    }

    links = alloca(sizeof(int) * (old_g->rows + 1));

    /* Make the links. LINKS[K] is the position of the line currently
       displayed (i.e. in A) that should be displayed at line K after
       refresh is complete. */
    while(point != 0)
    {
	if(point->op == Edit_Insert)
	{
	    while(last2 < point->line2)
		links[last2++] = current1++;
	    links[last2++] = -1;
	}
	else if(point->op == Edit_Delete)
	{
	    while(current1 < point->line1)
		links[last2++] = current1++;
	    current1++;
	}
	point = point->link;
    }
    while(last2 <= new_g->rows)
	links[last2++] = current1++;

#ifdef DEBUG
    printf("Drawing: ");
    fflush(stdout);
#endif

    /* Use the links
       Pass1: do all copying */
    i = 1;
    while(i <= new_g->rows)
    {
	int begin, delta, src, dst, n;

	/* Skip lines that must be drawn in pass 2 */
	while(i <= new_g->rows && links[i] == -1)
	    i++;

	begin = i;
	delta = links[i] - i;

	/* Find the length of the current section of lines that
	   can be copied in one chunk (i.e. because the distance
           between the line and the line to be copied to it is the
	   same across all the lines in the section.) */
	while(i <= new_g->rows && (links[i] - i) == delta && links[i] != -1)
	    i++;

	/* Start of a new section. We need to display lines
	   LINKS[BEGIN]->LINKS[i-1] of the old display, at line
	   BEGIN of the new. */

	src = links[begin];
	dst = begin;
	n = i - begin;

	/* Skip a null copy */
	if(src == dst || n == 0)
	    continue;

	redisplay_do_copy(w, old_g, new_g, src, dst, n);
#ifdef DEBUG
	printf(" COPY(%d: %d->%d) ", n, src, dst);
	fflush(stdout);
#endif

	/* In all lines yet to be updated, make sure that the
	   link to the line to be drawn there still points to
	   the same line. And if that line has been overwritten
	   flag that it must be redrawn from scratch. */
	if(src < dst)
	{
	    /* We're scrolling down the page */
	    int j;
	    for(j = i; j <= new_g->rows; j++)
	    {
		int link_j = links[j];
		if(link_j >= dst && link_j < src + n)
		{
		    /* The line to be drawn at J was one of the
		       lines we just moved. Update the link */
		    links[j] += (dst - src);
#ifdef DEBUG
		    printf(" RELOC(%d: %d->%d) ", j, link_j, links[j]);
		    fflush(stdout);
#endif
		}
		else if(link_j >= src + n && link_j < dst + n)
		{
		    /* We just overwrote the line to be displayed
		    at line J :-( */
		    links[j] = -1;
#ifdef DEBUG
		    printf(" TRASH(%d) ", j);
		    fflush(stdout);
#endif
		}
	    }
	}
	else
	{
	    /* We're scrolling up the page */
	    int j;
	    for(j = i; j <= new_g->rows; j++)
	    {
		int link_j = links[j];
		if(link_j >= dst && link_j < src)
		{
		    /* Overwritten */
		    links[j] = -1;
#ifdef DEBUG
		    printf(" TRASH(%d) ", j);
		    fflush(stdout);
#endif
		}
		else if(link_j >= src && link_j <= dst + n)
		{
		    /* Moved */
		    links[j] -= (src - dst);
#ifdef DEBUG
		    printf(" RELOC(%d: %d->%d) ", j, link_j, links[j]);
		    fflush(stdout);
#endif
		}
	    }
	}
    }

    /* Pass2: do all drawing */
    for(i = 1; i <= new_g->rows; i++)
    {
	if(links[i] == -1)
	{
	    redisplay_do_draw(w, old_g, new_g, i);
#ifdef DEBUG
	    printf(" DRAW(%d) ", i);
	    fflush(stdout);
#endif
	}
    }
#ifdef DEBUG
    printf("\n");
#endif
}


/* MAX-D is bound on size of edit script (zero means unbounded). Returns
   TRUE if the comparison went ok and everything's been redisplayed.
   Returns FALSE when more than MAX-D edit instructions were needed. */
static bool
patch_display(WIN *w, glyph_buf *old_g, glyph_buf *new_g)
{
    int col;				/* column number */
    int d;				/* current edit distance */
    int lower;				/* left-most diag under consid. */
    int k;				/* current diagonal */
    int row;				/* row number */
    int upper;				/* right-most diagonal under
					   consideration */
    int max_d;

#define ORIGIN (old_g->rows)		/* for accessing the arrays */
    
    /* for each diagonal, two items are saved: */
    int *last_d;			/* row containing the last d */
    struct edit_script **script;	/* corresponding edit script */

    struct edit_script *new;

    assert(old_g->rows == new_g->rows && old_g->cols == new_g->cols);

    last_d = alloca(sizeof(int) * 2 * old_g->rows);
    script = alloca(sizeof(struct edit_script *) * 2 * old_g->rows);

    if(redisplay_max_d == 0)
	max_d = 2 * old_g->rows;	/* no limit */
    else
	max_d = redisplay_max_d;

    /* initialise: 0 entries in D indicate identical prefixes. */
    for(row = 0;
	row < old_g->rows && row < new_g->rows
	&& compare_lines(old_g, new_g, row, row);
	++row) ;

    last_d[ORIGIN] = row;
    script[ORIGIN] = 0;
    lower = (row == old_g->rows) ? ORIGIN + 1 : ORIGIN - 1;
    upper = (row == new_g->rows) ? ORIGIN - 1 : ORIGIN + 1;
    if(lower > upper)
	return TRUE;			/* buffers are identical */

    /* for each value of the edit distance... */
    for(d = 1; d <= max_d; ++d)
    {
	/* for each relevant diagonal... */
	for(k = lower; k <= upper; k += 2)
	{
	    /* get space for the next edit instruction */
	    new = (struct edit_script *) alloca(sizeof(struct edit_script));

	    /* find a d on diagonal k */
	    if(k == ORIGIN - d || k != ORIGIN + d
	       && last_d[k+1] >= last_d[k-1])
	    {
		/* Moving down from the last d-1 on diagonal k+1
		   puts you farther along diagonal k than does
		   moving right from the last d-1 on diagonal k-1. */
		row = last_d[k+1] + 1;
		new->link = script[k+1];
		new->op = Edit_Delete;
	    }
	    else
	    {
		/* Move right from the last d-1 on diagonal k-1 */
		row = last_d[k-1];
		new->link = script[k-1];
		new->op = Edit_Insert;
	    }
	    /* Code common to the two cases */
	    new->line1 = row;
	    new->line2 = col = row + k - ORIGIN;
	    script[k] = new;

	    /* Slide down the diagonal */
	    while(row < old_g->rows
		  && col < new_g->rows
		  && compare_lines(old_g, new_g, row, col))
	    {
		++row;
		++col;
	    }
	    last_d[k] = row;

	    if(row == old_g->rows && col == new_g->rows)
	    {
		/* Hit southeast corner; have the answer */
#ifdef DEBUG
		dump_script(script[k]);
#endif
		execute_script(w, old_g, new_g, script[k]);
		return TRUE;
	    }

	    if(row == old_g->rows)
		/* Hit last row; don't look to the left */
		lower = k+2;

	    if(col == new_g->rows)
		/* Hit last column; don't look to the right */
		upper = k-2;
	}
	--lower;
	++upper;
    }

    /* Too many edit operations, give up. */
    return FALSE;

#undef ORIGIN
}


/* Putting it all together */

_PR VALUE cmd_redisplay(VALUE arg);
DEFUN_INT("redisplay", cmd_redisplay, subr_redisplay, (VALUE arg), V_Subr1, DOC_redisplay, "P") /*
::doc:redisplay::
redisplay [FORCE]

Redisplay everything that needs to be. When FORCE (the raw prefix arg) is
non-nil, absolutely everything is refreshed, not just what changed.
::end:: */
{
    WIN *w;
    VW *vw;

#ifdef DEBUG
    fprintf(stderr, "Entering redisplay..\n");
#endif

    for(vw = view_chain; vw != 0; vw = vw->vw_Next)
	if(vw->vw_Win != 0)
	    resync_xy(vw);

    for(w = win_chain; w != 0; w = w->w_Next)
    {
	if(w->w_Window != 0)
	{
	    glyph_buf *tem;

	    if(!NILP(arg) || (w->w_Flags & WINFF_FORCE_REFRESH))
	    {
		/* Must redraw this window. The easiest way to do this
		   is to just garbage the entire contents */
		garbage_glyphs(w, 0, 0, w->w_MaxX, w->w_MaxY);
		w->w_Flags &= ~WINFF_FORCE_REFRESH;
	    }

	    make_window_glyphs(w->w_NewContent, w);
	    hash_glyph_buf(w->w_NewContent);
	    if(!patch_display(w, w->w_Content, w->w_NewContent))
	    {
		/* MAX-D was exceeded. Draw all lines manually. */
		int row;
		for(row = 1; row <= w->w_MaxY; row++)
		    redisplay_do_draw(w, w->w_Content, w->w_NewContent, row);
	    }

	    /* Flip the old and new glyph buffers. */
	    tem = w->w_NewContent;
	    w->w_NewContent = w->w_Content;
	    w->w_Content = tem;
	}
    }
#ifdef DEBUG
    fprintf(stderr, "Leaving redisplay.\n");
#endif

    cmd_flush_output();
    return sym_t;
}

_PR VALUE var_redisplay_max_d(VALUE val);
DEFUN("redisplay-max-d", var_redisplay_max_d, subr_redisplay_max_d, (VALUE val), V_Var, DOC_redisplay_max_d) /*
::doc:redisplay_max_d::
The upper bound on the number of edit operations (insert line, or delete
line) per window redisplay. Zero means unbounded (i.e. 2*ROWS).

When the limit is exceeded the search for an optimal edit script is
aborted and each row of the window is redisplayed manually.
::end:: */
{
    return handle_var_int(val, &redisplay_max_d);
}

void
redisplay_init(void)
{
    ADD_SUBR_INT(subr_redisplay);
    ADD_SUBR(subr_redisplay_max_d);
}