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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ncurses.h>

#include "config.h"
#include "version.h"
#include "ui-support.h"
#include "io-internal.h"

extern struct color color;
extern char *browser;
extern int cursor_always_visible;

// Init the ncurses library.
void InitCurses (void) {
	initscr();
	
	keypad (stdscr, TRUE);	// Activate keypad so we can read function keys with getch.
	
	cbreak();				// No buffering.
	noecho();				// Do not echo typed chars onto the screen.
	clear();
	if (!cursor_always_visible)
		curs_set(0);			// Hide cursor.
	refresh();
}

// Print text in statusbar.
void UIStatus (const char * text, int delay, int warning) {
	if (warning)
		attron (COLOR_PAIR(10));
	clearLine (LINES-1, INVERSE);
	
	attron (WA_REVERSE);
	mvaddnstr (LINES-1, 1, text, COLS-2);
	
	// attroff is called here. If the calling function had it switched on,
	// switch it on again there!
	if (warning)
		attroff (COLOR_PAIR(10));
	attroff (WA_REVERSE);
		
	refresh();
	
	if (delay)
		sleep (delay);
}

// Swap all pointers inside a feed struct.
// Should only swap next and prev pointers of two structs!
void SwapPointers (struct feed * one, struct feed * two) {
	struct feed *tmp;

	tmp = newFeedStruct();
	
	tmp->feedurl = one->feedurl;
	tmp->feed = one->feed;
	tmp->content_length = one->content_length;
	tmp->title = one->title;
	tmp->link = one->link;
	tmp->description = one->description;
	tmp->lastmodified = one->lastmodified;
	tmp->lasthttpstatus = one->lasthttpstatus;
	tmp->content_type = one->content_type;
	tmp->cookies = one->cookies;
	tmp->authinfo = one->authinfo;
	tmp->servauth = one->servauth;
	tmp->items = one->items;
	tmp->problem = one->problem;
	tmp->override = one->override;
	tmp->original = one->original;
	tmp->perfeedfilter = one->perfeedfilter;
	tmp->execurl = one->execurl;
	tmp->smartfeed = one->smartfeed;
	tmp->feedcategories = one->feedcategories;
		
	one->feedurl = two->feedurl;
	one->feed = two->feed;
	one->content_length = two->content_length;
	one->title = two->title;
	one->link = two->link;
	one->description = two->description;
	one->lastmodified = two->lastmodified;
	one->lasthttpstatus = two->lasthttpstatus;
	one->content_type = two->content_type;
	one->cookies = two->cookies;
	one->authinfo = two->authinfo;
	one->servauth = two->servauth;
	one->items = two->items;
	one->problem = two->problem;
	one->override = two->override;
	one->original = two->original;
	one->perfeedfilter = two->perfeedfilter;
	one->execurl = two->execurl;
	one->smartfeed = two->smartfeed;
	one->feedcategories = two->feedcategories;
	
	two->feedurl = tmp->feedurl;
	two->feed = tmp->feed;
	two->content_length = tmp->content_length;
	two->title = tmp->title;
	two->link = tmp->link;
	two->description = tmp->description;
	two->lastmodified = tmp->lastmodified;
	two->lasthttpstatus = tmp->lasthttpstatus;
	two->content_type = tmp->content_type;
	two->cookies = tmp->cookies;
	two->authinfo = tmp->authinfo;
	two->servauth = tmp->servauth;
	two->items = tmp->items;
	two->problem = tmp->problem;
	two->override = tmp->override;
	two->original = tmp->original;
	two->perfeedfilter = tmp->perfeedfilter;
	two->execurl = tmp->execurl;
	two->smartfeed = tmp->smartfeed;
	two->feedcategories = tmp->feedcategories;
	
	free (tmp);
}

// Ignore "A", "The", etc. prefixes when sorting feeds.
const char * SnowSortIgnore (const char * title) {
	if (strncasecmp (title, "a ", 2) == 0)
		return title+2;
	else if (strncasecmp (title, "the ", 4) == 0)
		return title+4;
	else
		return title;
}

// Sort the struct list alphabetically.
// Sorting criteria is struct feed->title
void SnowSort (void) {
	int elements = 0;
	int i;
	const char *one, *two;
	struct feed * cur_ptr;
	
	// If there is no element do not run sort or it'll crash!
	if (first_ptr == NULL)
		return;

	UIStatus(_("Sorting, please wait..."), 0, 0);
	
	for (cur_ptr = first_ptr; cur_ptr != NULL; cur_ptr = cur_ptr->next_ptr)
		elements++;
	
	for (i = 0; i <= elements; i++) {
		for (cur_ptr = first_ptr; cur_ptr->next_ptr != NULL; cur_ptr = cur_ptr->next_ptr) {
			one = SnowSortIgnore (cur_ptr->title);
			two = SnowSortIgnore (cur_ptr->next_ptr->title);
			
			if (strcasecmp (one, two) > 0)
				SwapPointers (cur_ptr, cur_ptr->next_ptr);	
		}	
	}
}

// Draw a box with WA_REVERSE at coordinates x1y1/x2y2
void UISupportDrawBox (int x1, int y1, int x2, int y2) {
	int i, j;
	
	attron (WA_REVERSE);
	for (i = y1; i <= y2; i++) {
		for (j = x1; j <= x2; j++) {
			// Pretty borders. Well, they just sucked. ;)
			if (i == y1) {
				if (j == x1)
					mvaddch (i, j, ACS_ULCORNER);
				else if (j == x2)
					mvaddch (i, j, ACS_URCORNER);
				else
					mvaddch (i, j, ACS_HLINE);
			} else if (i == y2) {
				if (j == x1)
					mvaddch (i, j, ACS_LLCORNER);
				else if (j == x2)
					mvaddch (i, j, ACS_LRCORNER);
				else
					mvaddch (i, j, ACS_HLINE);
			} else if (j == x1 || j == x2) {
				mvaddch (i, j, ACS_VLINE);
			} else
			mvaddch (i, j, ' ');
		}	
	}
	attroff (WA_REVERSE);
}

// Draw main program header.
void UISupportDrawHeader (const char * headerstring) {
	clearLine (0, INVERSE);
	
	attron (WA_REVERSE);
	
	mvprintw (0, 1, "* Snownews %s", VERSION);
	if (headerstring != NULL) {
		if (strlen(headerstring) > COLS-18u)
			mvaddnstr (0, 19, headerstring, COLS-20);
		else
			mvaddstr (0, COLS-strlen(headerstring)-1, headerstring);
	}
	attroff (WA_REVERSE);
}

// Take a URL and execute in default browser.
// Apply all security restrictions before running system()!
void UISupportURLJump (const char * url) {
	int len;
	char *tmp;
	char *escapetext = NULL;
	char *syscall = NULL;
	
	// Should not happen. Nope, really should not happen.
	if (url == NULL)
		return;
	
	// Smartfeeds cannot be opened (for now).
	if (strncmp(url, "smartfeed", 9) == 0)
		return;
	
	// Complain loudly if browser string contains a single quote.
	if (strstr(browser, "'") != NULL)
		UIStatus (_("Unsafe browser string (contains quotes)! See snownews.kcore.de/faq#toc2.4"), 5, 1);
	
	// Discard url if it contains a single quote!
	if ((strstr(url, "'")) == NULL) {
		len = strlen(url) + 3;
		escapetext = malloc (len);
		snprintf (escapetext, len, "\'%s\'", url);
		len = strlen(browser) + len + 1;
		syscall = malloc (len);
		snprintf (syscall, len, browser, escapetext);
		free (escapetext);
	} else {
		UIStatus (_("Invalid URL passed. Possible shell exploit attempted!"), 3, 1);
		free (syscall);
		return;
	}
	
	len = strlen(syscall) + strlen(_("Executing %s")) + 1;
	tmp = malloc (len);
	snprintf (tmp, len, _("Executing %s"), syscall);
	UIStatus (tmp, 0, 0);
	free (tmp);
	
	len = len + 13;
	tmp = malloc (len);
	snprintf (tmp, len, "%s 2>/dev/null", syscall);

	// Switch on the cursor.
	curs_set(1);

	endwin();
	
	system(tmp);

	InitCurses();

	// Hide cursor again.
	if (!cursor_always_visible)
		curs_set(0);

	free (tmp);
	free (syscall);
}

void SmartFeedsUpdate (void) {
	struct feed *cur_ptr;
	
	// Find our smart feed.
	for (cur_ptr = first_ptr; cur_ptr != NULL; cur_ptr = cur_ptr->next_ptr) {
		if (cur_ptr->smartfeed == 1)
			SmartFeedNewitems (cur_ptr);
	}
}

void SmartFeedNewitems (struct feed * smart_feed) {
	struct feed *cur_feed;
	struct newsitem *new_item;
	struct newsitem *cur_item;
	
	// Be smart and don't leak the smart feed.
	// The items->data structures must not be freed, since a smart feed is only
	// a pointer collection and does not contain allocated memory.
	if (smart_feed->items != NULL) {
		while (smart_feed->items->next_ptr != NULL) {
			smart_feed->items = smart_feed->items->next_ptr;
			free (smart_feed->items->prev_ptr);
		}
		free (smart_feed->items);
	}

	// This must be NULL if there are no items.
	smart_feed->items = NULL;
	
	for (cur_feed = first_ptr; cur_feed != NULL; cur_feed = cur_feed->next_ptr) {
		// Do not add the smart feed recursively. 8)
		if (cur_feed != smart_feed) {
			for (cur_item = cur_feed->items; cur_item != NULL; cur_item = cur_item->next_ptr) {
				// If item is unread, add to smart feed.
				if (cur_item->data->readstatus == 0) {					
					new_item = malloc (sizeof (struct newsitem));
					new_item->data = cur_item->data;
					
					// Add to data structure.
					new_item->next_ptr = NULL;
					if (smart_feed->items == NULL) {
						new_item->prev_ptr = NULL;
						smart_feed->items = new_item;
					} else {
						new_item->prev_ptr = smart_feed->items;
						while (new_item->prev_ptr->next_ptr != NULL)
							new_item->prev_ptr = new_item->prev_ptr->next_ptr;
						new_item->prev_ptr->next_ptr = new_item;
					}
				}
			}
		}
	}
	
	// Only fill out once.
	if (smart_feed->title == NULL)
		smart_feed->title = strdup (_("(New headlines)"));
	if (smart_feed->link == NULL)
		smart_feed->link = strdup (smart_feed->feedurl);
}

int SmartFeedExists (const char * smartfeed) {
	struct feed *cur_ptr;
	
	// Find our smart feed.
	for (cur_ptr = first_ptr; cur_ptr != NULL; cur_ptr = cur_ptr->next_ptr) {
		if (strcmp(smartfeed, "newitems") == 0) {
			if (cur_ptr->smartfeed == 1)
				return 1;
		}
	}
	return 0;
}

void DrawProgressBar (int numobjects, int titlestrlen) {
	int i;
	
	attron (WA_REVERSE);
	for (i = 1; i <= numobjects; i++) {
		mvaddch (LINES-1, titlestrlen+i, '=');
	}
	mvaddch (LINES-1, COLS-3, ']');
	refresh();
	attroff(WA_REVERSE);
}

void displayErrorLog (void) {
	char errorlog[64];
	char command[96];
	char *pager = NULL;
	char *env;
	
	// Get the user's pager or default to less.
	env = getenv("PAGER");
	if (env)
		pager = strdup(env);
	else
		pager = strdup("less");
	
	env = getenv("HOME");
	snprintf (errorlog, sizeof(errorlog), "%s/.snownews/error.log", env);
	
	snprintf (command, sizeof(command), "%s %s", pager, errorlog);

	// Call as few as possible functions once we've left ncurses.
	endwin();
	system(command);
	InitCurses();
	
	clearLine (LINES-1, INVERSE);
	attron(WA_REVERSE);
	mvaddnstr (LINES-1, 1, "Press any key to exit the error log display.", COLS-2);
	attroff(WA_REVERSE);
	
	free (pager);
	
	getch();
}

void clearLine (int line, clear_line how) {
	int i;
	
	if (how == INVERSE)
		attron(WA_REVERSE);
	
	for (i = 0; i < COLS; i++) {
		mvaddch (line, i, ' ');
	}
	
	if (how == INVERSE)
		attron(WA_REVERSE);
}
