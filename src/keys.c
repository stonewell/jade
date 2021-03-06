/* keys.c -- Key binding and evaluating (this should be called events.c)
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
#include <string.h>
#include <stdlib.h>

/* Function to make a hash key from an event (c == code, m == modifiers)  */
#define KEYTAB_HASH_FUN(c,m) (((c) * 33) + (((m) & EV_MOD_MASK)))

#define KEYTAB_SIZE 127


/* current_event holds the event we're processing (or 0s), last_event
   contains the previously processed event.  */
unsigned long current_event[2], last_event[2];

/* Pointer to the window system's representation of the current_event,
   used for cooking events into strings.  */
static void *current_os_event;

/* print_prefix means echo all events upto the end of the key-sequence.
   printed_this_prefix says the last event has been echoed. */
static bool print_prefix, printed_this_prefix;

/* Buffer holding the events making this key-sequence. */
#define EVENT_BUFSIZ 20
static unsigned long event_buf[EVENT_BUFSIZ]; /* one event = (code,mods) */
static int event_index;

DEFSYM(global_keymap, "global-keymap");
DEFSYM(local_keymap, "local-keymap");
DEFSYM(overriding_local_keymap, "overriding-local-keymap");
DEFSYM(unbound_key_hook, "unbound-key-hook");
DEFSYM(esc_means_meta, "esc-means-meta");
DEFSYM(keymap, "keymap");
DEFSYM(minor_mode_keymap_alist, "minor-mode-keymap-alist");
DEFSYM(next_keymap_path, "next-keymap-path");
DEFSYM(mouse_keymap, "mouse-keymap");

static repv next_keymap_path;

/* true when the Meta qualifier should be added to the next event. */
static bool pending_meta;

/* Some doc strings
::doc:esc-means-meta::
When this variable is non-nil the `ESC' key means that the next event
is qualified by the `Meta' modifier.
This feature is included mainly for compatibility with GNU Emacs.
::end::
*/


/* Keymap searching */

/* Search the keymap KM for a binding of CODE&MODS.
   If CALLBACK is non-nil it's a function to call for the binding found.
   If this function returns true, then this binding is acceptable and
   is returned from the function. */
static repv
search_keymap(repv km, unsigned long code, unsigned long mods, bool (*callback)(repv key))
{
    /* If it's a symbol, dereference it. */
    while(rep_SYMBOLP(km) && !rep_NILP(km) && !rep_INTERRUPTP)
    {
	repv tem = Fsymbol_value(km, Qt);
	if(tem == km)
	    break;
	km = tem;
	rep_TEST_INT;
    }

    /* Find the list of bindings to scan. */
    if(rep_VECTORP(km))
    {
	if(rep_VECTOR_LEN(km) != KEYTAB_SIZE)
	    return 0;
	km = rep_VECTI(km, KEYTAB_HASH_FUN(code, mods) % KEYTAB_SIZE);
    }
    else if rep_CONSP(km)
	km = rep_CDR(km);
    else
	return 0;

    /* Scan them for a match.. */
    while(rep_CONSP(km))
    {
	rep_TEST_INT; if(rep_INTERRUPTP) break;
	if(rep_CONSP(rep_CAR(km)))
	{
	    repv ev = KEY_EVENT(rep_CAR(km));
	    if((rep_INT(EVENT_MODS(ev)) == mods) && rep_INT(EVENT_CODE(ev)) == code)
	    {
		repv key = rep_CAR(km);
		if(callback == 0 || callback(key))
		    return key;
	    }
	    km = rep_CDR(km);
	}
	else
	    /* An inherited sub-keymap. Start scanning it */
	    km = rep_CDR(km);
    }
    return 0;
}

/* Search for a binding of CODE&MODS.  */
static repv
lookup_binding(unsigned long code, unsigned long mods, bool (*callback)(repv key))
{
    repv k = 0, nkp = next_keymap_path;
    next_keymap_path = 0;
    if(nkp == 0 || nkp == Qglobal_keymap)
    {
	repv tem;

	tem = Fsymbol_value(Qoverriding_local_keymap, Qt);
	if(tem && !rep_VOIDP(tem) && !rep_NILP(tem))
	{
	    /* overriding-local-keymap replaces all but global-keymap
	       if non-nil. */
	    k = search_keymap(tem, code, mods, callback);
	}
	else
	{
	    /* First scan the extents under the mouse for mouse-keymap
	       properties.. */
	    Lisp_View *vw;
	    for (vw = curr_win->view_list;
		 k == 0 && vw != 0;
		 vw = vw->next_view)
	    {
		int i;
		for (i = 0; k == 0 && i < vw->pointer_extents_count; i++)
		{
		    tem = Fextent_get(rep_VAL(vw->pointer_extents[i]),
				      Qmouse_keymap);
		    if (tem && tem != Qnil)
			k = search_keymap (tem, code, mods, callback);
		}
		if (k != 0)
		    Fset_current_view (rep_VAL (vw), Qnil);
	    }

	    /* ..then search all current extents.. */
	    tem = Fget_extent(Qnil, Qnil);
	    while(!k && EXTENTP(tem))
	    {
		k = search_keymap(Fextent_get(tem, Qkeymap),
				  code, mods, callback);
		tem = Fextent_parent(tem);
	    }

	    /* ..then the minor modes.. */
	    tem = Fsymbol_value(Qminor_mode_keymap_alist, Qt);
	    while(!k && rep_CONSP(tem) && rep_CONSP(rep_CAR(tem)) && !rep_INTERRUPTP)
	    {
		repv cell = rep_CAR(tem);
		if(rep_SYMBOLP(rep_CAR(cell)))
		{
		    repv cond;
		    rep_GC_root gc_tem;
		    rep_PUSHGC(gc_tem, tem);
		    cond = Fsymbol_value(rep_CAR(cell), Qt);
		    if(!rep_NILP(cond) && !rep_VOIDP(cond))
			k = search_keymap(rep_CDR(cell), code, mods, callback);
		    rep_POPGC;
		}
		tem = rep_CDR(tem);
		rep_TEST_INT;
	    }
	    /* ..then the local map */
	    if(!k)
		k = search_keymap(Qlocal_keymap, code, mods, callback);
	}

	/* Finally, scan the global map */
	if(!k)
	    k = search_keymap(Qglobal_keymap, code, mods, callback);
    }
    else
    {
	rep_GC_root gc_nkp;
	rep_PUSHGC(gc_nkp, nkp);
	while(!k && rep_CONSP(nkp))
	{
	    k = search_keymap(rep_CAR(nkp), code, mods, callback);
	    nkp = rep_CDR(nkp);
	}
	rep_POPGC;
    }
    return (k != 0 && KEYP(k)) ? KEY_COMMAND(k) : 0;
}

static bool
eval_input_callback(repv key)
{
    repv cmd = KEY_COMMAND(key);
    if(rep_SYMBOLP(cmd))
    {
	cmd = Fsymbol_value (cmd, Qt);
	if (rep_CLOSUREP(cmd))
	{
	    repv fun = rep_CLOSURE(cmd)->fun;
	    if(rep_CONSP(fun) && rep_CAR(fun) == Qautoload)
	    {
		/* An autoload, try to load it. */
		rep_GC_root gc_key;
#if rep_INTERFACE >= 9
		rep_PUSHGC (gc_key, key);
		cmd = rep_call_with_closure (cmd, rep_load_autoload, cmd);
		rep_POPGC;
#else
		struct rep_Call lc;
		lc.fun = lc.args = lc.args_evalled_p = Qnil;
		rep_PUSH_CALL(lc);
		rep_USE_CLOSURE(cmd);
		rep_PUSHGC(gc_key, key);
		cmd = rep_load_autoload(cmd);
		rep_POPGC;
		rep_POP_CALL(lc);
#endif
		if(cmd == 0)
		    return false;
	    }
	}
    }
    if(Fkeymapp (cmd) != Qnil)
    {
	/* A prefix key, add its list to the next-keymap-path. */
	next_keymap_path = Fcons(cmd, next_keymap_path
				 ? next_keymap_path : Qnil);
	/* Look for more prefix keys */
	return false;
    }
    if (cmd == Qnil)
	return false;
    next_keymap_path = 0;
    return true;
}

struct eval_input_data {
    void *osinput;
    unsigned long code, mods;
};

static repv
inner_eval_input_event(repv data_)
{
    struct eval_input_data *data = (struct eval_input_data *) rep_PTR(data_);
    unsigned long code = data->code;
    unsigned long mods = data->mods;

    repv result = Qnil;
    event_buf[event_index++] = code;
    event_buf[event_index++] = mods;
    if(event_index == EVENT_BUFSIZ)
	event_index = 0;
    printed_this_prefix = false;
    if(!rep_NILP(Fsymbol_value (Qesc_means_meta, Qt))
       && !pending_meta
       && (code == esc_code) && (mods == esc_mods))
    {
	/* Treat this ESC as a Meta-prefix. */
	pending_meta = true;
    }
    else
    {
	Lisp_View *vw = curr_vw;
	repv cmd, orig_next_keymap_path = next_keymap_path;
	if(pending_meta)
	{
	    mods |= EV_MOD_META;
	    pending_meta = false;
	}
	current_event[0] = code;
	current_event[1] = mods;
	current_os_event = data->osinput;
	cmd = lookup_binding(code, mods, eval_input_callback);
	if(cmd != 0)
	{
	    /* Found a binding for this event; evaluate it. */
	    result = Fcall_command(cmd, Qnil);
	}
	else if(next_keymap_path != 0)
	{
	    /* We already handled some prefixes. */
	    Fset (Qthis_command, Qkeymap);
	    result = Qnil;
	}
	else if(orig_next_keymap_path != 0
		&& !rep_NILP(orig_next_keymap_path)
		&& orig_next_keymap_path != Qglobal_keymap)
	{
	    /* A multi-key binding, but no final step; clear the prefix
	       argument for the next command and beep. */
	    Fset (Qprefix_arg, Qnil);
	    Fbeep();
	}
	else
	{
	    /* An unbound key with no prefix keys. */
	    result = Fcall_hook(Qunbound_key_hook, Qnil, Qor);
	    if(result != 0 && rep_NILP(result)
	       && (mods & EV_TYPE_KEYBD) && data->osinput)
	    {
		/* Try to self-insert */
		char buff[256];
		size_t len;
		len = sys_cook_key(data->osinput, buff, 256 - 1);
		if(len > 0)
		{
		    buff[len] = 0;
		    if(!read_only_pos(vw->tx, vw->cursor_pos))
		    {
			repv old_undo_head = 0;
			Fcall_hook(Qpre_command_hook, Qnil, Qnil);
			if(Fsymbol_value (Qlast_command, Qt) == Qt
			   && rep_CONSP(vw->tx->undo_list)
			   && rep_NILP(rep_CAR(vw->tx->undo_list)))
			{
			    /* Last command was also an insertion, fix it
			       so that the undo information is merged. */
			    old_undo_head = vw->tx->undo_list;
			    vw->tx->undo_list = rep_CDR(vw->tx->undo_list);
			}
			if(pad_cursor(vw))
			{
			    repv arg = (Fprefix_numeric_argument
					(Fsymbol_value (Qprefix_arg, Qt)));
			    if(!rep_INTP(arg) || rep_INT(arg) < 1)
				Fbeep();
			    else if(rep_INT(arg) == 1)
			    {
				insert_string(vw->tx, buff,
					      len, vw->cursor_pos);
			    }
			    else if(len == 1
				    && rep_INT(arg) < sizeof(buff) - 1)
			    {
				/* Inserting a single char, more than
				   once, build a string of them. */
				memset(buff, buff[0], rep_INT(arg));
				insert_string(vw->tx, buff,
					      rep_INT(arg), vw->cursor_pos);
			    }
			    else
			    {
				/* Do a looping insertion */
				int i = rep_INT(arg);
				while(i-- > 0)
				{
				    insert_string(vw->tx, buff,
						  len, vw->cursor_pos);
				}
			    }
			}
			if(old_undo_head != 0)
			{
			    rep_CDR(old_undo_head) = vw->tx->undo_list;
			    vw->tx->undo_list = old_undo_head;
			}
			Fcall_hook(Qpost_command_hook, Qnil, Qnil);
			Fset (Qlast_command, Qt);
			result = Qt;
		    }
		    else
			result = 0;
		}
		/* Remove prefix arg. */
		Fset (Qprefix_arg, Qnil);
	    }
	}
	last_event[0] = current_event[0];
	last_event[1] = current_event[1];
	current_event[0] = current_event[1] = 0;
	current_os_event = NULL;
    }
    if(curr_vw)
    {
	if(print_prefix)
	{
	    print_event_prefix();
	    if(next_keymap_path == 0 && !pending_meta)
		print_prefix = false;
	}
    }
    if(next_keymap_path == 0 && !pending_meta)
	event_index = 0;
    return(result);
}

/* Process the event CODE+MODS. OS-INPUT-MSG is the raw input event
   from the window-system, this is only used to cook a string from.  */
repv
eval_input_event(void *osinput, unsigned long code, unsigned long mods)
{
    struct eval_input_data data;
    data.osinput = osinput;
    data.code = code;
    data.mods = mods;
    return inner_eval_input_event(rep_VAL(&data));
}


/* Translate text->event and vice versa */

struct key_def {
    const char *name;
    unsigned long mods, code;
};

static struct key_def default_mods[] = {
    { "S",	  EV_MOD_SHIFT },
    { "Shift",    EV_MOD_SHIFT },
    { "SFT",      EV_MOD_SHIFT },
    { "C",        EV_MOD_CTRL },
    { "Ctrl",     EV_MOD_CTRL },
    { "Control",  EV_MOD_CTRL },
    { "CTL",      EV_MOD_CTRL },
    { "M",        EV_MOD_META },
    { "Meta",     EV_MOD_META },
    { "Mod1",     EV_MOD_MOD1 },
    { "Mod2",     EV_MOD_MOD2 },
    { "Mod3",     EV_MOD_MOD3 },
    { "Mod4",     EV_MOD_MOD4 },
    { "Button1",  EV_MOD_BUTTON1 },
    { "Button2",  EV_MOD_BUTTON2 },
    { "Button3",  EV_MOD_BUTTON3 },
    { "Button4",  EV_MOD_BUTTON4 },
    { "Button5",  EV_MOD_BUTTON5 },
    { 0, 0 }
};

static struct key_def default_codes[] = {
    { "Click1",   EV_TYPE_MOUSE, EV_CODE_MOUSE_CLICK1 },
    { "Click2",   EV_TYPE_MOUSE, EV_CODE_MOUSE_CLICK2 },
    { "Off",      EV_TYPE_MOUSE, EV_CODE_MOUSE_UP },
    { "Move",     EV_TYPE_MOUSE, EV_CODE_MOUSE_MOVE },
    { 0, 0 }
};

/* Puts the integers defining the event described in DESC into CODE
   and MODS. */
bool
lookup_event(unsigned long *code, unsigned long *mods, const char *desc)
{
    const char *tem;
    char buf[100];
    *code = *mods = 0;

    /* First handle all modifiers */
    while(*desc && (tem = strchr(desc + 1, '-')) != 0)
    {
	struct key_def *x = default_mods;

	memcpy(buf, desc, tem - (char *)desc);
	buf[tem - desc] = 0;

	while(x->name != 0)
	{
	    if(strcasecmp(buf, x->name) == 0)
	    {
		*mods |= x->mods;
		break;
	    }
	    x++;
	}
	if(x->name == 0 && !sys_lookup_mod(buf, mods))
	    goto error;

	desc = tem + 1;
    }

    /* Then go for the code itself */
    {
	struct key_def *x = default_codes;
	while(x->name != 0)
	{
	    if(strcasecmp(desc, x->name) == 0)
	    {
		*mods |= x->mods;
		*code = x->code;
		return true;
	    }
	    x++;
	}
	if(sys_lookup_code(desc, code, mods))
	    return true;
	else
	    goto error;
    }

error:
    Fsignal(Qbad_event_desc, rep_LIST_1(rep_string_copy(desc)));
    return false;
}

/* Constructs the name of the event defined by CODE and MODS in BUF.  */
bool
lookup_event_name(char *buf, unsigned long code, unsigned long mods)
{
    int i;
    struct key_def *x;
    unsigned long type = mods & EV_TYPE_MASK;

    char *end = buf;
    *buf = 0;

    if(mods & WINDOW_META(curr_win))
	mods = (mods & ~WINDOW_META(curr_win)) | EV_MOD_META;
    mods &= ~EV_TYPE_MASK;

    for(i = 2; i < 32 && mods != 0; i++)	/* magic numbers!? */
    {
	unsigned long mask = 1 << i;
	if(mods & mask)
	{
	    mods &= ~mask;
	    x = default_mods;
	    while(x->name != 0)
	    {
		if(x->mods == mask)
		{
		    end = stpcpy(end, x->name);
		    break;
		}
		x++;
	    }
	    if(x->name == 0)
		end = sys_lookup_mod_name(end, mask);
	    end = stpcpy(end, "-");
	}
    }

    x = default_codes;
    while(x->name != 0)
    {
	if(type == x->mods && code == x->code)
	{
	    strcpy(end, x->name);
	    return true;
	}
	x++;
    }
    return sys_lookup_code_name(end, code, type);
}


/* Lisp functions */
DEFUN("make-keymap", Fmake_keymap, Smake_keymap, (void), rep_Subr0) /*
::doc:make-keymap::
make-keymap

Return a new keymap suitable for storing bindings in. This is a 127
element vector, each element is an empty list of bindings for that hash
bucket. Compare with the make-sparse-keymap function.
::end:: */
{
    return Fmake_vector(rep_MAKE_INT(KEYTAB_SIZE), Qnil);
}

DEFUN("make-sparse-keymap", Fmake_sparse_keymap, Smake_sparse_keymap,
      (repv base), rep_Subr1) /*
::doc:make-sparse-keymap::
make-sparse-keymap [BASE-KEYMAP]

Return a new keymap suitable for storing bindings in. This is a cons cell
looking like `(keymap . LIST-OF-BINDINGS)', LIST-OF-BINDINGS is initially
nil.

If BASE-KEYMAP is non-nil, it should be an existing sparse keymap whose
bindings are to be inherited by the new keymap.
::end:: */
{
    if(!rep_NILP(base) && (!rep_CONSP(base) || rep_CAR(base) != Qkeymap))
	return rep_signal_arg_error(base, 1);

    return Fcons(Qkeymap, base);
}

DEFUN("bind-keys", Fbind_keys, Sbind_keys, (repv args), rep_SubrL) /*
::doc:bind-keys::
bind-keys KEYMAP { EVENT-DESCRIPTION COMMAND }...

Adds key bindings to KEYMAP. Each EVENT-DESCRIPTION is a string naming an
event to bind to the corresponding COMMAND.

Returns KEYMAP when successful.
::end:: */
{
    bool rc = true;
    repv km, arg1, res = 0;
    if(!rep_CONSP(args))
	return 0;
    km = rep_CAR(args);
    args = rep_CDR(args);
    while(rc && rep_CONSP(args) && rep_CONSP(rep_CDR(args)))
    {
	unsigned long code, mods;
	repv key;
	arg1 = rep_CAR(args);
	args = rep_CDR(args);
	if(rep_STRINGP(arg1))
	{
	    if(!lookup_event(&code, &mods, rep_STR(arg1)))
		goto end;
	}
	else if(!rep_NILP(Feventp(arg1)))
	{
	    code = rep_INT(rep_CAR(arg1));
	    mods = rep_INT(rep_CDR(arg1));
	}
	else
	{
	    Fsignal(Qbad_event_desc, rep_LIST_1(arg1));
	    goto end;
	}
	rc = false;
	key = MAKE_KEY(MAKE_EVENT(rep_MAKE_INT(code), rep_MAKE_INT(mods)), rep_CAR(args));
	if(key != 0)
	{
	    if(rep_VECTORP(km))
	    {
		unsigned long hash = KEYTAB_HASH_FUN(code, mods) % KEYTAB_SIZE;
		repv old = rep_VECTI(km, hash);
		rep_VECTI(km, hash) = Fcons(key, old);
	    }
	    else
		rep_CDR(km) = Fcons(key, rep_CDR(km));
	    args = rep_CDR(args);
	    rc = true;
	}
	else
	    goto end;
    }
    if(rc)
	res = km;
end:
    return res;
}

DEFUN("unbind-keys", Funbind_keys, Sunbind_keys, (repv args), rep_SubrL) /*
::doc:unbind-keys::
unbind-keys KEY-MAP EVENT-DESCRIPTION...
::end:: */
{
    bool rc = true;
    repv km, arg1, res = 0;
    if(!rep_CONSP(args))
	return 0;
    km = rep_CAR(args);
    if(!((rep_VECTORP(km) && rep_VECTOR_LEN(km) == KEYTAB_SIZE)
       || rep_CONSP(km)))
	return(rep_signal_arg_error(km, 1));
    args = rep_CDR(args);
    while(rc && rep_CONSP(args))
    {
	unsigned long code, mods;
	repv *keyp;
	arg1 = rep_CAR(args);
	if(rep_STRINGP(arg1))
	{
	    if(!lookup_event(&code, &mods, rep_STR(arg1)))
		goto end;
	}
	else if(!rep_NILP(Feventp(arg1)))
	{
	    code = rep_INT(rep_CAR(arg1));
	    mods = rep_INT(rep_CDR(arg1));
	}
	else
	{
	    Fsignal(Qbad_event_desc, rep_LIST_1(arg1));
	    goto end;
	}
	rc = false;
	if(rep_VECTORP(km))
	    keyp = &rep_VECTI(km, KEYTAB_HASH_FUN(code, mods) % KEYTAB_SIZE);
	else
	    keyp = &rep_CDR(km);
	while(rep_CONSP(*keyp))
	{
	    repv cell = rep_CAR(*keyp);
	    if(rep_CONSP(cell))
	    {
		if((rep_INT(EVENT_MODS(KEY_EVENT(cell))) == mods)
		   && (rep_INT(EVENT_CODE(KEY_EVENT(cell))) == code))
		{
		    *keyp = rep_CDR(*keyp);
		    /* Keybindings are supposed to nest so only delete the
		    first entry for this event  */
		    break;
		}
		else
		    keyp = &rep_CDR(*keyp);
	    }
	    else
		/* An inherited keymap. Only delete bindings from
		   the initial keymap. */
		break;

	    rep_TEST_INT; if(rep_INTERRUPTP) return 0;
	}
	rc = true;
	args = rep_CDR(args);
    }
    if(rc)
	res = Qt;
end:
    return(res);
}

DEFUN("next-keymap-path", Fnext_keymap_path, Snext_keymap_path,
      (repv path), rep_Subr1) /*
::doc:next-keymap-path::
next-keymap-path [PATH]

Install a temporary list of keymaps, PATH, to be used when the next input
event is received. This function sets this-command to nil, and prefix-arg
to current-prefix-arg (i.e. it preserves the context for the next command).

As a special case, if PATH is the symbol t, the keymap to be used
for the next input event is returned, without changing anything.
Alternatively, if PATH is the symbol `global-keymap', use the currently
active root keymaps.
::end:: */
{
    if(path != Qt)
    {
	next_keymap_path = path;
	/* This isn't a true command */
	Fset (Qthis_command, Qnil);
	/* Pass the prefix-arg along */
	Fset (Qprefix_arg, Fsymbol_value (Qcurrent_prefix_arg, Qt));
    }
    return next_keymap_path ? next_keymap_path : Qnil;
}

DEFSTRING(not_in_handler, "Not in event handler");
DEFUN("current-event-string", Fcurrent_event_string, Scurrent_event_string, (void), rep_Subr0) /*
::doc:current-event-string::
current-event-string

Returns the string which would have been inserted by the current event if
a Lisp function hadn't been called instead.
::end:: */
{
    char buff[256];
    int len;
    if(!current_os_event)
	return(Fsignal(Qerror, rep_LIST_1(rep_VAL(&not_in_handler))));
    len = sys_cook_key(current_os_event, buff, 256 - 1);
    if(len > 0)
	return(rep_string_copy_n(buff, len));
    return(rep_null_string());
}

DEFUN("current-event", Fcurrent_event, Scurrent_event, (void), rep_Subr0) /*
::doc:current-event::
current-event

Return the event which caused the current command to be invoked.
::end:: */
{
    if(current_event[1])
	return MAKE_EVENT(rep_MAKE_INT(current_event[0]),
			  rep_MAKE_INT(current_event[1]));
    else
	return Qnil;
}

DEFUN("last-event", Flast_event, Slast_event, (void), rep_Subr0) /*
::doc:last-event::
last-event

Return the previous event which occurred.
::end:: */
{
    if(last_event[1])
	return MAKE_EVENT(rep_MAKE_INT(last_event[0]),
			  rep_MAKE_INT(last_event[1]));
    else
	return Qnil;
}

DEFUN("event-name", Fevent_name, Sevent_name, (repv ev), rep_Subr1) /*
::doc:event-name::
event-name EVENT

Returns a string naming the event EVENT.
::end:: */
{
    char buf[256];
    if(!EVENTP(ev))
	return rep_signal_arg_error(ev, 1);

    if(lookup_event_name(buf, rep_INT(EVENT_CODE(ev)), rep_INT(EVENT_MODS(ev))))
	return rep_string_copy(buf);
    else
	return Qnil;
}

DEFUN("lookup-event", Flookup_event, Slookup_event, (repv name), rep_Subr1) /*
::doc:lookup-event::
lookup-event EVENT-NAME

Return the event whose name is EVENT-NAME.
::end:: */
{
    unsigned long code, mods;
    rep_DECLARE1(name, rep_STRINGP);

    if(lookup_event(&code, &mods, rep_STR(name)))
	return MAKE_EVENT(rep_MAKE_INT(code), rep_MAKE_INT(mods));
    else
	return Qnil;
}

DEFUN("lookup-event-binding", Flookup_event_binding, Slookup_event_binding, (repv ev), rep_Subr1) /*
::doc:lookup-event-binding::
lookup-event-binding EVENT

Return the command currently associated with the event EVENT.
::end:: */
{
    repv res;
    if(!EVENTP(ev))
	return(rep_signal_arg_error(ev, 1));

    res = lookup_binding(rep_INT(EVENT_CODE(ev)),
			 rep_INT(EVENT_MODS(ev)),
			 eval_input_callback);
    if (res == 0 && next_keymap_path != 0)
    {
	/* A prefix binding. Fake a function call to next-keymap-path. */
	if (rep_CONSP(next_keymap_path))
	{
	    res = Fcons(Qnext_keymap_path,
			   Fcons(Fcons(Qquote,
					     Fcons(next_keymap_path,
						      Qnil)),
				    Qnil));
	}
    }
    next_keymap_path = 0;
    return res ? res : Qnil;
}

DEFUN("search-keymap", Fsearch_keymap, Ssearch_keymap,
      (repv ev, repv km), rep_Subr2) /*
::doc:search-keymap::
search-keymap EVENT KEYMAP

Return the (COMMAND . EVENT) binding of EVENT in KEYMAP, or nil.
::end:: */
{
    repv res;
    rep_DECLARE1(ev, EVENTP);
    res = search_keymap(km, rep_INT(EVENT_CODE(ev)), rep_INT(EVENT_MODS(ev)), 0);
    return res ? res : Qnil;
}

DEFUN("keymapp", Fkeymapp, Skeymapp, (repv arg), rep_Subr1) /*
::doc:keymapp::
keymapp ARG

Returns t if ARG can be used as a keymap.
::end:: */
{
    if((rep_VECTORP(arg) && rep_VECTOR_LEN(arg) == KEYTAB_SIZE)
       || (rep_CONSP(arg) && rep_CAR(arg) == Qkeymap))
	return Qt;
    else
	return Qnil;
}

DEFUN("eventp", Feventp, Seventp, (repv arg), rep_Subr1) /*
::doc:eventp::
eventp ARG

Returns t if the ARG is an input event.
::end:: */
{
    return EVENTP(arg) ? Qt : Qnil;
}

/* If necessary, print the name of the current event prefix and return
   true, else return false.  */
bool
print_event_prefix(void)
{
    int i;
    char buf[256];
    char *bufp = buf;
    if((next_keymap_path == 0 && !pending_meta)
       && (!print_prefix || printed_this_prefix))
    {
	print_prefix = false;
	return(false);
    }
    if(!print_prefix)
	print_prefix = true;
    for(i = 0; i < event_index; i += 2)
    {
	if(lookup_event_name(bufp, event_buf[i], event_buf[i+1]))
	{
	    bufp += strlen(bufp);
	    *bufp++ = ' ';
	}
    }
    if(next_keymap_path != 0 || pending_meta)
    {
	if(bufp > buf)
	    bufp--;			/* erase the last space */
	*bufp++ = '.';
	*bufp++ = '.';
	*bufp++ = '.';
    }
    messagen(buf, bufp - buf);
    printed_this_prefix = true;
    return(true);
}

void
keys_init(void)
{
    rep_INTERN_SPECIAL(global_keymap);
    rep_INTERN_SPECIAL(local_keymap);
    rep_INTERN_SPECIAL(overriding_local_keymap);
    rep_INTERN_SPECIAL(unbound_key_hook);
    rep_INTERN_SPECIAL(esc_means_meta);
    Fset (Qesc_means_meta, Qt);
    rep_INTERN(keymap);
    rep_INTERN_SPECIAL(minor_mode_keymap_alist);
    rep_INTERN(next_keymap_path);
    next_keymap_path = 0;
    rep_mark_static(&next_keymap_path);
    rep_INTERN(mouse_keymap);

    rep_ADD_SUBR(Smake_keymap);
    rep_ADD_SUBR(Smake_sparse_keymap);
    rep_ADD_SUBR(Sbind_keys);
    rep_ADD_SUBR(Sunbind_keys);
    rep_ADD_SUBR(Snext_keymap_path);
    rep_ADD_SUBR(Scurrent_event_string);
    rep_ADD_SUBR(Scurrent_event);
    rep_ADD_SUBR(Slast_event);
    rep_ADD_SUBR(Sevent_name);
    rep_ADD_SUBR(Slookup_event);
    rep_ADD_SUBR(Slookup_event_binding);
    rep_ADD_SUBR(Ssearch_keymap);
    rep_ADD_SUBR(Skeymapp);
    rep_ADD_SUBR(Seventp);
}
