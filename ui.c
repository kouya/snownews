// This file is part of Snownews - A lightweight console RSS newsreader
//
// Copyright (c) 2003-2004 Oliver Feiler <kiza@kcore.de>
// Copyright (c) 2021 Mike Sharov <msharov@users.sourceforge.net>
//
// Snownews is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 3
// as published by the Free Software Foundation.
//
// Snownews is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Snownews. If not, see http://www.gnu.org/licenses/.

#include "ui.h"
#include "main.h"
#include "about.h"
#include "cat.h"
#include "conv.h"
#include "dialog.h"
#include "feedio.h"
#include "setup.h"
#include "uiutil.h"
#include <ncurses.h>
#include <libxml/parser.h>
#ifdef UTF_8
#define xmlStrlen(s) xmlUTF8Strlen(s)
#endif

//----------------------------------------------------------------------

// Scrollable feed->description.
struct scrolltext {
    char* line;
    struct scrolltext* next;
    struct scrolltext* prev;
};

//----------------------------------------------------------------------

static bool resize_dirty = false;

//----------------------------------------------------------------------

void sig_winch (int p __attribute__((unused)))
{
    resize_dirty = true;
}

// View newsitem in scrollable window.
// Speed of this code has been greatly increased in 1.2.1.
static void UIDisplayItem (const struct newsitem* current_item, const struct feed* current_feed)
{
    struct scrolltext* first_line = NULL;	// Linked list of lines
    unsigned linenumber = 0;	// First line on screen (scrolling). Ugly hack.
    unsigned maxlines = 0;
    bool rewrap = true;

    while (1) {
	erase();

	const char* feed_title = current_feed->description;
	if (!feed_title)
	    feed_title = current_feed->title;
	if (!feed_title)
	    feed_title = "* " SNOWNEWS_NAME " " SNOWNEWS_VERSTRING;

	// Convert feed title to current locale
	char* dejunked_title = UIDejunk (feed_title);
	if (!dejunked_title)
	    dejunked_title = strdup (feed_title);
	char* converted_title = iconvert (dejunked_title);
	if (!converted_title)
	    converted_title = strdup (dejunked_title);
	free (dejunked_title);

	// Print feed title
	UISupportDrawHeader (converted_title);
	free (converted_title);

	// Print publishing date if we have one.
	if (current_item->data->date) {
	    char* date_str = unixToPostDateString (current_item->data->date);
	    if (date_str) {
		move (0, COLS - strlen(date_str) - 2);
		attron (WA_REVERSE);
		addch (' ');
		addstr (date_str);
		addch (' ');
		attroff (WA_REVERSE);
		free (date_str);
	    }
	}

	// Print item title
	unsigned ydesc = 2, xdesc = 1;
	if (current_item->data->title) {
	    dejunked_title = UIDejunk (current_item->data->title);
	    if (!dejunked_title)
		dejunked_title = strdup (current_item->data->title);
	    converted_title = iconvert (dejunked_title);
	    if (!converted_title)
		converted_title = strdup (dejunked_title);
	    free (dejunked_title);
	    unsigned titlelen = xmlStrlen ((xmlChar*) converted_title);
	    unsigned xtitle = xdesc;
	    if (titlelen < COLS - xdesc*2)
		xtitle = (COLS - titlelen) / 2u;
	    move (ydesc, xtitle);
	    attr_set (WA_BOLD, 2, NULL);
	    add_utf8 (converted_title);
	    attr_set (WA_NORMAL, 0, NULL);
	    free (converted_title);
	    ydesc += 2;
	}

	// Print item text
	if (!current_item->data->description || !current_item->data->description[0])
	    mvadd_utf8 (ydesc, xdesc, _("No description available."));
	else {
	    // Only generate a new scroll list if we need to rewrap everything.
	    // Otherwise just typeaheadskip this block.
	    if (rewrap) {
		char* converted = iconvert (current_item->data->description);
		if (converted == NULL)
		    converted = strdup (current_item->data->description);
		char* newtext = UIDejunk (converted);
		free (converted);
		char* newtextwrapped = WrapText (newtext, COLS - 4);
		free (newtext);
		char* freeme = newtextwrapped;	// Set ptr to str start so we can free later.

		// Split newtextwrapped at \n and put into double linked list.
		while (1) {
		    char* textslice = strsep (&newtextwrapped, "\n");

		    // Find out max number of lines text has.
		    ++maxlines;

		    if (textslice != NULL) {
			struct scrolltext* textlist = malloc (sizeof (struct scrolltext));
			textlist->line = strdup (textslice);

			// Gen double list with new items at bottom.
			textlist->next = NULL;
			if (first_line == NULL) {
			    textlist->prev = NULL;
			    first_line = textlist;
			} else {
			    textlist->prev = first_line;
			    while (textlist->prev->next != NULL)
				textlist->prev = textlist->prev->next;
			    textlist->prev->next = textlist;
			}
		    } else
			break;
		}
		free (freeme);
		rewrap = false;
	    }
	    // Skip to the first visible line
	    unsigned ientry = 0;
	    const struct scrolltext* l = first_line;
	    while (ientry < linenumber && l) {
		++ientry;
		l = l->next;
	    }
	    // We sould now have the linked list setup'ed... hopefully.
	    for (unsigned y = ydesc; y <= LINES - 3u && l; ++ientry, ++y, l = l->next)
		mvadd_utf8 (y, xdesc, l->line);
	}

	char keyinfostr [256];
	if (current_item->data->link)
	    snprintf (keyinfostr, sizeof (keyinfostr), "-> %s", current_item->data->link);
	else
	    snprintf (keyinfostr, sizeof (keyinfostr), _("Press '%c' or Enter to return to previous screen. Hit '%c' for help screen."), _settings.keybindings.prevmenu, _settings.keybindings.help);
	UIStatus (keyinfostr, 0, 0);

	int uiinput = getch();
	if (uiinput == _settings.keybindings.help || uiinput == '?')
	    UIDisplayItemHelp();
	else if (uiinput == '\n' || uiinput == _settings.keybindings.prevmenu || uiinput == _settings.keybindings.enter) {
	    // Free the wrapped text linked list.
	    while (first_line) {
		struct scrolltext* l = first_line;
		first_line = first_line->next;
		free (l->line);
		free (l);
	    }
	    return;
	} else if (uiinput == _settings.keybindings.urljump)
	    UISupportURLJump (current_item->data->link);
	else if (uiinput == _settings.keybindings.next || uiinput == KEY_RIGHT) {
	    if (current_item->next != NULL) {
		current_item = current_item->next;
		linenumber = 0;
		rewrap = true;
		maxlines = 0;
	    } else {
		// Setting rewrap to 1 to get the free block below executed.
		rewrap = true;
		uiinput = ungetch (_settings.keybindings.prevmenu);
	    }
	} else if (uiinput == _settings.keybindings.prev || uiinput == KEY_LEFT) {
	    if (current_item->prev != NULL) {
		current_item = current_item->prev;
		linenumber = 0;
		rewrap = true;
		maxlines = 0;
	    } else {
		// Setting rewrap to 1 to get the free block below executed.
		rewrap = true;
		uiinput = ungetch (_settings.keybindings.prevmenu);
	    }
	} else if (uiinput == KEY_NPAGE || uiinput == ' ' || uiinput == _settings.keybindings.pdown) {
	    // Scroll by one page.
	    for (int i = 0; i < LINES - 9; ++i)
		if (linenumber + (LINES - 7) < maxlines)
		    ++linenumber;
	} else if (uiinput == KEY_PPAGE || uiinput == _settings.keybindings.pup) {
	    for (int i = 0; i < LINES - 9; ++i)
		if (linenumber >= 1)
		    --linenumber;
	} else if (uiinput == KEY_UP && linenumber >= 1)
	    --linenumber;
	else if (uiinput == KEY_DOWN && linenumber + (LINES - 7) < maxlines)
	    ++linenumber;
	else if (resize_dirty || uiinput == KEY_RESIZE) {
	    rewrap = true;
	    // Reset maxlines, otherwise the program will scroll to far down.
	    maxlines = 0;

	    endwin();
	    refresh();
	    resize_dirty = false;
	} else if (uiinput == _settings.keybindings.about)
	    UIAbout();
	// Redraw screen on ^L
	else if (uiinput == 12)
	    clear();

	// Free the linked list structure if we need to rewrap the text block.
	if (rewrap) {
	    // Free the wrapped text linked list.
	    while (first_line) {
		struct scrolltext* l = first_line;
		first_line = first_line->next;
		free (l->line);
		free (l);
	    }
	}
    }
}

static int UIDisplayFeed (struct feed* current_feed)
{
    // Set highlighted to _feed_list at the beginning.
    // Otherwise bad things (tm) will happen.
    const struct newsitem* first_scr_ptr = current_feed->items;
    const struct newsitem* tmp_first = first_scr_ptr;

    // Select first unread item if we enter feed view or
    // leave first item active if there is no unread.
    const struct newsitem* highlighted = current_feed->items;
    const struct newsitem* tmp_highlighted = highlighted;
    unsigned highlightline = LINES - 1;

    // Moves highlight to next unread item.
    unsigned highlightnum = 1;	// Index of the highlighted item, 1 = first visible
    while (highlighted && highlighted->next && highlighted->data->readstatus) {
	highlighted = highlighted->next;
	if (++highlightnum > LINES - 4u) {
	    --highlightnum;
	    if (first_scr_ptr->next)
		first_scr_ptr = first_scr_ptr->next;
	}
    }
    // If *highlighted points to a read entry now we have hit the last
    // entry and there are no unread items anymore. Restore the original
    // location in this case.
    // Check if we have no items at all!
    if (highlighted && highlighted->data->readstatus) {
	highlighted = tmp_highlighted;
	first_scr_ptr = tmp_first;
    }
    // Typeahead enabled?
    bool typeahead = false;
    unsigned typeaheadskip = 0;	// # of lines to skip (for typeahead)
    char* searchstr = NULL;	// Typeahead searchstr string
    unsigned searchstrlen = 0;

    // Save first starting position. For typeahead.
    const struct newsitem* savestart = first_scr_ptr;	// Typeahead internal usage (tmp position save)
    const struct newsitem* savestart_first = NULL;

    // Put all categories of the current feed into a comma seperated list.
    char* categories = GetCategoryList (current_feed);

    bool reloaded = false;	// We need to signal the main function if we changed feed contents.
    while (1) {
	erase();

	// Get a title or something
	const char* title = current_feed->description;
	if (!title)
	    title = current_feed->title;
	if (!title)
	    title = "Untitled";

	// Convert title to current locale
	char* dejunked_title = UIDejunk (title);
	if (!dejunked_title)
	    dejunked_title = strdup (title);
	char* converted_title = iconvert (dejunked_title);
	if (!converted_title)
	    converted_title = strdup (dejunked_title);
	free (dejunked_title);

	// Print title
	UISupportDrawHeader (converted_title);
	free (converted_title);

	// We start the item list at line 3.
	unsigned ypos = 2, itemnum = 1;
	const unsigned ymax = LINES - 4;

	if (typeahead) {
	    // This resets the offset for every typeahead loop.
	    first_scr_ptr = current_feed->items;

	    unsigned count = 0, skipper = 0;	// # of lines already skipped
	    bool found = false;
	    for (const struct newsitem * i = current_feed->items; i; ++count, i = i->next) {
		// count+1 > ymax:
		// If the _next_ line would go over the boundary.
		if (count + 1 > ymax)
		    if (first_scr_ptr->next)
			first_scr_ptr = first_scr_ptr->next;
		if (!searchstrlen)
		    continue;
		// Substring match.
		if (i->data->title && s_strcasestr (i->data->title, searchstr)) {
		    found = true;
		    highlighted = i;
		    if (typeaheadskip >= 1 && skipper != typeaheadskip) {
			++skipper;
			continue;
		    }
		    break;
		}
	    }
	    if (!found) {
		// Restore original position on no match.
		highlighted = savestart;
		first_scr_ptr = savestart_first;
	    }
	}

	// Print unread entries in bold.
	const struct newsitem* current_item = NULL;
	for (const struct newsitem* item = first_scr_ptr; item; item = item->next) {
	    // Set cursor to start of current line and clear it.
	    move (ypos, 0);
	    clrtoeol();

	    if (!item->data->readstatus) {
		// Apply color style.
		if (!_settings.monochrome) {
		    attron (COLOR_PAIR (2));
		    if (_settings.color.newitemsbold)
			attron (WA_BOLD);
		} else
		    attron (WA_BOLD);
	    }

	    if (item == highlighted) {
		current_item = item;
		highlightline = ypos;
		highlightnum = itemnum;
		attron (WA_REVERSE);
		mvhline (ypos, 0, ' ', COLS);
	    }
	    // Check for empty <title>
	    if (item->data->title) {
		char* newtext;
		if (current_feed->smartfeed == 1) {
		    char tmpstr[512];
		    snprintf (tmpstr, sizeof (tmpstr), "(%s) %s", item->data->parent->title, item->data->title);
		    newtext = UIDejunk (tmpstr);
		} else
		    newtext = UIDejunk (item->data->title);

		char* converted = iconvert (newtext);
		if (converted == NULL)
		    converted = strdup (newtext);
		free (newtext);

		int columns = COLS - 6;	// Cut max item length.
		mvaddn_utf8 (ypos, 1, converted, columns);
		if (xmlStrlen ((xmlChar*) converted) > columns)
		    mvaddstr (ypos, columns + 1, "...");

		free (converted);
	    } else {
		if (current_feed->smartfeed == 1) {
		    char tmpstr[256];
		    snprintf (tmpstr, sizeof (tmpstr), "(%s) %s", item->data->parent->title, _("No title"));
		    mvadd_utf8 (ypos, 1, tmpstr);
		} else
		    mvadd_utf8 (ypos, 1, _("No title"));
	    }

	    ++ypos;
	    if (item == highlighted)
		attroff (WA_REVERSE);
	    if (!item->data->readstatus) {
		// Disable color style.
		if (!_settings.monochrome) {
		    attroff (COLOR_PAIR (2));
		    if (_settings.color.newitemsbold)
			attroff (WA_BOLD);
		} else
		    attroff (WA_BOLD);
	    }

	    if (itemnum >= ymax)
		break;
	    ++itemnum;
	}

	char tmpstr [256];
	if (typeahead)
	    snprintf (tmpstr, sizeof (tmpstr), "-> %s", searchstr);
	else if (current_item && current_item->data->link)
	    snprintf (tmpstr, sizeof (tmpstr), "-> %s", current_item->data->link);
	else
	    snprintf (tmpstr, sizeof (tmpstr), _("Press '%c' to return to main menu, '%c' to show help."), _settings.keybindings.prevmenu, _settings.keybindings.help);
	UIStatus (tmpstr, 0, 0);

	move (highlightline, 0);
	int uiinput = getch();

	time_t curtime = time (NULL);
	if (typeahead) {
	    // Only match real characters.
	    if (uiinput >= ' ' && uiinput <= '~') {
		searchstr[searchstrlen] = uiinput;
		searchstr[searchstrlen + 1] = 0;
		++searchstrlen;
		searchstr = realloc (searchstr, searchstrlen + 2);
		// ASCII 127 is DEL, 263 is... actually I have
		// no idea, but it's what the text console returns.
	    } else if (uiinput == 127 || uiinput == 263) {
		// Don't let searchstrlen go below 0!
		if (searchstrlen > 0) {
		    --searchstrlen;
		    searchstr[searchstrlen] = 0;
		    searchstr = realloc (searchstr, searchstrlen + 2);
		} else {
		    typeahead = false;
		    free (searchstr);
		}
	    }
	} else {
	    if (uiinput == _settings.keybindings.help || uiinput == '?')
		UIDisplayFeedHelp();
	    else if (uiinput == _settings.keybindings.prevmenu) {
		free (categories);
		return reloaded;
	    } else if ((uiinput == KEY_UP || uiinput == _settings.keybindings.prev) && highlighted && highlighted->prev) {
		// Check if we have no items at all!
		highlighted = highlighted->prev;
		// Adjust first visible entry.
		if (--highlightnum < 1 && first_scr_ptr->prev) {
		    ++highlightnum;
		    first_scr_ptr = first_scr_ptr->prev;
		}
	    } else if ((uiinput == KEY_DOWN || uiinput == _settings.keybindings.next) && highlighted && highlighted->next) {
		highlighted = highlighted->next;
		// Adjust first visible entry.
		if (++highlightnum > ymax && first_scr_ptr->next) {
		    --highlightnum;
		    first_scr_ptr = first_scr_ptr->next;
		}
	    } else if ((uiinput == KEY_NPAGE || uiinput == ' ' || uiinput == _settings.keybindings.pdown) && highlighted) {
		// Move highlight one page up/down == ymax
		for (unsigned i = 0; i < ymax && highlighted->next; ++i) {
		    highlighted = highlighted->next;
		    if (++highlightnum > ymax && first_scr_ptr->next) {
			--highlightnum;
			first_scr_ptr = first_scr_ptr->next;
		    }
		}
	    } else if ((uiinput == KEY_PPAGE || uiinput == _settings.keybindings.pup) && highlighted) {
		for (unsigned i = 0; i < ymax && highlighted->prev; ++i) {
		    highlighted = highlighted->prev;
		    if (--highlightnum < 1 && first_scr_ptr->prev) {
			++highlightnum;
			first_scr_ptr = first_scr_ptr->prev;
		    }
		}
	    } else if (uiinput == KEY_HOME || uiinput == _settings.keybindings.home) {
		highlighted = first_scr_ptr = current_feed->items;
		highlightnum = 0;
	    } else if (uiinput == KEY_END || uiinput == _settings.keybindings.end) {
		highlighted = first_scr_ptr = current_feed->items;
		highlightnum = 0;
		while (highlighted && highlighted->next) {
		    highlighted = highlighted->next;
		    if (++highlightnum >= ymax && first_scr_ptr->next) {
			--highlightnum;
			first_scr_ptr = first_scr_ptr->next;
		    }
		}
	    } else if (uiinput == _settings.keybindings.reload || uiinput == _settings.keybindings.forcereload) {
		if (_unfiltered_feed_list) {
		    UIStatus (_("Please deactivate the category filter before using this function."), 2, 0);
		    continue;
		}
		if (current_feed->smartfeed == 1)
		    continue;

		if (uiinput == _settings.keybindings.forcereload) {
		    free (current_feed->lasterror);
		    current_feed->lasterror = NULL;
		}

		UpdateFeed (current_feed);
		highlighted = current_feed->items;
		// Reset first_scr_ptr if reloading.
		first_scr_ptr = current_feed->items;
		reloaded = true;
	    } else if (uiinput == _settings.keybindings.urljump)
		UISupportURLJump (current_feed->link);
	    else if (uiinput == _settings.keybindings.urljump2 && highlighted)
		UISupportURLJump (highlighted->data->link);
	    else if (uiinput == _settings.keybindings.markread) {	// Mark everything read.
		for (struct newsitem* i = current_feed->items; i; i = i->next) {
		    if (!i->data->readstatus) {
			i->data->readstatus = true;
			current_feed->mtime = curtime;
		    }
		}
	    } else if (uiinput == _settings.keybindings.markunread && highlighted) {
		highlighted->data->readstatus = !highlighted->data->readstatus;
		current_feed->mtime = curtime;
		reloaded = true;
	    } else if (uiinput == _settings.keybindings.about)
		UIAbout();
	    else if (uiinput == _settings.keybindings.feedinfo)
		FeedInfo (current_feed);
	    else if (resize_dirty || uiinput == KEY_RESIZE) {
		clear();
		resize_dirty = false;
	    } else if (uiinput == 12)	// Redraw screen on ^L
		clear();
	}
	if (uiinput == '\n' || (uiinput == _settings.keybindings.enter && !typeahead)) {
	    // If typeahead is active clear it's state and free the structure.
	    if (typeahead) {
		free (searchstr);
		typeahead = false;
		if (!_settings.cursor_always_visible)
		    curs_set (0);
		typeaheadskip = 0;
	    }
	    // Check if we have no items at all!
	    // Don't even try to view a non existant item.
	    if (highlighted) {
		UIDisplayItem (highlighted, current_feed);
		if (!highlighted->data->readstatus) {
		    highlighted->data->readstatus = true;
		    current_feed->mtime = curtime;
		}
		tmp_highlighted = highlighted;
		tmp_first = first_scr_ptr;

		// highlighted = first_scr_ptr;
		// Moves highlight to next unread item.
		while (highlighted->next && highlighted->data->readstatus) {
		    highlighted = highlighted->next;
		    if (++highlightnum > ymax && first_scr_ptr->next) {
			--highlightnum;
			first_scr_ptr = first_scr_ptr->next;
		    }
		}

		// If *highlighted points to a read entry now we have hit the last
		// entry and there are no unread items anymore. Restore the original
		// location in this case.
		if (highlighted->data->readstatus) {
		    highlighted = tmp_highlighted;
		    first_scr_ptr = tmp_first;
		}
	    }
	}
	// TAB key is decimal 9.
	if (uiinput == 9 || uiinput == _settings.keybindings.typeahead) {
	    if (typeahead) {
		if (searchstrlen == 0) {
		    typeahead = false;
		    // Typeahead now off.
		    if (!_settings.cursor_always_visible)
			curs_set (0);
		    free (searchstr);
		    typeaheadskip = 0;
		    highlighted = savestart;
		    first_scr_ptr = savestart_first;
		} else {
		    unsigned found = 0;
		    for (const struct newsitem * i = current_feed->items; i; i = i->next)
			if (i->data->title && s_strcasestr (i->data->title, searchstr))
			    ++found;	// Substring match.
		    if (typeaheadskip == found - 1)
			typeaheadskip = 0;
		    else if (found > 1)
			++typeaheadskip;	// If more than one match was found and user presses tab we will skip matches.
		}
	    } else {
		typeahead = true;
		// Typeahead now on.
		curs_set (1);
		searchstr = malloc (2);
		memset (searchstr, 0, 2);
		searchstrlen = 0;
		// Save all start positions.
		savestart = highlighted;
		savestart_first = first_scr_ptr;
	    }
	}
	// ctrl+g clears typeahead.
	if (uiinput == 7) {
	    // But only if it was switched on previously.
	    if (typeahead) {
		typeahead = false;
		free (searchstr);
		typeaheadskip = 0;
		highlighted = savestart;
		first_scr_ptr = savestart_first;
	    }
	}
	// ctrl+u clears line.
	if (uiinput == 21) {
	    searchstrlen = 0;
	    searchstr = realloc (searchstr, 2);
	    memset (searchstr, 0, 2);
	}
    }
}

void UIMainInterface (void)
{
    struct feed* first_scr_ptr = _feed_list;	// First pointer on top of screen. Used for scrolling.
    struct feed* savestart = first_scr_ptr;

    struct feed* savestart_first = NULL;
    struct feed* highlighted = _feed_list;
    unsigned highlightnum = 1;
    unsigned highlightline = LINES - 1;	// Line with current selected item cursor
    // will be moved to this line to have it in
    // the same line as the highlight. Visually
    // impaired users with screen readers seem
    // to need this.

    bool typeahead = false;
    unsigned typeaheadskip = 0;	// Number of typeahead matches to skip.
    char* search = NULL;	// Typeahead search string.
    unsigned searchlen = 0;	// " length.

    char* filters[8] = { };    // Category filters. Must be NULL when not used!
    unsigned numfilters = 0;	// Number of active filters.
    bool filteractivated = false;
    bool update_smartfeeds = true;
    bool andxor = false;	// Toggle for AND/OR combinations of filters.

    while (1) {
	if (update_smartfeeds) {
	    // This only needs to be done if new items are added, old removed.
	    // Reload, add, delete.
	    SmartFeedsUpdate();
	    update_smartfeeds = false;
	}
	erase();

	char* filterstring = NULL;
	if (filters[0]) {
	    numfilters = 0;
	    size_t len = 0;
	    for (unsigned i = 0; i < sizeof(filters)/sizeof(filters[0]) && filters[i]; ++i, ++numfilters)
		len += strlen (filters[i]) + strlen (", ");
	    filterstring = calloc (len, 1);
	    for (unsigned i = 0; i < numfilters; ++i) {
		if (i)
		    strcat (filterstring, ", ");
		strcat (filterstring, filters[i]);
	    }
	}
	UISupportDrawHeader (filterstring);
	if (filterstring)
	    free (filterstring);
	filterstring = NULL;

	// If a filter is defined we need to make copy of the pointers in
	// struct feed and work on that.
	// Build a new list only with items matching current filter.
	//
	// Never EVER set filteractivated=true if there is no filter defined!
	// This should be moved to its own function in ui-support.c!
	if (filteractivated) {
	    if (!_unfiltered_feed_list)
		_unfiltered_feed_list = _feed_list;
	    _feed_list = NULL;
	    for (const struct feed * cur_ptr = _unfiltered_feed_list; cur_ptr; cur_ptr = cur_ptr->next) {
		unsigned found = 0;
		if (!cur_ptr->feedcategories)
		    continue;
		for (const struct feedcategories * c = cur_ptr->feedcategories; c; c = c->next)
		    for (unsigned i = 0; i < 8 && filters[i]; ++i)
			if (strcasecmp (c->name, filters[i]) == 0)	// Match found
			    ++found;
		if (!found)
		    continue;

		bool addfilteritem = false;
		if (!andxor) { // AND
		    if (found == numfilters)
			addfilteritem = true;
		} else if (found)	// OR
		    addfilteritem = true;

		if (addfilteritem) {
		    struct feed* new_feed = newFeedStruct();
		    // These structs will get leaked!
		    // Yeah, yeah, it is on my todo list...
		    // Though valgrind doesn't report them as leaked.

		    // Duplicate pointers
		    new_feed->feedurl = cur_ptr->feedurl;
		    new_feed->xmltext = cur_ptr->xmltext;
		    new_feed->title = cur_ptr->title;
		    new_feed->link = cur_ptr->link;
		    new_feed->description = cur_ptr->description;
		    new_feed->lasterror = cur_ptr->lasterror;
		    new_feed->items = cur_ptr->items;
		    new_feed->problem = cur_ptr->problem;
		    new_feed->custom_title = cur_ptr->custom_title;
		    new_feed->original = cur_ptr->original;
		    new_feed->perfeedfilter = cur_ptr->perfeedfilter;
		    new_feed->execurl = cur_ptr->execurl;
		    new_feed->feedcategories = cur_ptr->feedcategories;

		    // Add to bottom of pointer chain.
		    struct feed** prev_feed = &_feed_list;
		    while (*prev_feed && (*prev_feed)->next)
			*prev_feed = (*prev_feed)->next;
		    new_feed->prev = *prev_feed;
		    (*prev_feed)->next = new_feed;
		}
	    }
	    first_scr_ptr = _feed_list;
	    highlighted = first_scr_ptr;
	    filteractivated = false;
	}

	unsigned ypos = 2, itemnum = 1;

	if (typeahead) {
	    // This resets the offset for every typeahead loop.
	    first_scr_ptr = _feed_list;

	    unsigned count = 0, skipper = 0;
	    bool found = false;
	    for (struct feed * cur_ptr = _feed_list; cur_ptr; ++count, cur_ptr = cur_ptr->next) {
		// count+1 >= Lines-4: if the _next_ line would go over the boundary.
		if (count + 1 > LINES - 4u && first_scr_ptr->next)
		    first_scr_ptr = first_scr_ptr->next;
		// Exact match from beginning of line.
		if (searchlen > 0 && s_strcasestr (cur_ptr->title, search)) {
		    found = true;
		    highlighted = cur_ptr;
		    if (typeaheadskip >= 1 && skipper != typeaheadskip)
			++skipper;
		    break;
		}
	    }
	    if (!found) {
		// Restore original position on no match.
		highlighted = savestart;
		first_scr_ptr = savestart_first;
	    }
	}

	for (struct feed * cur_ptr = first_scr_ptr; cur_ptr; cur_ptr = cur_ptr->next) {
	    // Set cursor to start of current line and clear it.
	    move (ypos, 0);
	    clrtoeol();

	    // Determine number of new items in feed.
	    unsigned newcount = 0;
	    for (const struct newsitem * i = cur_ptr->items; i; i = i->next)
		if (!i->data->readstatus)
		    ++newcount;

	    // Make highlight if we are the highlighted feed
	    if (cur_ptr == highlighted) {
		highlightline = ypos;
		highlightnum = itemnum;
		attron (WA_REVERSE);
		mvhline (ypos, 0, ' ', COLS);
	    }

	    int columns;
	    if (cur_ptr->feedcategories != NULL)
		columns = COLS - 26 - strlen (_("new"));
	    else
		columns = COLS - 9 - strlen (_("new"));

	    mvaddn_utf8 (ypos, 1, cur_ptr->title, columns);
	    if (xmlStrlen ((xmlChar*) cur_ptr->title) > columns)
		mvaddstr (ypos, columns + 1, "...");

	    if (cur_ptr->problem)
		mvaddch (ypos, 0, '!');

	    // Execute this _after_ the for loop. Otherwise it'll suck CPU like crazy!
	    if (newcount) {
		const char* localized_msg = ngettext ("%3u new", "%3u new", newcount);
		char msgbuf[16];
		snprintf (msgbuf, sizeof (msgbuf), localized_msg, newcount);
		mvadd_utf8 (ypos, COLS - 1 - strlen (localized_msg), msgbuf);
	    }

	    if (cur_ptr->feedcategories != NULL)
		mvaddn_utf8 (ypos, COLS - 21 - strlen (_("new")), cur_ptr->feedcategories->name, 15);

	    ++ypos;

	    if (cur_ptr == highlighted)
		attroff (WA_REVERSE);

	    if (itemnum >= LINES - 4u)
		break;
	    ++itemnum;
	}

	if (typeahead) {
	    char msgbuf[128];
	    snprintf (msgbuf, sizeof (msgbuf), "-> %s", search);
	    UIStatus (msgbuf, 0, 0);
	} else {
	    char msgbuf[128];
	    snprintf (msgbuf, sizeof (msgbuf), _("Press '%c' for help window."), _settings.keybindings.help);
	    if (easterEgg())
		snprintf (msgbuf, sizeof (msgbuf), _("Press '%c' for help window. (Press '%c' to play Santa Hunta!)"), _settings.keybindings.help, _settings.keybindings.about);
	    UIStatus (msgbuf, 0, 0);
	}

	move (highlightline, 0);
	int uiinput = getch();

	if (typeahead) {
	    // Only match real characters.
	    if (uiinput >= ' ' && uiinput <= '~') {
		search[searchlen] = uiinput;
		search[++searchlen] = 0;
		search = realloc (search, searchlen + 1);
	    } else if (uiinput == 127 || uiinput == 263) {
		// Don't let searchlen go below 0!
		if (searchlen > 0)
		    search[--searchlen] = 0;
		else {
		    typeahead = false;
		    free (search);
		}
	    }
	} else {
	    if (uiinput == _settings.keybindings.quit) {
		// Restore original _feed_list if filter is defined!
		if (filters[0])
		    _feed_list = _unfiltered_feed_list;
		MainQuit (NULL, NULL);
	    } else if (uiinput == _settings.keybindings.reload || uiinput == _settings.keybindings.forcereload) {
		if (filters[0])
		    UIStatus (_("Please deactivate the category filter before using this function."), 2, 0);
		else {
		    if (highlighted && uiinput == _settings.keybindings.forcereload) {
			free (highlighted->lasterror);
			highlighted->lasterror = NULL;
		    }
		    UpdateFeed (highlighted);
		    update_smartfeeds = true;
		}
	    } else if (uiinput == _settings.keybindings.reloadall) {
		if (filters[0])
		    UIStatus (_("Please deactivate the category filter before using this function."), 2, 0);
		else {
		    UpdateAllFeeds();
		    update_smartfeeds = true;
		}
	    } else if (uiinput == _settings.keybindings.addfeed || uiinput == _settings.keybindings.newheadlines) {
		if (filters[0])
		    UIStatus (_("Please deactivate the category filter before using this function."), 2, 0);
		else if (uiinput == _settings.keybindings.addfeed) {
		    switch (UIAddFeed (NULL)) {
			case 0:
			    UIStatus (_("Successfully added new item..."), 1, 0);
			    _feed_list_changed = true;
			    break;
			case 2:
			    UIStatus (_("Invalid URL! Please add http:// if you forgot this."), 2, 1);
			    break;
			case -1:
			    UIStatus (_("There was a problem adding the feed!"), 2, 1);
			    break;
			default:
			    break;
		    }
		} else if (!SmartFeedExists ("newitems"))
		    UIAddFeed ("smartfeed:/newitems");

		// Scroll to top of screen and redraw everything.
		highlighted = _feed_list;
		first_scr_ptr = _feed_list;
		update_smartfeeds = true;
	    } else if (uiinput == _settings.keybindings.help || uiinput == '?')
		UIHelpScreen();
	    else if (uiinput == _settings.keybindings.deletefeed) {
		// This should be moved to its own function in ui-support.c!
		if (filters[0]) {
		    UIStatus (_("Please deactivate the category filter before using this function."), 2, 0);
		    continue;
		}
		// Move this code into its own function!

		// If the deleted feed was the last one of a specific category,
		// remove this category from the global list.
		if (highlighted) {
		    if (UIDeleteFeed (highlighted->title) == 1) {
			// Do it!
			struct feed* removed = highlighted;
			// Save current highlight position.
			struct feed* saved_highlighted = highlighted;

			// Remove cachefile from filesystem.
			char* hashme = Hashify (highlighted->feedurl);
			char cachefilename[PATH_MAX];
			CacheFilePath (hashme, cachefilename, sizeof(cachefilename));
			free (hashme);

			// Errors from unlink can be ignored. Worst thing that happens is that
			// we delete a file that doesn't exist.
			unlink (cachefilename);

			// Unlink pointer from chain.
			if (highlighted == _feed_list) {
			    // first element
			    if (_feed_list->next != NULL) {
				_feed_list = _feed_list->next;
				first_scr_ptr = _feed_list;
				highlighted->next->prev = NULL;
			    } else {
				_feed_list = NULL;
				first_scr_ptr = NULL;
			    }
			    // Set new highlighted to _feed_list again.
			    saved_highlighted = _feed_list;
			} else if (highlighted->next == NULL) {
			    // last element
			    // Set new highlighted to element before deleted one.
			    saved_highlighted = highlighted->prev;
			    // If highlighted was first line move first line upward in pointer chain.
			    if (highlighted == first_scr_ptr)
				first_scr_ptr = first_scr_ptr->prev;

			    highlighted->prev->next = NULL;
			} else {
			    // element inside list */
			    // Set new highlighted to element after deleted one.
			    saved_highlighted = highlighted->next;
			    // If highlighted was last line, move first line downward in pointer chain.
			    if (highlighted == first_scr_ptr)
				first_scr_ptr = first_scr_ptr->next;

			    highlighted->prev->next = highlighted->next;
			    highlighted->next->prev = highlighted->prev;
			}
			// Put highlight to new highlight position.
			highlighted = saved_highlighted;

			// free (removed) pointer
			if (removed->smartfeed == 0) {
			    if (removed->items) {
				while (removed->items->next) {
				    removed->items = removed->items->next;
				    free (removed->items->prev->data->title);
				    free (removed->items->prev->data->link);
				    free (removed->items->prev->data->description);
				    free (removed->items->prev);
				}
				free (removed->items->data->title);
				free (removed->items->data->link);
				free (removed->items->data->description);
				free (removed->items);
			    }
			    free (removed->feedurl);
			    free (removed->xmltext);
			    removed->xmltext = NULL;
			    removed->content_length = 0;
			    free (removed->title);
			    free (removed->link);
			    free (removed->description);
			    free (removed->lasterror);
			    free (removed->custom_title);
			    free (removed->original);
			    free (removed);
			    _feed_list_changed = true;
			}
			update_smartfeeds = true;
		    }
		}
	    } else if ((uiinput == KEY_UP || uiinput == _settings.keybindings.prev) && highlighted && highlighted->prev) {
		highlighted = highlighted->prev;
		if (--highlightnum < 1 && first_scr_ptr->prev) {	// Reached first onscreen entry.
		    ++highlightnum;
		    first_scr_ptr = first_scr_ptr->prev;
		}
	    } else if ((uiinput == KEY_DOWN || uiinput == _settings.keybindings.next) && highlighted && highlighted->next) {
		highlighted = highlighted->next;
		if (++highlightnum >= LINES - 5u && first_scr_ptr->next) {	// If we fall off the screen, advance first_scr_ptr to next entry.
		    --highlightnum;
		    first_scr_ptr = first_scr_ptr->next;
		}
	    } else if (uiinput == KEY_NPAGE || uiinput == ' ' || uiinput == _settings.keybindings.pdown) {
		// Move highlight one page up/down == LINES-6
		for (unsigned i = 0; i < LINES - 6u && highlighted->next; ++i) {
		    highlighted = highlighted->next;
		    if (++highlightnum > LINES - 6u && first_scr_ptr->next) {
			--highlightnum;
			first_scr_ptr = first_scr_ptr->next;
		    }
		}
	    } else if (uiinput == KEY_PPAGE || uiinput == _settings.keybindings.pup) {
		for (unsigned i = 0; i < LINES - 6u && highlighted->prev; ++i) {
		    highlighted = highlighted->prev;
		    if (--highlightnum < 1 && first_scr_ptr->prev) {
			++highlightnum;
			first_scr_ptr = first_scr_ptr->prev;
		    }
		}
	    } else if (uiinput == KEY_HOME || uiinput == _settings.keybindings.home) {
		highlighted = first_scr_ptr = _feed_list;
		highlightnum = 0;
	    } else if (uiinput == KEY_END || uiinput == _settings.keybindings.end) {
		highlighted = first_scr_ptr = _feed_list;
		highlightnum = 0;
		while (highlighted && highlighted->next) {
		    highlighted = highlighted->next;
		    if (++highlightnum >= LINES - 6u && first_scr_ptr->next) {
			--highlightnum;
			first_scr_ptr = first_scr_ptr->next;
		    }
		}
	    } else if (uiinput == _settings.keybindings.moveup) {
		// This function is deactivated when a filter is active.
		if (filters[0] != NULL) {
		    UIStatus (_("You cannot move items while a category filter is defined!"), 2, 0);
		    continue;
		}
		// Move item up.
		if (highlighted) {
		    if (highlighted->prev != NULL) {
			SwapPointers (highlighted, highlighted->prev);
			highlighted = highlighted->prev;
			if (highlightnum - 1 < 1 && first_scr_ptr->prev)
			    first_scr_ptr = first_scr_ptr->prev;
			_feed_list_changed = true;
		    }
		    update_smartfeeds = true;
		}
	    } else if (uiinput == _settings.keybindings.movedown) {
		// This function is deactivated when a filter is active.
		if (filters[0]) {
		    UIStatus (_("You cannot move items while a category filter is defined!"), 2, 0);
		    continue;
		}
		// Move item down.
		if (highlighted) {
		    if (highlighted->next) {
			SwapPointers (highlighted, highlighted->next);
			highlighted = highlighted->next;
			if (++highlightnum >= LINES - 6u && first_scr_ptr->next) {
			    --highlightnum;
			    first_scr_ptr = first_scr_ptr->next;
			}
			_feed_list_changed = true;
		    }
		    update_smartfeeds = true;
		}
	    } else if (uiinput == _settings.keybindings.dfltbrowser) {
		UIChangeBrowser();
		SaveBrowserSetting();
	    } else if (uiinput == _settings.keybindings.markallread) {
		// This function is safe for using in filter mode, because it only
		// changes int values. It automatically marks the correct ones read
		// if a filter is applied since we are using a copy of the main data.
		time_t curtime = time (NULL);
		for (struct feed * f = _feed_list; f; f = f->next) {
		    for (struct newsitem * i = f->items; i; i = i->next) {
			if (!i->data->readstatus) {
			    i->data->readstatus = true;
			    f->mtime = curtime;
			}
		    }
		}
	    } else if (uiinput == _settings.keybindings.about)
		UIAbout();
	    else if (uiinput == _settings.keybindings.changefeedname && highlighted) {
		if (filters[0])	// This needs to be worked on before it works while a filter is applied!
		    UIStatus (_("Please deactivate the category filter before using this function."), 2, 0);
		else {
		    UIChangeFeedName (highlighted);
		    _feed_list_changed = true;
		    update_smartfeeds = true;
		}
	    } else if (uiinput == _settings.keybindings.perfeedfilter && highlighted) {
		if (filters[0])
		    UIStatus (_("Please deactivate the category filter before using this function."), 2, 0);
		else {
		    UIPerFeedFilter (highlighted);
		    _feed_list_changed = true;
		}
	    } else if (uiinput == _settings.keybindings.sortfeeds && highlighted) {
		if (filters[0])	// Deactivate sorting function if filter is applied.
		    UIStatus (_("Please deactivate the category filter before using this function."), 2, 0);
		else {
		    SnowSort();
		    _feed_list_changed = true;
		    update_smartfeeds = true;
		}
	    } else if (uiinput == _settings.keybindings.categorize && highlighted && !highlighted->smartfeed) {
		if (filters[0])	// This needs to be worked on before it works while a filter is applied!
		    UIStatus (_("Please deactivate the category filter before using this function."), 2, 0);
		else {
		    CategorizeFeed (highlighted);
		    _feed_list_changed = true;
		}
	    } else if (uiinput == _settings.keybindings.filter) {
		// GUI to set a filter
		char* catfilter = DialogGetCategoryFilter();
		if (catfilter) {
		    unsigned iunusedfilter = 0;
		    while (iunusedfilter < 8 && filters[iunusedfilter])
			++iunusedfilter;
		    if (iunusedfilter < 8) {
			filters[iunusedfilter] = strdup (catfilter);
			filteractivated = true;
		    }
		} else {
		    ResetFilters (filters);

		    // If DialogGetCategoryFilter() was used to switch off filter
		    // Restore _feed_list here.
		    if (_unfiltered_feed_list) {
			// Restore _feed_list
			_feed_list = _unfiltered_feed_list;
			first_scr_ptr = _feed_list;
			highlighted = _feed_list;
			_unfiltered_feed_list = NULL;
		    }
		}
		free (catfilter);
		_feed_list_changed = true;
	    } else if (uiinput == _settings.keybindings.filtercurrent) {
		// Set filter to primary filter of this feed.
		// Free filter if it's not empty to avoid memory leaks!
		if (filters[0]) {
		    ResetFilters (filters);

		    // Set new filter to primary category.
		    // highlighted can be null if there is no feed highlighted.
		    // First start, no feeds ever added, multiple filters applied that no feed matches.
		    if (highlighted && highlighted->feedcategories) {
			filters[0] = strdup (highlighted->feedcategories->name);
			filteractivated = true;
		    }
		    // Restore _feed_list
		    _feed_list = _unfiltered_feed_list;
		    first_scr_ptr = _feed_list;
		    highlighted = _feed_list;
		    _unfiltered_feed_list = NULL;
		} else if (highlighted && highlighted->feedcategories) {
		    filters[0] = strdup (highlighted->feedcategories->name);
		    filteractivated = true;
		}
	    } else if (uiinput == _settings.keybindings.nofilter) {
		// Remove all filters.
		if (filters[0]) {
		    ResetFilters (filters);

		    // Restore _feed_list
		    _feed_list = _unfiltered_feed_list;
		    first_scr_ptr = _feed_list;
		    highlighted = _feed_list;
		    _unfiltered_feed_list = NULL;
		}
	    } else if (uiinput == 'X' && filters[0]) {	// AND or OR matching for the filer.
		andxor = !andxor;
		filteractivated = true;	// Reconstruct filter.
	    } else if (resize_dirty || uiinput == KEY_RESIZE) {
		clear();
		resize_dirty = false;
	    } else if (uiinput == 12)	// Redraw screen on ^L
		clear();
	}
	if (uiinput == '\n' || (uiinput == _settings.keybindings.enter && !typeahead)) {
	    // If typeahead is active clear it's state and free the structure.
	    if (typeahead) {
		free (search);
		typeahead = false;
		// Clear typeaheadskip flag, otherwise type ahead breaks, because we
		// have a typeaheadskip "offset" when using it again.
		typeaheadskip = 0;
	    }
	    // Select this feed, open and view entries.
	    // If contents of the feed was changed during display (reload),
	    // regenerate the smartfeed.
	    if (highlighted && UIDisplayFeed (highlighted))
		update_smartfeeds = true;

	    // Clear screen after we return from here.
	    erase();
	}
	// TAB key is decimal 9.
	if (uiinput == 9 || uiinput == _settings.keybindings.typeahead) {
	    if (typeahead) {
		if (searchlen == 0) {
		    typeahead = false;
		    // Typeahead now off.
		    free (search);
		    typeaheadskip = 0;
		    highlighted = savestart;
		    first_scr_ptr = savestart_first;
		} else {
		    unsigned found = 0;
		    for (const struct feed * f = _feed_list; f; f = f->next)
			if (s_strcasestr (f->title, search))	// Substring match.
			    ++found;
		    if (typeaheadskip == found - 1)	// found-1 to avoid empty tab cycle.
			typeaheadskip = 0;
		    else if (found > 1)	// If more than one match was found and user presses tab we will typeaheadskip matches.
			++typeaheadskip;
		}
	    } else {
		typeahead = true;
		// Typeahead now on.
		search = malloc (2);
		memset (search, 0, 2);
		searchlen = 0;
		// Save all start positions.
		savestart = highlighted;
		savestart_first = first_scr_ptr;
	    }
	}
	// ctrl+g clears typeahead.
	if (uiinput == 7) {
	    // But only if it was switched on previously.
	    if (typeahead) {
		typeahead = false;
		free (search);
		typeaheadskip = 0;
		highlighted = savestart;
		first_scr_ptr = savestart_first;
	    }
	}
	// ctrl+u clears line.
	if (uiinput == 21) {
	    searchlen = 0;
	    search = realloc (search, 2);
	    memset (search, 0, 2);
	}
    }
}
