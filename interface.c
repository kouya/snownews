// This file is part of Snownews - A lightweight console RSS newsreader
//
// Copyright (c) 2003-2004 Oliver Feiler <kiza@kcore.de>
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

#ifdef UTF_8
#define xmlStrlen xmlUTF8Strlen
#endif

#include <ncurses.h>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "config.h"
#include "main.h"
#include "netio.h"
#include "interface.h"
#include "xmlparse.h"
#include "conversions.h"
#include "dialog.h"
#include "ui-support.h"
#include "categories.h"
#include "about.h"
#include "io-internal.h"
#include "os-support.h"
#include "support.h"

extern struct keybindings keybindings;
extern struct color color;
extern int use_colors;
extern struct categories *first_category;
extern int cursor_always_visible;

static int resize_dirty = 0;

struct feed *first_bak = NULL;	// Backup first pointer for filter mode.
				// Needs to be global so it can be used in the signal handler.
				// Must be set to NULL by default and whenever it's not used anymore!

// Scrollable feed->description.
struct scrolltext {
	char * line;
	struct scrolltext * next_ptr;
	struct scrolltext * prev_ptr;
};

#ifdef SIGWINCH
void sig_winch (int p __attribute__((unused)))
{
	resize_dirty = 1;
}
#endif

// View newsitem in scrollable window.
// Speed of this code has been greatly increased in 1.2.1.
void UIDisplayItem (struct newsitem * current_item, struct feed * current_feed, char * categories) {
	int i, j;
	int uiinput;
	char tmpstr[512];
	char *newtext;
	char *newtextwrapped;
	char *textslice;
	int columns;
	struct scrolltext *textlist = NULL;
	struct scrolltext *cur_ptr = NULL;
	struct scrolltext *first_ptr = NULL;
	struct scrolltext *tmp_ptr = NULL;
	struct scrolltext *next_ptr = NULL;
	int ypos;
	int linenumber = 0;	// First line on screen (scrolling). Ugly hack.
	int maxlines = 0;
	char *freeme;		// str pos pointer.
	char *converted;	// Converted from UTF-8 to local charset.
	int rewrap;		// Set to 1 if text needs to be rewrapped.
	int forceredraw = 0;
	char *date_str;

	// Activate rewrap.
	rewrap = 1;
	
	while (1) {
		if (forceredraw) {
			clear();
			forceredraw = 0;
		} else {
			move (0, 0);
			clrtobot();
		}
				
		ypos = 6;
		
		UISupportDrawHeader(categories);

		// If feed doesn't have a description print title instead.
		attron (WA_BOLD);
		columns = COLS-10;
		if (current_feed->description != NULL) {
			// Define columns, because malloc three lines down might fail
			// and corrupt mem while resizing the window.
			
			newtext = UIDejunk (current_feed->description);	
			converted = iconvert (newtext);
			if (converted == NULL)
				converted = strdup (newtext);
			
			mvaddnstr (2, 1, converted, columns);
			if (xmlStrlen ((xmlChar *)converted) > columns)
				mvaddstr (2, columns+1, "...");
				
			free (converted);
			free (newtext);
		} else {
			// Print original feed's description and not "(New headlines)".
			if (current_feed->smartfeed == 1) {
				newtext = UIDejunk (current_item->data->parent->description);
				converted = iconvert (newtext);
				if (converted == NULL)
				converted = strdup (newtext);
			
				mvaddnstr (2, 1, converted, columns);
				if (xmlStrlen ((xmlChar *)converted) > columns)
					mvaddstr (2, columns+1, "...");

				free (converted);
				free (newtext);
			} else
				mvaddstr (2, 1, current_feed->title);
		}
		
		attroff (WA_BOLD);
		
		// Print publishing date if we have one.
		move (LINES-3, 0);
		clrtoeol();
		if (current_item->data->date != 0) {
			date_str = unixToPostDateString(current_item->data->date);
			if (date_str) {
				attron(WA_BOLD);
				mvaddstr(LINES-3, 2, date_str);
				attroff(WA_BOLD);
				free (date_str);
			}
		}
		
		// Don't print feed titles longer than screenwidth here.
		// columns is needed to avoid race conditions while resizing the window.
		columns = COLS-6;
		// And don't crash if title is empty.
		if (current_item->data->title != NULL) {
			newtext = UIDejunk (current_item->data->title);		
			converted = iconvert (newtext);
			if (converted == NULL)
				converted = strdup (newtext);
			
			if (xmlStrlen ((xmlChar *)converted) > columns) {
				mvaddnstr (4, 1, converted, columns);
				if (xmlStrlen ((xmlChar *)converted) > columns)
					mvaddstr (4, columns+1, "...");
			} else
				mvprintw (4, (COLS/2)-(xmlStrlen((xmlChar *)converted)/2), "%s", converted);
			
			free (converted);
			free (newtext);
		} else
			mvaddstr (4, (COLS/2)-(strlen(_("No title"))/2), _("No title"));
		
		if (current_item->data->description == NULL)
			mvprintw (6, 1, _("No description available."));
		else {
			if (strlen(current_item->data->description) == 0)
				mvprintw (6, 1, _("No description available."));
			else {
				// Only generate a new scroll list if we need to rewrap everything.
				// Otherwise just skip this block.
				if (rewrap) {
					converted = iconvert (current_item->data->description);
					if (converted == NULL)
						converted = strdup (current_item->data->description);					
					newtext = UIDejunk (converted);
					free (converted);
					newtextwrapped = WrapText (newtext, COLS-4);
					free (newtext);
					freeme = newtextwrapped;	// Set ptr to str start so we can free later.
					
					// Split newtextwrapped at \n and put into double linked list.
					while (1) {
						textslice = strsep (&newtextwrapped, "\n");
						
						// Find out max number of lines text has.
						maxlines++;
						
						if (textslice != NULL) {
							textlist = malloc (sizeof(struct scrolltext));
							
							textlist->line = strdup(textslice);
							
							// Gen double list with new items at bottom.
							textlist->next_ptr = NULL;
							if (first_ptr == NULL) {
								textlist->prev_ptr = NULL;
								first_ptr = textlist;
							} else {
								textlist->prev_ptr = first_ptr;
								while (textlist->prev_ptr->next_ptr != NULL)
									textlist->prev_ptr = textlist->prev_ptr->next_ptr;
								textlist->prev_ptr->next_ptr = textlist;
							}
						} else
							break;
					}
				
					free (freeme);
					rewrap = 0;
				}
				
				j = linenumber;
				// We sould now have the linked list setup'ed... hopefully.
				for (cur_ptr = first_ptr; cur_ptr != NULL; cur_ptr = cur_ptr->next_ptr) {
					// Skip lines we don't need to display.
					if (j > 0) {
						j--;
						continue;
					}
					if (ypos > LINES-4)
						continue;
					mvaddstr (ypos, 2, cur_ptr->line);
					ypos++;
				}
			}
		}
		
		// Apply color style.
		if (use_colors) {
			attron (COLOR_PAIR(3));
			if (color.urljumpbold)
				attron (WA_BOLD);
		} else
			attron (WA_BOLD);

		// Print article URL.
		if (current_item->data->link == NULL)
			mvaddstr (LINES-2, 1, _("-> No link"));
		else
			mvprintw (LINES-2, 1, "-> %s", current_item->data->link);
		
		// Disable color style.
		if (use_colors) {
			attroff (COLOR_PAIR(3));
			if (color.urljumpbold)
				attroff (WA_BOLD);
		} else
			attroff (WA_BOLD);
		
		// Set current item to read.
		current_item->data->readstatus = 1;
		
		snprintf (tmpstr, sizeof(tmpstr), _("Press '%c' or Enter to return to previous screen. Hit '%c' for help screen."),
			keybindings.prevmenu, keybindings.help);
		UIStatus (tmpstr, 0, 0);
		uiinput = getch();
		
		if ((uiinput == keybindings.help) ||
			(uiinput == '?'))
			UIDisplayItemHelp();
		if ((uiinput == '\n') ||
			(uiinput == keybindings.prevmenu) || (uiinput == keybindings.enter)) {
			// Free the wrapped text linked list.
			// Why didn't valgrind find this? Counted as "still reachable".
			// Strange voodoo magic may be going on here! Maybe we just append
			// to the chain for each successive call and the prev_ptrs are still valid?
			for (cur_ptr = first_ptr; cur_ptr != NULL; cur_ptr = next_ptr) {
				tmp_ptr = cur_ptr;
				if (cur_ptr->next_ptr != NULL)
					first_ptr = cur_ptr->next_ptr;
				free (tmp_ptr->line);
				next_ptr = cur_ptr->next_ptr;
				free (tmp_ptr);
			}
			
			return;
		}
		if (uiinput == keybindings.urljump)
			UISupportURLJump (current_item->data->link);
		
		if ((uiinput == keybindings.next) ||
			(uiinput == KEY_RIGHT)) {
			if (current_item->next_ptr != NULL) {
				current_item = current_item->next_ptr;
				linenumber = 0;
				rewrap = 1;
				maxlines = 0;
			} else {
				// Setting rewrap to 1 to get the free block below executed.
				rewrap = 1;
				uiinput = ungetch(keybindings.prevmenu);
			}
		}
		if ((uiinput == keybindings.prev) ||
			(uiinput == KEY_LEFT)) {
			if (current_item->prev_ptr != NULL) {
				current_item = current_item->prev_ptr;
				linenumber = 0;
				rewrap = 1;
				maxlines = 0;
			} else {
				// Setting rewrap to 1 to get the free block below executed.
				rewrap = 1;
				uiinput = ungetch(keybindings.prevmenu);
			}
		}
		// Scroll by one page.
		if ((uiinput == KEY_NPAGE) || (uiinput == 32) ||
			(uiinput == keybindings.pdown)) {
			for (i = 0; i < LINES-9; i++) {
				if (linenumber+(LINES-7) < maxlines)
					linenumber++;
			}
		}
		if ((uiinput == KEY_PPAGE) || (uiinput == keybindings.pup)) {
			for (i = 0; i < LINES-9; i++) {
				if (linenumber >= 1)
					linenumber--;
			}
		}
		if (uiinput == KEY_UP) {
			if (linenumber >= 1)
				linenumber--;
		}
		if (uiinput == KEY_DOWN) {
			if (linenumber+(LINES-7) < maxlines)
				linenumber++;
		}
		if (resize_dirty || uiinput == KEY_RESIZE) {
			rewrap = 1;
			// Reset maxlines, otherwise the program will scroll to far down.
			maxlines = 0;

			endwin();
			refresh();
			resize_dirty = 0;
		}
		if (uiinput == keybindings.about)
			UIAbout();
		// Redraw screen on ^L
		if (uiinput == 12)
			forceredraw = 1;
		
		// Free the linked list structure if we need to rewrap the text block.
		if (rewrap) {
			// Free textlist list before we start loop again.
			for (cur_ptr = first_ptr; cur_ptr != NULL; cur_ptr = next_ptr) {
				tmp_ptr = cur_ptr;
				if (cur_ptr->next_ptr != NULL)
					first_ptr = cur_ptr->next_ptr;
				free (tmp_ptr->line);
				next_ptr = cur_ptr->next_ptr;
				free (tmp_ptr);
			}
			// first_ptr must be set to NULL again!  = back to init value.
			first_ptr = NULL;
		}
	}
}


int UIDisplayFeed (struct feed * current_feed) {
	char tmpstr[512];
	char *search = NULL;	// Typeahead search string
	char *converted;	// Converted from UTF-8 to local charset.
	char *categories = NULL;
	char *newtext;
	int uiinput = 0;
	int typeahead = 0;	// Typeahead enabled?
	int skip = 0;		// # of lines to skip (for typeahead)
	int skipper = 0;	// # of lines already skipped
	int searchlen = 0;
	int count, found;
	int columns;
	int forceredraw = 0;
	int i, ypos;
	int itemnum;
	int highlightnum = 1;
	int reloaded = 0;		// We need to signal the main function if we changed feed contents.
	struct newsitem *savestart;	// Typeahead internal usage (tmp position save)
	struct newsitem *savestart_first = NULL;
	struct newsitem *cur_ptr;
	struct newsitem *curitem_ptr;
	struct newsitem *highlighted;
	struct newsitem *tmp_highlighted;
	struct newsitem *tmp_first;
	struct newsitem *first_scr_ptr;
	struct newsitem *markasread;
	int highlightline = LINES-1;
		
	// Set highlighted to first_ptr at the beginning.
	// Otherwise bad things (tm) will happen.
	first_scr_ptr = current_feed->items;
	highlighted = current_feed->items;
	
	// Select first unread item if we enter feed view or
	// leave first item active if there is no unread.
	tmp_highlighted = highlighted;
	tmp_first = first_scr_ptr;
	highlighted = first_scr_ptr;
	// Moves highlight to next unread item.
	
	// Check if we have no items at all!
	if (highlighted != NULL) {
		while (highlighted->next_ptr != NULL) {
			if (highlighted->data->readstatus == 1) {
				highlighted = highlighted->next_ptr;
				
				highlightnum++;
				if (highlightnum+1 > LINES-7) {
					// Fell off screen.
					if (first_scr_ptr->next_ptr != NULL)
						first_scr_ptr = first_scr_ptr->next_ptr;
				}	
				
			} else
				break;				
		}
	}
	// If *highlighted points to a read entry now we have hit the last
	// entry and there are no unread items anymore. Restore the original
	// location in this case.
	// Check if we have no items at all!
	if (highlighted != NULL) {
		if (highlighted->data->readstatus == 1) {
			highlighted = tmp_highlighted;
			first_scr_ptr = tmp_first;
		}
	}
	
	// Save first starting position. For typeahead.
	savestart = first_scr_ptr;
	
	// Put all categories of the current feed into a comma seperated list.
	categories = GetCategoryList (current_feed);
	
	while (1) {
		if (forceredraw) {
			clear();
			forceredraw = 0;
		} else {
			move (0, 0);
			clrtobot();
		}
		
		UISupportDrawHeader (categories);
		
		if (typeahead) {
			// This resets the offset for every typeahead loop.
			first_scr_ptr = current_feed->items;
			
			count = 0;
			skipper = 0;
			found = 0;
			for (cur_ptr = current_feed->items; cur_ptr != NULL; cur_ptr = cur_ptr->next_ptr) {
				// count+1 > LINES-7:
				// If the _next_ line would go over the boundary.
				if (count+1 > LINES-7) {
						if (first_scr_ptr->next_ptr != NULL)
							first_scr_ptr = first_scr_ptr->next_ptr;
				}
				if (searchlen > 0) {
					// if (strncmp (search, cur_ptr->title, searchlen) == 0) {
					// Substring match.
					if (cur_ptr->data->title != NULL) {
						if (s_strcasestr (cur_ptr->data->title, search) != NULL) {
							found = 1;
							highlighted = cur_ptr;
							if (skip >= 1) {
								if (skipper == skip) {
									break;
								} else {
									skipper++;
									// We need to increase counter also, because we
									// don't reach the end of the loop after continue!.
									count++;
									continue;
								}
							}
							break;
						}
					}
					count++;
				}
			}
			if (!found) {
				// Restore original position on no match.
				highlighted = savestart;
				first_scr_ptr = savestart_first;
			}
		}

		if (color.feedtitle > -1) {
			attron (COLOR_PAIR(4));
			if (color.feedtitlebold == 1)
				attron (WA_BOLD);
		} else
			attron (WA_BOLD);
		if (current_feed->description != NULL) {
			// Print a max of COLS-something chars.
			// This function might cause overflow if user resizes screen
			// during malloc and strncpy. To prevent this we use a private
			// columns variable.
			columns = COLS-10;
			
			newtext = UIDejunk (current_feed->description);
			converted = iconvert (newtext);
			if (converted == NULL)
				converted = strdup (newtext);
						
			mvaddnstr (2, 1, converted, columns);
			if (xmlStrlen ((xmlChar *)converted) > columns)
				mvaddstr(2, columns+1, "...");
			
			free (converted);
			free (newtext);
		} else {
			mvprintw (2, 1,	"%s", current_feed->title);
		}

		if (color.feedtitle > -1) {
			attroff (COLOR_PAIR(4));
			if (color.feedtitlebold == 1)
				attroff (WA_BOLD);
		} else
			attroff (WA_BOLD);
		
		// We start the item list at line 5.
		ypos = 4;
		itemnum = 1;
		
		columns = COLS-6;	// Cut max item length.
		// Print unread entries in bold.
		for (curitem_ptr = first_scr_ptr; curitem_ptr != NULL; curitem_ptr = curitem_ptr->next_ptr) {
			// Set cursor to start of current line and clear it.
			move (ypos, 0);
			clrtoeol();
			
			if (curitem_ptr->data->readstatus != 1) {
				// Apply color style.
				if (use_colors) {
					attron (COLOR_PAIR(2));
					if (color.newitemsbold)
						attron (WA_BOLD);
				} else
					attron (WA_BOLD);
			}

			if (curitem_ptr == highlighted) {
				highlightline = ypos;
				highlightnum = itemnum;
				attron (WA_REVERSE);
				for (i = 0; i < COLS; i++)
					mvaddch (ypos, i, ' ');
			}
			
			// Check for empty <title>
			if (curitem_ptr->data->title != NULL) {
				if (current_feed->smartfeed == 1) {
					snprintf (tmpstr, sizeof(tmpstr), "(%s) %s",
						curitem_ptr->data->parent->title, curitem_ptr->data->title);
					newtext = UIDejunk (tmpstr);
				} else
					newtext = UIDejunk (curitem_ptr->data->title);
				
				converted = iconvert (newtext);
				if (converted == NULL)
					converted = strdup (newtext);
				
				mvaddnstr (ypos, 1, converted, columns);
				if (xmlStrlen ((xmlChar *)converted) > columns)
					mvaddstr (ypos, columns+1, "...");

				free (converted);
				free (newtext);
			} else {
				if (current_feed->smartfeed == 1) {
					snprintf (tmpstr, sizeof(tmpstr), "(%s) %s",
						curitem_ptr->data->parent->title, _("No title"));
					mvaddstr (ypos, 1, tmpstr);
				} else
					mvaddstr (ypos, 1, _("No title"));
			}
			
			ypos++;
			if (curitem_ptr == highlighted)
				attroff (WA_REVERSE);
			if (curitem_ptr->data->readstatus != 1) {
				// Disable color style.
				if (use_colors) {
					attroff (COLOR_PAIR(2));
					if (color.newitemsbold)
						attroff (WA_BOLD);
				} else
					attroff (WA_BOLD);
			}
				
			if (itemnum >= LINES-7)
				break;
			itemnum++;
		}
		
		// Apply color style.
		if (use_colors) {
			attron (COLOR_PAIR(3));
			if (color.urljumpbold == 1)
				attron (WA_BOLD);
		} else
			attron (WA_BOLD);
		
		// Print feed URL.
		if (current_feed->link == NULL)
			mvaddstr (LINES-2, 1, _("-> No link"));
		else
			mvprintw (LINES-2, 1, "-> %s", current_feed->link);
		
		// Disable color style.
		if (use_colors) {
			attroff (COLOR_PAIR(3));
			if (color.urljumpbold == 1)
				attroff (WA_BOLD);
		} else
			attroff (WA_BOLD);
		
		if (typeahead) {
			snprintf (tmpstr, sizeof(tmpstr), "-> %s", search);
			UIStatus (tmpstr, 0, 0);
		} else {
			snprintf (tmpstr, sizeof(tmpstr), _("Press '%c' to return to main menu, '%c' to show help."),
				keybindings.prevmenu, keybindings.help);
			UIStatus (tmpstr, 0, 0);
		}
		
		move (highlightline, 0);
		uiinput = getch();
		
		if (typeahead) {
			// Only match real characters.
			if ((uiinput >= 32) && (uiinput <= 126)) {
				search[searchlen] = uiinput;
				search[searchlen+1] = 0;
				searchlen++;
				search = realloc (search, searchlen+2);
			// ASCII 127 is DEL, 263 is... actually I have
			// no idea, but it's what the text console returns.
			} else if ((uiinput == 127) || (uiinput == 263)) {
				// Don't let searchlen go below 0!
				if (searchlen > 0) {
					searchlen--;
					search[searchlen] = 0;
					search = realloc (search, searchlen+2);
				} else {
					typeahead = 0;
					free (search);
				}
			}
		} else {
			if ((uiinput == keybindings.help) ||
				(uiinput == '?'))
				UIDisplayFeedHelp();
			if (uiinput == keybindings.prevmenu) {
				free (categories);
				return reloaded;
			}
			if ((uiinput == KEY_UP) ||
				(uiinput == keybindings.prev)) {
				// Check if we have no items at all!
				if (highlighted != NULL) {
					if (highlighted->prev_ptr != NULL) {
						highlighted = highlighted->prev_ptr;
						if (highlightnum-1 < 1) {
							// First scr entry.
							if (first_scr_ptr->prev_ptr != NULL)
								first_scr_ptr = first_scr_ptr->prev_ptr;
							}
					}
				}
			}
			if ((uiinput == KEY_DOWN) ||
				(uiinput == keybindings.next)) {
				// Check if we have no items at all!
				if (highlighted != NULL) {
					if (highlighted->next_ptr != NULL) {
						highlighted = highlighted->next_ptr;
						if (highlightnum+1 > LINES-7) {
							// Fell off screen. */	
							if (first_scr_ptr->next_ptr != NULL)
								first_scr_ptr = first_scr_ptr->next_ptr;
						}	
					}
				}
			}
			// Move highlight one page up/down == LINES-9
			if ((uiinput == KEY_NPAGE) || (uiinput == 32) ||
				(uiinput == keybindings.pdown)) {
				if (highlighted != NULL) {
					for (i = highlightnum; i <= LINES-8; i++) {
						if (highlighted->next_ptr != NULL) {
							highlighted = highlighted->next_ptr;
						} else
							break;
					}
				}
				first_scr_ptr = highlighted;
			}
			if ((uiinput == KEY_PPAGE) || (uiinput == keybindings.pup)) {
				if (highlighted != NULL) {
					for (i = highlightnum; i <= LINES-8; i++) {
						if (highlighted->prev_ptr != NULL) {
							highlighted = highlighted->prev_ptr;
						} else
							break;
					}
				}
				first_scr_ptr = highlighted;
			}
			if ((uiinput == KEY_HOME) || (uiinput == keybindings.home)) {
				highlighted = current_feed->items;
				first_scr_ptr = current_feed->items;
			}
			if ((uiinput == KEY_END) || (uiinput == keybindings.end)) {
				if (highlighted != NULL) {
					while (highlighted->next_ptr != NULL) {
						highlighted = highlighted->next_ptr;
						highlightnum++;
						if (highlightnum+1 > LINES-6) {
							// If we fall off the screen, advance first_scr_ptr to next entry.
							if (first_scr_ptr->next_ptr != NULL)
								first_scr_ptr = first_scr_ptr->next_ptr;
						}
					}
				}
			}
			if ((uiinput == keybindings.reload) ||
				(uiinput == keybindings.forcereload)) {
				if (first_bak != NULL) {
					UIStatus (_("Please deactivate the category filter before using this function."), 2, 0);
					continue;
				}
				if (current_feed->smartfeed == 1)
					continue;
					
				if (uiinput == keybindings.forcereload) {
					free (current_feed->lastmodified);
					current_feed->lastmodified = NULL;
				}
				
				UpdateFeed (current_feed);
				highlighted = current_feed->items;
				// Reset first_scr_ptr if reloading.
				first_scr_ptr = current_feed->items;
				forceredraw = 1;
				reloaded = 1;
			}
			
			if (uiinput == keybindings.urljump)
				UISupportURLJump (current_feed->link);
			if (uiinput == keybindings.urljump2)
				if (highlighted != NULL) {
					UISupportURLJump (highlighted->data->link);
				}
			
			if (uiinput == keybindings.markread) {
				// Mark everything read.
				for (markasread = current_feed->items; markasread != NULL; markasread = markasread->next_ptr) {
					markasread->data->readstatus = 1;
				}
			}
			if (uiinput == keybindings.markunread) {
				// highlighted->data->readstatus = 0;
				// Works as toggle function now.
				if (highlighted != NULL) {
					(highlighted->data->readstatus == 0) ? (highlighted->data->readstatus = 1) : (highlighted->data->readstatus = 0);
					reloaded = 1;
				}
			}
			if (uiinput == keybindings.about)
				UIAbout();
			if (uiinput == keybindings.feedinfo)
				FeedInfo(current_feed);
			if (resize_dirty || uiinput == KEY_RESIZE) {
				endwin();
				refresh();
				resize_dirty = 0;
			}
			// Redraw screen on ^L
			if (uiinput == 12)
				forceredraw = 1;
		}
		
		if ((uiinput == '\n') ||
			((uiinput == keybindings.enter) && (!typeahead))) {
			// If typeahead is active clear it's state and free the structure.
			if (typeahead) {
				free (search);
				typeahead = 0;
				if (!cursor_always_visible)
					curs_set(0);
				skip = 0;
			}
			// Check if we have no items at all!
			// Don't even try to view a non existant item.
			if (highlighted != NULL) {
				UIDisplayItem (highlighted, current_feed, categories);
				tmp_highlighted = highlighted;
				tmp_first = first_scr_ptr;
				
				// highlighted = first_scr_ptr;
				// Moves highlight to next unread item.
				while (highlighted->next_ptr != NULL) {
					if (highlighted->data->readstatus == 1) {
						highlighted = highlighted->next_ptr;
						
						highlightnum++;
						if (highlightnum+1 > LINES-7) {
							// Fell off screen. */	
							if (first_scr_ptr->next_ptr != NULL)
								first_scr_ptr = first_scr_ptr->next_ptr;
						}	
						
					} else
						break;				
				}
				
				// If *highlighted points to a read entry now we have hit the last
				// entry and there are no unread items anymore. Restore the original
				// location in this case.
				if (highlighted->data->readstatus == 1) {
					highlighted = tmp_highlighted;
					first_scr_ptr = tmp_first;
				}
			}
		}
		
		// TAB key is decimal 9.
		if (uiinput == 9 || uiinput == keybindings.typeahead) {
			if (typeahead) {
				if (searchlen == 0) {
					typeahead = 0;
					// Typeahead now off.
					if (!cursor_always_visible)
						curs_set(0);
					free (search);
					skip = 0;
					highlighted = savestart;
					first_scr_ptr = savestart_first;
				} else {
					found = 0;
					for (cur_ptr = current_feed->items; cur_ptr != NULL; cur_ptr = cur_ptr->next_ptr) {
						// Substring match.
						if (cur_ptr->data->title != NULL) {	// Don't crash on items with no title!
							if (s_strcasestr (cur_ptr->data->title, search) != NULL)
								found++;
						}
					}
					if (skip == found-1) {
						skip = 0;
					} else if (found > 1) {
						// If more than one match was found and user presses tab we will skip matches.
						skip++;
					}
				}
			} else {
				typeahead = 1;
				// Typeahead now on.
				curs_set(1);
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
				typeahead = 0;
				free (search);
				skip = 0;
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

void UIMainInterface (void) {
	int i, ypos, len;
	int newcount;		// Number of new articles.
	int uiinput = 0;	// User input. Needs to be int to read function keys with getch
	int typeahead = 0;
	int skip = 0;		// Number of typeahead matches to skip.
	int skipper = 0;	// " skipped
	int found = 0;		// Number of typeahead matches.
	char *search = NULL;	// Typeahead search string.
	int searchlen = 0;	// " length.
	char tmp[1024];		// Used mostly for UIStatus. Always use snprintf!
	char *file, *hashme;
	struct feed *cur_ptr;
	struct feed *new_ptr;
	struct newsitem *curitem_ptr;
	struct newsitem *markasread;
	struct feed *highlighted;
	struct feed *first_scr_ptr;	// First pointer on top of screen. Used for scrolling.
	struct feed *removed;		// Removed struct tmp pointer.
	struct feed *savestart;
	struct feed *savestart_first = NULL;
	struct feedcategories *category;
	int itemnum;			// Num of items on screen. Max is LINES-4
	int highlightnum = 1;
	int columns;
	int count;
	int forceredraw = 0;
	char *filterstring = NULL;	// Returned category filter.
	char *filters[8];		// Category filters. Must be NULL when not used!
	char *localized_msg;		// Use ngettext to allow plural forms in translations.
	int numfilters = 0;		// Number of active filters.
	int filteractivated = 0;	// 0=no filer; 1=filer(s) set.
	int addfilteritem;
	int andxor = 0;			// Toggle for AND/OR combinations of filters.
	int update_smartfeeds = 1;
	int highlightline = LINES-1;	// Line with current selected item cursor
					// will be moved to this line to have it in
					// the same line as the highlight. Visually
					// impaired users with screen readers seem
					// to need this.

	first_scr_ptr = first_ptr;
	highlighted = first_ptr;
	savestart = first_scr_ptr;

	for (i = 0; i <= 7; i++)
		filters[i] = NULL;
	
	// Clear produces flickering when used inside the loop!
	// Only call it once and use move+clrtobot inside!
	clear();
	
	while (1) {
		if (update_smartfeeds) {
			// This only needs to be done if new items are added, old removed.
			// Reload, add, delete.
			SmartFeedsUpdate();
			update_smartfeeds = 0;
		}
		
		if (forceredraw) {
			// Call clear() to reinit everything.
			// Usually called after downloading/parsing feed to
			// avoid Ramsch on the screen.  :)
			clear();
			forceredraw = 0;
		} else {
			// Clear screen with clrtobot()
			move (0,0);
			clrtobot();
		}

		if (filters[0] == NULL)
			UISupportDrawHeader(NULL);
		else {
			len = strlen(filters[0]) + 1;
			filterstring = strdup (filters[0]);
			numfilters = 1;
			
			for (i = 1; i <= 7; i++) {
				if (filters[i] == NULL)
					break;
				
				len += strlen(filters[i]) + 3;
				filterstring = realloc (filterstring, len);
				strcat (filterstring, ", ");
				strcat (filterstring, filters[i]);
				
				numfilters++;
			}
			UISupportDrawHeader(filterstring);
			free (filterstring);
		}
			
		// If a filter is defined we need to make copy of the pointers in
		// struct feed and work on that.
		// Build a new list only with items matching current filter.
		//
		// Never EVER set filteractivated=1 if there is no filter defined!
		// This should be moved to its own function in ui-support.c!
		if (filteractivated) {
			if (first_bak == NULL)
				first_bak = first_ptr;
			first_ptr = NULL;
			for (cur_ptr = first_bak; cur_ptr != NULL; cur_ptr = cur_ptr->next_ptr) {
				found = 0;
				if (cur_ptr->feedcategories != NULL) {
					for (category = cur_ptr->feedcategories; category != NULL; category = category->next_ptr) {
						for (i = 0; i <= 7; i++) {
							if (filters[i] != NULL) {
								if (strcasecmp (category->name, filters[i]) == 0) {
									// Match found
									found++;
								}
							}
						}
					}
					if (!found)
						continue;
				} else
					continue;
				
				addfilteritem = 0;
				if (andxor == 0) {
					// AND
					if (found == numfilters)
						addfilteritem = 1;
				} else {
					// OR
					if (found)
						addfilteritem = 1;
				}
				
				if (addfilteritem) {
					new_ptr = newFeedStruct();
					// These structs will get leaked!
					// Yeah, yeah, it is on my todo list...
					// Though valgrind doesn't report them as leaked.

					// Duplicate pointers
					new_ptr->feedurl = cur_ptr->feedurl;
					new_ptr->feed = cur_ptr->feed;
					new_ptr->title = cur_ptr->title;
					new_ptr->link = cur_ptr->link;
					new_ptr->description = cur_ptr->description;
					new_ptr->lastmodified = cur_ptr->lastmodified;
					new_ptr->lasthttpstatus = cur_ptr->lasthttpstatus;
					new_ptr->cookies = cur_ptr->cookies;
					new_ptr->authinfo = cur_ptr->authinfo;
					new_ptr->servauth = cur_ptr->servauth;
					new_ptr->items = cur_ptr->items;
					new_ptr->problem = cur_ptr->problem;
					new_ptr->override = cur_ptr->override;
					new_ptr->original = cur_ptr->original;
					new_ptr->perfeedfilter = cur_ptr->perfeedfilter;
					new_ptr->execurl = cur_ptr->execurl;
					new_ptr->smartfeed = 0;
					new_ptr->feedcategories = cur_ptr->feedcategories;

					// Add to bottom of pointer chain.
					new_ptr->next_ptr = NULL;
					if (first_ptr == NULL) {
						new_ptr->prev_ptr = NULL;
						first_ptr = new_ptr;
					} else {
						new_ptr->prev_ptr = first_ptr;
						while (new_ptr->prev_ptr->next_ptr != NULL)
							new_ptr->prev_ptr = new_ptr->prev_ptr->next_ptr;
						new_ptr->prev_ptr->next_ptr = new_ptr;
					}
				}
			}
			first_scr_ptr = first_ptr;
			highlighted = first_scr_ptr;
			filteractivated = 0;
		}
		
		ypos = 2;
		itemnum = 1;
	
		if (typeahead) {
			// This resets the offset for every typeahead loop.
			first_scr_ptr = first_ptr;
			
			count = 0;
			skipper = 0;
			found = 0;
			for (cur_ptr = first_ptr; cur_ptr != NULL; cur_ptr = cur_ptr->next_ptr) {
				// count+1 >= Lines-4:
				// If the _next_ line would go over the boundary.
				if (count+1 > LINES-4) {
						if (first_scr_ptr->next_ptr != NULL)
							first_scr_ptr = first_scr_ptr->next_ptr;
				}
				if (searchlen > 0) {
					// Exact match from beginning of line.
					// if (strncmp (search, cur_ptr->title, searchlen) == 0) {
					// Substring match.
					if (s_strcasestr (cur_ptr->title, search) != NULL) {
						found = 1;
						highlighted = cur_ptr;
						if (skip >= 1) {
							if (skipper == skip) {
								break;
							} else {
								skipper++;
								// We need to increase counter also, because we
								// don't reach the end of the loop after continue!.
								count++;
								continue;
							}
						}
						break;
					}
				}
				count++;
			}
			if (!found) {
				// Restore original position on no match.
				highlighted = savestart;
				first_scr_ptr = savestart_first;
			}
		}
		
		for (cur_ptr = first_scr_ptr; cur_ptr != NULL; cur_ptr = cur_ptr->next_ptr) {
			// Set cursor to start of current line and clear it.
			move (ypos, 0);
			clrtoeol();
			
			// Determine number of new items in feed.
			newcount = 0;
			for (curitem_ptr = cur_ptr->items; curitem_ptr != NULL; curitem_ptr = curitem_ptr->next_ptr) {
				if (curitem_ptr->data->readstatus == 0)
					newcount++;
			}
			
			// Make highlight if we are the highlighted feed
			if (cur_ptr == highlighted) {
				highlightline = ypos;
				highlightnum = itemnum;
				attron (WA_REVERSE);
				for (i = 0; i < COLS; i++)
					mvaddch (ypos, i, ' ');
			}
			
			if (cur_ptr->feedcategories != NULL)
				columns = COLS-26-strlen(_("new"));
			else
				columns = COLS-9-strlen(_("new"));
			
			mvaddnstr (ypos, 1, cur_ptr->title, columns);
			if (xmlStrlen((xmlChar *)cur_ptr->title) > columns)
				mvaddstr (ypos, columns+1, "...");
			
			if (cur_ptr->problem)
				mvaddch (ypos, 0, '!');
			
			// Execute this _after_ the for loop. Otherwise it'll suck CPU like crazy!
			if (newcount != 0) {
				localized_msg = ngettext("%3d new", "%3d new", newcount);
				snprintf(tmp, sizeof(tmp), localized_msg, newcount);
				mvaddstr (ypos, COLS-1-strlen(localized_msg), tmp);
			}
			
			if (cur_ptr->feedcategories != NULL)
				mvaddnstr (ypos, COLS-21-strlen(_("new")), cur_ptr->feedcategories->name, 15);
						
			ypos++;
			
			if (cur_ptr == highlighted)
				attroff (WA_REVERSE);
			
			if (itemnum >= LINES-4)
				break;
			itemnum++;
		}

		if (typeahead) {
			snprintf (tmp, sizeof(tmp), "-> %s", search);
			UIStatus (tmp, 0, 0);
		} else {
			snprintf (tmp, sizeof(tmp), _("Press '%c' for help window."), keybindings.help);
			if (easterEgg()) {
				snprintf (tmp, sizeof(tmp), _("Press '%c' for help window. (Press '%c' to play Santa Hunta!)"),
					keybindings.help, keybindings.about);
			}
			UIStatus (tmp, 0, 0);
		}
		
		move (highlightline, 0);
		uiinput = getch();
		
		if (typeahead) {
			// Only match real characters.
			if ((uiinput >= 32) && (uiinput <= 126)) {
				search[searchlen] = uiinput;
				search[searchlen+1] = 0;
				searchlen++;
				search = realloc (search, searchlen+2);
			} else if ((uiinput == 127) || (uiinput == 263)) {
				// Don't let searchlen go below 0!
				if (searchlen > 0) {
					searchlen--;
					search[searchlen] = 0;
					search = realloc (search, searchlen+2);
				} else {
					typeahead = 0;
					free (search);
				}
			}
		} else {
			if (uiinput == keybindings.quit) {
				// Restore original first_ptr if filter is defined!
				if (filters[0] != NULL)
					first_ptr = first_bak;
				
				MainQuit (NULL, NULL);
			}
			if ((uiinput == keybindings.reload) ||
				(uiinput == keybindings.forcereload)) {
				if (filters[0] != NULL) {
					UIStatus (_("Please deactivate the category filter before using this function."), 2, 0);
					continue;
				}
				
				if (highlighted) {
					if (uiinput == keybindings.forcereload) {
						free (highlighted->lastmodified);
						highlighted->lastmodified = NULL;
					}
				}
				
				UpdateFeed (highlighted);
				
				forceredraw = 1;
				update_smartfeeds = 1;
			}
			if (uiinput == keybindings.reloadall) {
				if (filters[0] != NULL) {
					UIStatus (_("Please deactivate the category filter before using this function."), 2, 0);
					continue;
				}
				UpdateAllFeeds();
				forceredraw = 1;
				update_smartfeeds = 1;
			}
			if ((uiinput == keybindings.addfeed) ||
				(uiinput == keybindings.newheadlines)) {
				if (filters[0] != NULL) {
					UIStatus (_("Please deactivate the category filter before using this function."), 2, 0);
					continue;
				}
				if (uiinput == keybindings.addfeed) {
					switch (UIAddFeed(NULL)) {
						case 0:
							UIStatus (_("Successfully added new item..."), 1, 0);
							break;
						case 2:
							UIStatus (_("Invalid URL! Please add http:// if you forgot this."), 2, 1);
							continue;
							break;
						case -1:
							UIStatus (_("There was a problem adding the feed!"), 2, 1);
							break;
						default:
							break;
					}
				} else {
					if (!SmartFeedExists("newitems"))
						UIAddFeed ("smartfeed:/newitems");
				}
				
				// Scroll to top of screen and redraw everything.
				highlighted = first_ptr;
				first_scr_ptr = first_ptr;
				forceredraw = 1;
				update_smartfeeds = 1;
			}
			if ((uiinput == keybindings.help) ||
				(uiinput == '?'))
				UIHelpScreen();
			if (uiinput == keybindings.deletefeed) {
				// This should be moved to its own function in ui-support.c!
				if (filters[0] != NULL) {
					UIStatus (_("Please deactivate the category filter before using this function."), 2, 0);
					continue;
				}
				
				// Move this code into its own function!
				
				// If the deleted feed was the last one of a specific category,
				// remove this category from the global list.
				if (highlighted) {
					if (UIDeleteFeed(highlighted->title) == 1) {
						// Do it!
						removed = highlighted;
						// Save current highlight position.
						cur_ptr = highlighted;
						
						// Remove cachefile from filesystem.
						hashme = Hashify(highlighted->feedurl);
						len = (strlen(getenv("HOME")) + strlen(hashme) + 18);
						file = malloc (len);
						snprintf (file, len, "%s/.snownews/cache/%s", getenv("HOME"), hashme);
						
						// Errors from unlink can be ignored. Worst thing that happens is that
						// we delete a file that doesn't exist.
						unlink (file);
						
						free (file);
						free (hashme);
										
						// Unlink pointer from chain.
						if (highlighted == first_ptr) {
							// first element
							if (first_ptr->next_ptr != NULL) {
								first_ptr = first_ptr->next_ptr;
								first_scr_ptr = first_ptr;
								highlighted->next_ptr->prev_ptr = NULL;
							} else {
								first_ptr = NULL;
								first_scr_ptr = NULL;
							}
							// Set new highlighted to first_ptr again.
							cur_ptr = first_ptr;
						} else if (highlighted->next_ptr == NULL) {
							// last element
							// Set new highlighted to element before deleted one.
							cur_ptr = highlighted->prev_ptr;
							// If highlighted was first line move first line upward in pointer chain.
							if (highlighted == first_scr_ptr)
								first_scr_ptr = first_scr_ptr->prev_ptr;
							
							highlighted->prev_ptr->next_ptr = NULL;
						} else {
							// element inside list */		
							// Set new highlighted to element after deleted one.
							cur_ptr = highlighted->next_ptr;
							// If highlighted was last line, move first line downward in pointer chain.
							if (highlighted == first_scr_ptr)
								first_scr_ptr = first_scr_ptr->next_ptr;
							
							highlighted->prev_ptr->next_ptr = highlighted->next_ptr;
							highlighted->next_ptr->prev_ptr = highlighted->prev_ptr;
						}
						// Put highlight to new highlight position.
						highlighted = cur_ptr;
					
						// free (removed) pointer
						if (removed->smartfeed == 0) {
							if (removed->items != NULL) {
								while (removed->items->next_ptr != NULL) {
									removed->items = removed->items->next_ptr;
									free (removed->items->prev_ptr->data->title);
									free (removed->items->prev_ptr->data->link);
									free (removed->items->prev_ptr->data->description);
									free (removed->items->prev_ptr);
								}
								free (removed->items->data->title);
								free (removed->items->data->link);
								free (removed->items->data->description);
								free (removed->items);
							}
							
							free (removed->feedurl);
							free (removed->feed);
							free (removed->title);
							free (removed->link);
							free (removed->description);
							free (removed->lastmodified);
							free (removed->override);
							free (removed->original);
							free (removed->cookies);
							free (removed->authinfo);
							free (removed->servauth);
							free (removed);
						}
						
						update_smartfeeds = 1;
					}
				}
			}
			if ((uiinput == KEY_UP) ||
				(uiinput == keybindings.prev)) {
				if (highlighted != NULL) {
					if (highlighted->prev_ptr != NULL) {
						highlighted = highlighted->prev_ptr;
						if (highlightnum-1 < 1) {
							// Reached first onscreen entry.
							if (first_scr_ptr->prev_ptr != NULL)
								first_scr_ptr = first_scr_ptr->prev_ptr;
						}
					}
				}
			}
			if ((uiinput == KEY_DOWN) ||
				(uiinput == keybindings.next)) {
				if (highlighted != NULL) {
					if (highlighted->next_ptr != NULL) {
						highlighted = highlighted->next_ptr;
						if (highlightnum+1 > LINES-4) {
							// If we fall off the screen, advance first_scr_ptr
							// to next entry.
							if (first_scr_ptr->next_ptr != NULL)
								first_scr_ptr = first_scr_ptr->next_ptr;
						}
					}
				}
			}
			// Move highlight one page up/down == LINES-6
			if ((uiinput == KEY_NPAGE) || (uiinput == 32) ||
				(uiinput == keybindings.pdown)) {
				if (highlighted != NULL) {
					for (i = highlightnum; i <= LINES-5; i++) {
						if (highlighted->next_ptr != NULL) {
							highlighted = highlighted->next_ptr;
						} else
							break;
					}
				}
				first_scr_ptr = highlighted;
			}
			if ((uiinput == KEY_PPAGE) || (uiinput == keybindings.pup)) {
				if (highlighted != NULL) {
					for (i = highlightnum; i <= LINES-5; i++) {
						if (highlighted->prev_ptr != NULL) {
							highlighted = highlighted->prev_ptr;
						} else
							break;
					}
				}
				first_scr_ptr = highlighted;
			}
			if ((uiinput == KEY_HOME) || (uiinput == keybindings.home)) {
				highlighted = first_ptr;
				first_scr_ptr = first_ptr;
			}
			if ((uiinput == KEY_END) || (uiinput == keybindings.end)) {
				if (highlighted != NULL) {
					while (highlighted->next_ptr != NULL) {
						highlighted = highlighted->next_ptr;
						highlightnum++;
						// Why is the following calculation highlightnum+1>LINES-3,
						// but for KEY_DOWN it must be highlightnum+1>LINES-4?
						// I don't get it, it's the same code, no?
						if (highlightnum+1 > LINES-3) {
							// If we fall off the screen, advance first_scr_ptr to next entry.
							if (first_scr_ptr->next_ptr != NULL)
								first_scr_ptr = first_scr_ptr->next_ptr;
						}
					}
				}
			}
			if (uiinput == keybindings.moveup) {
				// This function is deactivated when a filter is active.
				if (filters[0] != NULL) {
					UIStatus (_("You cannot move items while a category filter is defined!"), 2, 0);
					continue;
				}
				// Move item up.
				if (highlighted) {
					if (highlighted->prev_ptr != NULL) {
						SwapPointers (highlighted, highlighted->prev_ptr);
						highlighted = highlighted->prev_ptr;
							if (highlightnum-1 < 1) {
								if (first_scr_ptr->prev_ptr != NULL)
									first_scr_ptr = first_scr_ptr->prev_ptr;
							}
					}
					update_smartfeeds = 1;
				}
			}
			if (uiinput == keybindings.movedown) {
				// This function is deactivated when a filter is active.
				if (filters[0] != NULL) {
					UIStatus (_("You cannot move items while a category filter is defined!"), 2, 0);
					continue;
				}
				// Move item down.
				
				if (highlighted) {
					if (highlighted->next_ptr != NULL) {
						SwapPointers (highlighted, highlighted->next_ptr);
						highlighted = highlighted->next_ptr;
						if (highlightnum+1 > LINES-4) {
							if (first_scr_ptr->next_ptr != NULL)
									first_scr_ptr = first_scr_ptr->next_ptr;
						}
					}
					update_smartfeeds = 1;
				}
			}
			if (uiinput == keybindings.dfltbrowser)
				UIChangeBrowser();
			if (uiinput == keybindings.markallread) {
				// This function is safe for using in filter mode, because it only
				// changes int values. It automatically marks the correct ones read
				// if a filter is applied since we are using a copy of the main data.
				for (cur_ptr = first_ptr; cur_ptr != NULL; cur_ptr = cur_ptr->next_ptr) {
					for (markasread = cur_ptr->items; markasread != NULL; markasread = markasread->next_ptr) {
						markasread->data->readstatus = 1;
					}
				}
			}
			if (uiinput == keybindings.about)
				UIAbout();
			if (uiinput == keybindings.changefeedname) {
				// This needs to be worked on before it works while a filter is applied!
				if (filters[0] != NULL) {
					UIStatus (_("Please deactivate the category filter before using this function."), 2, 0);
					continue;
				}
				if (highlighted) {
					UIChangeFeedName(highlighted);
					update_smartfeeds = 1;
				}
			}
			if (uiinput == keybindings.perfeedfilter) {
				if (filters[0] != NULL) {
					UIStatus (_("Please deactivate the category filter before using this function."), 2, 0);
					continue;
				}
				if (highlighted)
					UIPerFeedFilter (highlighted);
			}
			if (uiinput == keybindings.sortfeeds) {
				// Deactivate sorting function if filter is applied.
				if (filters[0] != NULL) {
					UIStatus (_("Please deactivate the category filter before using this function."), 2, 0);
					continue;
				}
				if (highlighted) {
					SnowSort();
					update_smartfeeds = 1;
				}
			}
			if (uiinput == keybindings.categorize) {
				// This needs to be worked on before it works while a filter is applied!
				if (filters[0] != NULL) {
					UIStatus (_("Please deactivate the category filter before using this function."), 2, 0);
					continue;
				}
				if (highlighted) {
					if (highlighted->smartfeed != 0)
						continue;
					
					CategorizeFeed (highlighted);
				}
			}
			if (uiinput == keybindings.filter) {
				// GUI to set a filter
				filterstring = DialogGetCategoryFilter();
				if (filterstring != NULL) {
					for (i = 0; i <= 7; i++) {
						if (filters[i] == NULL) {
							filters[i] = strdup (filterstring);
							break;
						}
					}
				filteractivated = 1;
				} else {
					ResetFilters (filters);
					
					// If DialogGetCategoryFilter() was used to switch off filter
					// Restore first_ptr here.
					if (first_bak != NULL) {
						// Restore first_ptr
						first_ptr = first_bak;
						first_scr_ptr = first_ptr;
						highlighted = first_ptr;
						first_bak = NULL;
					}
				}
				free (filterstring);
			}
			if (uiinput == keybindings.filtercurrent) {
				// Set filter to primary filter of this feed.
				// Free filter if it's not empty to avoid memory leaks!
				if (filters[0] != NULL) {
					ResetFilters (filters);
					
					// Set new filter to primary category.
					// highlighted can be null if there is no feed highlighted.
					// First start, no feeds ever added, multiple filters applied that no feed matches.
					if (highlighted != NULL) {
						if (highlighted->feedcategories != NULL) {
							filters[0] = strdup (highlighted->feedcategories->name);
							filteractivated = 1;
						}
					}
					
					// Restore first_ptr
					first_ptr = first_bak;
					first_scr_ptr = first_ptr;
					highlighted = first_ptr;
					first_bak = NULL;
				} else if ((highlighted != NULL) && (highlighted->feedcategories != NULL)) {
					filters[0] = strdup (highlighted->feedcategories->name);
					filteractivated = 1;
				}
			}
			if (uiinput == keybindings.nofilter) {
				// Remove all filters.
				if (filters[0] != NULL) {
					ResetFilters (filters);
					
					// Restore first_ptr
					first_ptr = first_bak;
					first_scr_ptr = first_ptr;
					highlighted = first_ptr;
					first_bak = NULL;
				}
			}
			// AND or OR matching for the filer.
			if (uiinput == 'X') {
				// Only if we have a filter!
				if (filters[0] != NULL) {
					andxor = !andxor;
					// Reconstruct filter.
					filteractivated = 1;
				}
			}
			if (resize_dirty || uiinput == KEY_RESIZE) {
				endwin();
				refresh();
				resize_dirty = 0;
			}
			// Redraw screen on ^L
			if (uiinput == 12)
				forceredraw = 1;
			
			if (uiinput == 'E') {
				displayErrorLog();
				forceredraw = 1;
			}
		}
		if ((uiinput == '\n') ||
			((uiinput == keybindings.enter) && (!typeahead))) {
			// If typeahead is active clear it's state and free the structure.
			if (typeahead) {
				free (search);
				typeahead = 0;
				// Clear skip flag, otherwise type ahead breaks, because we
				// have a skip "offset" when using it again.
				skip = 0;
			}
			// Select this feed, open and view entries.
			// If contents of the feed was changed during display (reload),
			// regenerate the smartfeed.
			if (highlighted != NULL) {
				if (UIDisplayFeed (highlighted) != 0)
					update_smartfeeds = 1;
			}
			
			// Clear screen after we return from here.
			move(0,0);
			clrtobot();
		}
		
		// TAB key is decimal 9.
		if (uiinput == 9 || uiinput == keybindings.typeahead) {
			if (typeahead) {
				if (searchlen == 0) {
					typeahead = 0;
					// Typeahead now off.
					free (search);
					skip = 0;
					highlighted = savestart;
					first_scr_ptr = savestart_first;
				} else {
					found = 0;
					for (cur_ptr = first_ptr; cur_ptr != NULL; cur_ptr = cur_ptr->next_ptr) {
						// Substring match.
						if (s_strcasestr (cur_ptr->title, search) != NULL)
							found++;
					}
					if (skip == found-1) { // found-1 to avoid empty tab cycle.
						skip = 0;
					} else if (found > 1) {
						// If more than one match was found and user presses tab we will skip matches.
						skip++;
					}
				}
			} else {
				typeahead = 1;
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
				typeahead = 0;
				free (search);
				skip = 0;
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
