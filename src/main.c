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

#if rep_INTERFACE >= 9
DEFSYM(rep, "rep");
#endif

static bool
on_idle (int since_last)
{
    if(remove_all_messages(true)
       || print_event_prefix()
       || auto_save_buffers(false))
    {
	return true;
    }
    else
	return false;
}

static void
on_termination (void)
{
    /* Autosave all buffers */
    while(auto_save_buffers(true))
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
	Fset (Qjade_directory, rep_string_copy(getenv("JADEDIR")));
    else
	Fset (Qjade_directory, rep_string_copy(JADE_DIR));

    rep_INTERN_SPECIAL(jade_lisp_lib_directory);
    if(getenv("JADELISPDIR") != 0)
	Fset (Qjade_lisp_lib_directory, rep_string_copy(getenv("JADELISPDIR")));
    else
	Fset (Qjade_lisp_lib_directory, rep_string_copy(JADE_LISPDIR));

    rep_INTERN_SPECIAL(jade_site_lisp_directory);
    if(getenv("JADESITELISPDIR") != 0)
	Fset (Qjade_site_lisp_directory,
	      rep_string_copy(getenv("JADESITELISPDIR")));
    else
	Fset (Qjade_site_lisp_directory,
	      rep_string_concat2(rep_STR(Fsymbol_value (Qjade_directory, Qt)),
			  "/site-lisp"));

    rep_INTERN_SPECIAL(jade_exec_directory);
    if(getenv("JADEEXECDIR") != 0)
	Fset (Qjade_exec_directory, rep_string_copy(getenv("JADEEXECDIR")));
    else
	Fset (Qjade_exec_directory, rep_string_copy(JADE_EXECDIR));

    if(getenv("JADEDOCFILE") != 0)
	Fset (Qdocumentation_file, rep_string_copy(getenv("JADEDOCFILE")));
    else
	Fset (Qdocumentation_file,
	      rep_string_concat2(rep_STR(Fsymbol_value(Qjade_directory, Qt)),
			  "/" JADE_VERSION "/DOC"));

    Fset (Qdocumentation_files,
	  Fcons(Fsymbol_value (Qdocumentation_file, Qt),
		Fsymbol_value (Qdocumentation_files, Qt)));

    Fset(Qload_path,
	 Fcons(Fsymbol_value (Qjade_lisp_lib_directory, Qt),
	       Fcons(Fsymbol_value (Qjade_site_lisp_directory, Qt),
		     Fsymbol_value (Qload_path, Qt))));

    Fset (Qdl_load_path,
	  Fcons(Fsymbol_value (Qjade_exec_directory, Qt),
		Fsymbol_value (Qdl_load_path, Qt)));

    rep_INTERN(invalid_area);
    rep_DEFINE_ERROR(invalid_area);
    rep_INTERN(window_error);
    rep_DEFINE_ERROR(window_error);
    rep_INTERN(invalid_pos);
    rep_DEFINE_ERROR(invalid_pos);
    rep_INTERN(buffer_read_only);
    rep_DEFINE_ERROR(buffer_read_only);
    rep_INTERN(bad_event_desc);
    rep_DEFINE_ERROR(bad_event_desc);

    rep_regsub_fun = jade_regsub;
    rep_regsublen_fun = jade_regsublen;
    rep_on_idle_fun = on_idle;
    rep_on_termination_fun = on_termination;
}    

bool
batch_mode_p (void)
{
    static bool mode, cached;
    if (!cached)
    {
	mode = Fsymbol_value (Qbatch_mode, Qt) != Qnil;
	cached = true;
    }
    return mode;
}

static repv
inner_main (repv arg)
{
    repv res = rep_load_environment (rep_string_copy ("jade"));
    if (res != 0)
    {
	if(!batch_mode_p ())
	    res = rep_top_level_recursive_edit ();
    }
    return res;
}

int
main(int argc, char **argv)
{
    int rc = 5;

    prog_name = *argv++; argc--;
    rep_init (prog_name, &argc, &argv);

    if (rep_get_option ("--version", 0))
    {
       printf ("jade version %s\n", JADE_VERSION);
       return 0;
    }

#if rep_INTERFACE >= 9
    rep_push_structure ("jade");
    rep_structure_exports_all (rep_structure, true);
    rep_structure_set_binds (rep_structure, true);
    rep_INTERN (rep);
    Frequire (Qrep);
#endif

    if (sys_init(prog_name))
    {
	jade_symbols();

	inner_main(Qnil);

	rc = rep_top_level_exit ();

	windows_kill();
	views_kill();
	buffers_kill();
	glyphs_kill();

	sys_kill();
	rep_kill();
    }
    return rc;
}
