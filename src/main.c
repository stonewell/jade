/* main.c -- Entry point for Jade
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

#include "jade.h"
#include "build.h"
#include <string.h>
#include <limits.h>

static char *prog_name;

DEFSYM(jade_directory, "jade-directory");
DEFSYM(jade_lisp_lib_directory, "jade-lisp-lib-directory");
DEFSYM(jade_site_lisp_directory, "jade-site-lisp-directory");
DEFSYM(jade_exec_directory, "jade-exec-directory"); /*
::doc:jade-directory::
The directory in which all of Jade's installed data files live.
::end::
::doc:jade-lisp-lib-directory::
The name of the directory in which the standard lisp files live.
::end::
::doc:jade-site-lisp-directory::
The name of the directory in which site-specific Lisp files are stored.
::end::
::doc:jade-exec-directory::
The name of the directory containing Jade's architecture specific files.
::end:: */

/* some errors */
DEFSYM(invalid_area, "invalid-area");
DEFSTRING(err_invalid_area, "Invalid area");
DEFSYM(window_error, "window-error");
DEFSTRING(err_window_error, "Window error");
DEFSYM(invalid_pos, "invalid-pos");
DEFSTRING(err_invalid_pos, "Invalid position");
DEFSYM(buffer_read_only, "buffer-read-only");
DEFSTRING(err_buffer_read_only, "Buffer is read-only");
DEFSYM(bad_event_desc, "bad-event-desc");
DEFSTRING(err_bad_event_desc, "Invalid event description");

static rep_bool
on_idle (int since_last)
{
    if(remove_all_messages(TRUE)
       || print_event_prefix()
       || auto_save_buffers(FALSE))
    {
	return rep_TRUE;
    }
    else
	return rep_FALSE;
}

static void
on_termination (void)
{
    /* Autosave all buffers */
    while(auto_save_buffers(TRUE) > 0)
	;
}

static void
jade_symbols (void)
{
    files_init();
    buffers_init();
    commands_init();
    edit_init();
    find_init();
    extent_init();
    glyphs_init();
    keys_init();
    misc_init();
    movement_init();
    redisplay_init();
    undo_init();
    views_init();
    windows_init();
    sys_windows_init();

    if(!faces_init() || !first_buffer())
	exit (10);

    rep_INTERN_SPECIAL(jade_directory);
    if(getenv("JADEDIR") != 0)
	rep_SYM(Qjade_directory)->value = rep_string_dup(getenv("JADEDIR"));
    else
	rep_SYM(Qjade_directory)->value = rep_string_dup(JADE_DIR);

    rep_INTERN_SPECIAL(jade_lisp_lib_directory);
    if(getenv("JADELISPDIR") != 0)
	rep_SYM(Qjade_lisp_lib_directory)->value
	    = rep_string_dup(getenv("JADELISPDIR"));
    else
	rep_SYM(Qjade_lisp_lib_directory)->value
	    = rep_string_dup(JADE_LISPDIR);

    rep_INTERN_SPECIAL(jade_site_lisp_directory);
    if(getenv("JADESITELISPDIR") != 0)
	rep_SYM(Qjade_site_lisp_directory)->value
	    = rep_string_dup(getenv("JADESITELISPDIR"));
    else
	rep_SYM(Qjade_site_lisp_directory)->value
	    = rep_concat2(rep_STR(rep_SYM(Qjade_directory)->value),
			  "/site-lisp");

    rep_INTERN_SPECIAL(jade_exec_directory);
    if(getenv("JADEEXECDIR") != 0)
	rep_SYM(Qjade_exec_directory)->value
	    = rep_string_dup(getenv("JADEEXECDIR"));
    else
	rep_SYM(Qjade_exec_directory)->value = rep_string_dup(JADE_EXECDIR);

    if(getenv("JADEDOCFILE") != 0)
	rep_SYM(Qdocumentation_file)->value
	    = rep_string_dup(getenv("JADEDOCFILE"));
    else
	rep_SYM(Qdocumentation_file)->value
	    = rep_concat2(rep_STR(rep_SYM(Qjade_directory)->value),
			  "/" JADE_VERSION "/DOC");

    rep_SYM(Qdocumentation_files)->value
	= Fcons(rep_SYM(Qdocumentation_file)->value,
		rep_SYM(Qdocumentation_files)->value);

    rep_SYM(Qload_path)->value
	= Fcons(rep_SYM(Qjade_lisp_lib_directory)->value,
		Fcons(rep_SYM(Qjade_site_lisp_directory)->value,
		      rep_SYM(Qload_path)->value));

    rep_SYM(Qdl_load_path)->value = Fcons(rep_SYM(Qjade_exec_directory)->value,
					  rep_SYM(Qdl_load_path)->value);

    rep_INTERN(invalid_area); rep_ERROR(invalid_area);
    rep_INTERN(window_error); rep_ERROR(window_error);
    rep_INTERN(invalid_pos); rep_ERROR(invalid_pos);
    rep_INTERN(buffer_read_only); rep_ERROR(buffer_read_only);
    rep_INTERN(bad_event_desc); rep_ERROR(bad_event_desc);

    rep_regsub_fun = jade_regsub;
    rep_regsublen_fun = jade_regsublen;
    rep_on_idle_fun = on_idle;
    rep_on_termination_fun = on_termination;
}    

static void
usage (void)
{
    fputs ("\
    --version    print version details\n\
    --no-rc      don't load rc or site-init files\n\
    -f FUNCTION  call the Lisp function FUNCTION\n\
    -l FILE      load the file of Lisp forms called FILE\n\
    -q           quit\n"
          , stderr);
}

int
main(int argc, char **argv)
{
    int rc = 5;

    prog_name = *argv++; argc--;
    rep_init (prog_name, &argc, &argv, 0, usage);

    if (rep_get_option ("--version", 0))
    {
       printf ("jade version %s\n", JADE_VERSION);
       return 0;
    }

    if (sys_init(prog_name))
    {
	repv res;

	jade_symbols();

	res = Fload(rep_string_dup ("jade"), Qnil, Qnil, Qnil, Qnil);
	if (res != rep_NULL)
	{
	    rc = 0;
	    if(rep_SYM(Qbatch_mode)->value == Qnil)
		res = Frecursive_edit ();
	}
	else if(rep_throw_value && rep_CAR(rep_throw_value) == Qquit)
	{
	    if(rep_INTP(rep_CDR(rep_throw_value)))
		rc = rep_INT(rep_CDR(rep_throw_value));
	    else
		rc = 0;
	    rep_throw_value = 0;
	}

	if(rep_throw_value && rep_CAR(rep_throw_value) == Qerror)
	{
	    /* If quitting due to an error, print the error cell if
	       at all possible. */
	    repv stream = Fstderr_file();
	    repv old_tv = rep_throw_value;
	    rep_GC_root gc_old_tv;
	    rep_PUSHGC(gc_old_tv, old_tv);
	    rep_throw_value = rep_NULL;
	    if(stream && rep_FILEP(stream))
	    {
		fputs("error--> ", stderr);
		Fprin1(rep_CDR(old_tv), stream);
		fputc('\n', stderr);
	    }
	    else
		fputs("jade: error in initialisation\n", stderr);
	    rep_throw_value = old_tv;
	    rep_POPGC;
	}

	windows_kill();
	views_kill();
	buffers_kill();
	glyphs_kill();

	sys_kill();
	rep_kill();
    }
    return rc;
}
