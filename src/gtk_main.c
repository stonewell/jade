/* gtk_main.c -- Main code for GTK window-system
   Copyright (C) 1999 John Harper <john@dcs.warwick.ac.uk>
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
#include <gtk/gtkmain.h>
#include <string.h>
#include <assert.h>

/* For the resource database functions, and geometry parser */
#ifdef HAVE_X11
# include <X11/Xlib.h>
# include <gdk/gdkprivate.h>
#endif

#ifdef HAVE_UNIX
# include <fcntl.h>
#endif

/* Command line options, and their default values. */
static char *geom_str = "80x24";
static int opt_sync = 0;

static GdkColormap *color_map;

u_long gtk_meta_mod;

/* Default font name. */
DEFSTRING(def_font_str_data, DEFAULT_FONT);

DEFSYM (gtk, "gtk");

/* Dynamically-bound interface to rep-gtk.c */
repv (*gtk_jade_wrap_gtkobj)(GtkObject *object);
GtkObject *(*gtk_jade_get_gtkobj)(repv obj);
void (*gtk_jade_callback_postfix)(void);


/* Resource/option management */

#ifdef HAVE_X11
/* Scan the resource db for the entries we're interested in. */
static void
get_resources(char *prog_name)
{
    char *s;
    if((s = XGetDefault(gdk_display, prog_name, "geometry"))
       || (s = XGetDefault(gdk_display, "Jade", "Geometry")))
	geom_str = s;
    if((s = XGetDefault(gdk_display, prog_name, "foreground"))
       || (s = XGetDefault(gdk_display, "Jade", "Foreground")))
	default_fg_color = s;
    if((s = XGetDefault(gdk_display, prog_name, "background"))
       || (s = XGetDefault(gdk_display, "Jade", "Background")))
	default_bg_color = s;
    if((s = XGetDefault(gdk_display, prog_name, "block"))
       || (s = XGetDefault(gdk_display, "Jade", "Block")))
	default_block_color = s;
    if((s = XGetDefault(gdk_display, prog_name, "highlight"))
       || (s = XGetDefault(gdk_display, "Jade", "Highlight")))
	default_hl_color = s;
    if((s = XGetDefault(gdk_display, prog_name, "modeline"))
       || (s = XGetDefault(gdk_display, "Jade", "Modeline")))
	default_ml_color = s;
    if((s = XGetDefault(gdk_display, prog_name, "font"))
       || (s = XGetDefault(gdk_display, "Jade", "Font")))
	def_font_str = rep_string_dup(s);
}
#endif

/* Scan the command line for options. */
static void
get_options(void)
{
    repv opt;
    if (rep_get_option("--sync", 0))
	opt_sync = 1;
    if (rep_get_option("--geometry", &opt))
	geom_str = strdup (rep_STR(opt));
    if (rep_get_option("--fg", &opt))
	default_fg_color = strdup (rep_STR(opt));
    if (rep_get_option("--bg", &opt))
	default_bg_color = strdup (rep_STR(opt));
    if (rep_get_option("--bl", &opt))
	default_block_color = strdup (rep_STR(opt));
    if (rep_get_option("--hl", &opt))
	default_hl_color = strdup (rep_STR(opt));
    if (rep_get_option("--ml", &opt))
	default_ml_color = strdup (rep_STR(opt));
    if (rep_get_option("--font", &opt))
	def_font_str = opt;
}

/* After parsing the command line and the resource database, use the
   information. */
static bool
use_options(void)
{
#ifdef HAVE_X11
    int x, y, w, h;
    int gflgs = XParseGeometry(geom_str, &x, &y, &w, &h);

    if(gflgs & WidthValue)
	def_dims[2] = w;
    else
	def_dims[2] = -1;
    if(gflgs & HeightValue)
	def_dims[3] = h;
    else
	def_dims[3] = -1;
    /* TODO: need to use -ve values properly */
    if(gflgs & XValue)
	def_dims[0] = x;
    else
	def_dims[0] = -1;
    if(gflgs & YValue)
	def_dims[1] = y;
    else
	def_dims[1] = -1;
#endif

#ifdef HAVE_X11
    if (opt_sync)
	XSynchronize (gdk_display, True);
#endif

    return TRUE;
}

static void
make_argv (repv list, int *argc, char ***argv)
{
    int c = rep_INT (Flength (list)), i;
    char **v;

    v = (char **)rep_alloc ((c+1) * sizeof(char**));
    for (i = 0; i < c; i++, list = rep_CDR (list))
    {
	if (!rep_STRINGP (rep_CAR (list)))
	{
	    rep_free ((char *)v);
	    return;
	}
	v[i] = strdup (rep_STR (rep_CAR (list)));
    }
    v[c] = NULL;
  
    *argv = v;
    *argc = c;
}

/* Called from main(). */
bool
sys_init(char *program_name)
{
    int argc;
    char **argv;
    repv head, *last;

    make_argv (Fcons (rep_SYM(Qprogram_name)->value,
		      rep_SYM(Qcommand_line_args)->value), &argc, &argv);

    /* We need to initialise GTK now. The rep-gtk library will
       not reinitialise it.. */
    gtk_init (&argc, &argv);

    argc--; argv++;
    head = Qnil;
    last = &head;
    while(argc > 0)
    {
	*last = Fcons(rep_string_dup(*argv), Qnil);
	last = &rep_CDR(*last);
	argc--;
	argv++;
    }
    rep_SYM(Qcommand_line_args)->value = head;

    def_font_str = rep_VAL (&def_font_str_data);
#ifdef HAVE_X11
    get_resources (program_name);
#endif
    get_options ();
    use_options ();

    color_map = gdk_colormap_get_system ();
    gtk_meta_mod = gtk_find_meta ();

    /* Loading the gtk rep library will replace the usual
       event loop with one that works with GTK. */
    rep_INTERN(gtk);
    Frequire (Qgtk, Qnil);

    /* Find the gtkobj<->lispobj converters */
    gtk_jade_wrap_gtkobj = rep_find_dl_symbol (Qgtk, "sgtk_wrap_gtkobj");
    gtk_jade_get_gtkobj = rep_find_dl_symbol (Qgtk, "sgtk_get_gtkobj");
    gtk_jade_callback_postfix = rep_find_dl_symbol (Qgtk, "sgtk_callback_postfix");
    assert (gtk_jade_wrap_gtkobj != 0
	    && gtk_jade_get_gtkobj != 0
	    && gtk_jade_callback_postfix != 0);

    rep_beep_fun = gdk_beep;

    return TRUE;
}

void
sys_kill (void)
{
    gdk_colormap_unref (color_map);
    color_map = 0;
}

/* Print the options. */
void
sys_usage(void)
{
    fputs("    --geometry WINDOW-GEOMETRY\n"
	  "    --fg FOREGROUND-COLOUR\n"
	  "    --bg BACKGROUND-COLOUR\n"
	  "    --hl HIGHLIGHT-COLOUR\n"
	  "    --ml MODELINE-COLOUR\n"
	  "    --bl BLOCK-COLOUR\n"
	  "    --font FONT-NAME\n"
	  "    --sync\n"
	  "    FILE         Load FILE into an editor buffer\n", stderr);
}

repv
sys_get_mouse_pos(WIN *w)
{
    int x, y;
    /* XXX track mouse pointer position in gtk_jade.c.. */
    gtk_widget_get_pointer (GTK_WIDGET (w->w_Window), &x, &y);
    return make_pos((x - w->w_LeftPix) / w->w_FontX,
		    (y - w->w_TopPix) / w->w_FontY);
}


/* Color handling. */

DEFSTRING(no_parse_color, "Can't parse color");
DEFSTRING(no_alloc_color, "Can't allocate color");

repv
sys_make_color(Lisp_Color *c)
{
    if (gdk_color_parse (rep_STR (c->name), &c->color))
    {
	if (gdk_colormap_alloc_color (color_map, &c->color, 0, 1))
	    return rep_VAL (c);
	else
	    return Fsignal(Qerror, rep_list_2(rep_VAL(&no_alloc_color),
					      c->name));
    }
    else
	return Fsignal(Qerror, rep_list_2(rep_VAL(&no_parse_color), c->name));
}

void
sys_free_color(Lisp_Color *c)
{
    gdk_colormap_free_colors (color_map, &c->color, 1);
}
