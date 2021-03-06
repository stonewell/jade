/* mac_main.m -- Main code for Mac OS X window-system
   Copyright (C) 1999-2007 John Harper <jsh@unfactored.org>

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
#include "mac_internal.h"
#include <string.h>
#include <assert.h>

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

/* Command line options, and their default values. */

static int opt_width = 80, opt_height = 60;

unsigned long mac_meta_mod;

bool mac_app_is_active;

/* Default font name. */
DEFSTRING(def_font_str_data, DEFAULT_FONT);

/* Kind of a hack, we use this class directly as our app delegate. */

@interface JadeAppDelegate : NSObject

+ (void)applicationDidBecomeActive:(NSNotification *)notification;
+ (void)applicationDidResignActive:(NSNotification *)notification;

@end


/* Resource/option management */

static void
get_resources(void)
{
  OBJC_BEGIN

  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  NSString *str;
  double value;

  value = [defaults doubleForKey:@"windowWidth"];
  if (value != 0) {
    opt_width = (int)value;
  }

  value = [defaults doubleForKey:@"windowHeight"];
  if (value != 0) {
    opt_height = (int)value;
  }

  str = [defaults stringForKey:@"backgroundColor"];
  if (str != nil) {
    default_bg_color = strdup(str.UTF8String);
  }

  str = [defaults stringForKey:@"foregroundColor"];
  if (str != nil) {
    default_fg_color = strdup(str.UTF8String);
  }

  str = [defaults stringForKey:@"selectionColor"];
  if (str != nil) {
    default_block_color = strdup(str.UTF8String);
  }

  str = [defaults stringForKey:@"highlightColor"];
  if (str != nil) {
    default_hl_color = strdup(str.UTF8String);
  }

  str = [defaults stringForKey:@"modelineColor"];
  if (str != nil) {
    default_ml_color = strdup(str.UTF8String);
  }

  str = [defaults stringForKey:@"fontName"];
  if (str != nil) {
    def_font_str = rep_string_copy(str.UTF8String);
  }

  value = [defaults doubleForKey:@"fontSize"];
  if (value != 0) {
    mac_font_size = value;
  }

  OBJC_END
}

/* Scan the command line for options. */
static void
get_options(void)
{
    repv opt;
    if (rep_get_option("--width", &opt))
	opt_width = atoi(rep_STR(opt));
    if (rep_get_option("--height", &opt))
	opt_height = atoi(rep_STR(opt));
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
    if (rep_get_option("--font-size", &opt))
	mac_font_size = atof(rep_STR(opt));
}

/* After parsing the command line and the resource database, use the
   information. */
static bool
use_options(void)
{
    set_default_geometry (0, 0, opt_width, opt_height);
    return TRUE;
}

void
sys_beep (Lisp_Window *w)
{
    NSBeep ();
}

DEFSTRING(tilde_dir, "~");

/* Called from main(). */
bool
sys_init(char *program_name)
{
    if (!batch_mode_p ())
	setpgid (0, 0);

    OBJC_BEGIN
    [[NSApplication sharedApplication] setDelegate:(id)[JadeAppDelegate class]];
    OBJC_END

    if (!batch_mode_p ())
	Fset_default (Qdefault_directory, rep_VAL (&tilde_dir));

    def_font_str = rep_VAL (&def_font_str_data);
    get_resources();
    get_options ();
    use_options ();

    mac_meta_mod = mac_find_meta ();

    return true;
}

void
sys_kill (void)
{
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
	  "    FILE         Load FILE into an editor buffer\n", stderr);
}


/* Color handling. */

DEFSTRING(no_parse_color, "Can't parse color");

static int
hexvalue (int c)
{
  if (c >= '0' && c <= '9')
    return c - '0';
  else if (c >= 'a' && c <= 'f')
    return 10 + c - 'a';
  else if (c >= 'A' && c <= 'F')
    return 10 + c - 'A';
  else
    return 0;
}

static bool
mac_parse_color (const char *str, struct mac_color *c)
{
    static CGColorSpaceRef cs;
    CGFloat rgba[4];
    int len;

    if (str[0] == '#')
    {
	len = strlen ((const char *) str);

	switch (len)
	{
	case 4:
	case 5:
	    rgba[0] = hexvalue (str[1]) * (1.0 / 15.0);
	    rgba[1] = hexvalue (str[2]) * (1.0 / 15.0);
	    rgba[2] = hexvalue (str[3]) * (1.0 / 15.0);
	    rgba[3] = len == 4 ? 1.0 : hexvalue (str[4]) * (1.0 / 15.0);
	    break;
	case 7:
	case 9:
	  rgba[0] = (hexvalue (str[1]) * 16 + hexvalue (str[2])) * (1.0 / 255.0);
	  rgba[1] = (hexvalue (str[3]) * 16 + hexvalue (str[4])) * (1.0 / 255.0);
	  rgba[2] = (hexvalue (str[5]) * 16 + hexvalue (str[6])) * (1.0 / 255.0);
	  rgba[3] = len == 7 ? 1.0 : (hexvalue (str[7]) * 16 + hexvalue (str[8])) * (1.0 / 255.0);
	  break;
	default:
	  return false;
	}
    }
    else
    {
	static const struct {const char name[16]; CGFloat rgba[4];} colors[] =
	{
	    {"aqua", {0.0, 1.0, 1.0, 1.0}},
	    {"black", {0.0, 0.0, 0.0, 1.0}},
	    {"blue", {0.0, 0.0, 1.0, 1.0}},
	    {"fuchsia", {1.0, 0.0, 1.0, 1.0}},
	    {"gray", {0.5, 0.5, 0.5, 1.0}},
	    {"green", {0.0, 0.5, 0.0, 1.0}},
	    {"lime", {0.0, 1.0, 0.0, 1.0}},
	    {"maroon", {0.5, 0.0, 0.0, 1.0}},
	    {"navy", {0.0, 0.0, 0.5, 1.0}},
	    {"olive", {0.5, 0.5, 0.0, 1.0}},
	    {"purple", {0.5, 0.0, 0.5, 1.0}},
	    {"red", {1.0, 0.0, 0.0, 1.0}},
	    {"silver", {.75, .75, .75, 1.0}},
	    {"teal", {0.0, 0.5, 0.5, 1.0}},
	    {"white", {1.0, 1.0, 1.0, 1.0}},
	    {"yellow", {1.0, 1.0, 0.0, 1.0}},
	};
	const int n_colors = sizeof (colors) / sizeof (colors[0]);
	int i;

	for (i = 0; i < n_colors; i++)
	{
	    if (strcmp (colors[i].name, (const char *) str) == 0)
	    {
		memcpy (rgba, colors[i].rgba, sizeof (colors[i].rgba));
		break;
	    }
	}
	if (i == n_colors)
	    return false;
    }

    if (cs == NULL)
	cs = CGColorSpaceCreateDeviceRGB ();

    c->cg_color = CGColorCreate (cs, rgba);
    c->luminance = rgba[0] * .2125f + rgba[1] * .7154f + rgba[2] * .0721f;

    return true;
}

repv
sys_make_color(Lisp_Color *c)
{
    if (!mac_parse_color (rep_STR (c->name), &c->color))
	return Fsignal(Qerror, rep_list_2(rep_VAL(&no_parse_color), c->name));

    return rep_VAL (c);
}

void
sys_free_color(Lisp_Color *c)
{
    CGColorRelease (c->color.cg_color);
}


/* NSApplication delegate. */

@implementation JadeAppDelegate

+ (void)applicationDidBecomeActive:(NSNotification *)notification
{
    mac_app_is_active = true;
}

+ (void)applicationDidResignActive:(NSNotification *)notification
{
    mac_app_is_active = false;
}

@end
