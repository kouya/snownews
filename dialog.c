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

#include <ncurses.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "interface.h"
#include "main.h"
#include "categories.h"
#include "ui-support.h"
#include "xmlparse.h"
#include "filters.h"
#include "dialog.h"
#include "io-internal.h"
#include "conversions.h"

extern char *browser;
extern struct keybindings keybindings;
extern struct categories *first_category;
extern struct color color;
 
extern int cursor_always_visible;

char * UIOneLineEntryField (int x, int y) {
	char *text;

	text = malloc(512);
        
	// UIStatus switches off attron!
	attron (WA_REVERSE);
	echo();
	curs_set(1);

	move (y, x);
	// Beware of hardcoded textlength size!
	// getnstr size does NOT include \0. This is really stupid and causes
	// 1-byte overflows. Always size=len-1!
	getnstr (text, 511);

	noecho();
	if (!cursor_always_visible)
		curs_set(0);
	attroff (WA_REVERSE);

	// This memory needs to be freed in the calling function!
	return text;
}

void UIChangeBrowser (void) {
	char *browserstring;
	char *fmtstr;
	int len;
	
	repeat:
	
	// malloc = 17 (strlen("Current setting: ") + browser
	// We will malloc a bigger junk, because other languages
	// might need longer strings and crash!
	len = strlen(_("Current setting: %s")) + strlen(browser) + 1;
	browserstring = malloc (len);
	snprintf (browserstring, len, _("Current setting: %s"), browser);

	// Clear screen area we want to "draw" to.
	attron (WA_REVERSE);
	UISupportDrawBox (3, 5, COLS-4, 7);
		
	UIStatus (browserstring, 0, 0);
	free (browserstring);
	
	browserstring = UIOneLineEntryField (5, 6);
	
	if (strlen(browserstring) == 0) {
		free (browserstring);
		UIStatus (_("Aborted."), 1, 0);
		return;
	}
	
	fmtstr = strstr (browserstring, "%");
	if ((fmtstr != NULL) && (fmtstr[1] != 's')) {
		free (browserstring);
		UIStatus (_("Only %s format strings allowed for default browser!"), 3, 1);
		goto repeat;
	}
	
	if (strstr(browserstring, "'") != NULL)
		UIStatus (_("Unsafe browser string (contains quotes)! See snownews.kcore.de/faq#toc2.4"), 3, 1);
	
	browser = realloc (browser, strlen(browserstring)+1);
	strncpy (browser, browserstring, strlen(browserstring)+1);
	
	free (browserstring);
}

// Dialog to change feedname.
// Return: 0	on success
//	   1	on user abort
//	   2	original title restored
void UIChangeFeedName (struct feed *cur_ptr) {
	char *newname;

	// Clear screen area we want to "draw" to.
	attron (WA_REVERSE);
	UISupportDrawBox (3, 5, COLS-4, 7);

	UIStatus (_("Enter new name. Blank line to abort. '-' to reset."), 0, 0);
	
	newname = UIOneLineEntryField (5, 6);
	
	// If strlen is zero, return 1.
	if (strlen(newname) == 0) {
		free (newname);
		return;
	}
	
	// If newname contains "|", abort since this is used as a delimiter for the config file.
	if (strstr (newname, "|") != NULL) {
		free (newname);
		UIStatus (_("The new title must not contain a \"|\" character!"), 2, 0);
		return ;
	}
	
	// Restor original title.
	if ((newname != NULL) && (cur_ptr->override != NULL)) {
		if (strcmp(newname, "-") == 0) {
			if (cur_ptr->title != NULL)
				free (cur_ptr->title);
			cur_ptr->title = strdup(cur_ptr->original);
			free (cur_ptr->original);
			// Set back original to NULL pointer.
			cur_ptr->original = NULL;
			free (cur_ptr->override);
			cur_ptr->override = NULL;
			free (newname);
			return;
		}
	}
	
	// Copy new name into ->override.
	free (cur_ptr->override);
	cur_ptr->override = strdup (newname);
	
	// Save original.
	free (cur_ptr->original);
	cur_ptr->original = strdup (cur_ptr->title);
	
	// Set new title.
	free (cur_ptr->title);
	cur_ptr->title = strdup (newname);
		
	free (newname);
	return;
}

// Popup window to add new RSS feed. Passing an URL will
// automatically add it, no questions asked.
int UIAddFeed (char * newurl) {
	char tmp[512];
	char *url;
	struct feed *new_ptr;

	if (newurl == NULL) {
		// Clear screen area we want to "draw" to.
		attron (WA_REVERSE);
		UISupportDrawBox (3, 5, COLS-4, 7);

		UIStatus (_("Enter URL of the feed you want to add. Blank line to abort."), 0, 0);

		url = UIOneLineEntryField (5, 6);

		// If read stringlength is ZARO (abort of action requested) return 1
		// and confuse the calling function.
		if (strlen(url) == 0) {
			free (url);
			return 1;
		}
		
		CleanupString(url, 0);
		
		// Support that stupid feed:// "protocol"
		if (strncasecmp (url, "feed://", 7) == 0)
			memcpy (url, "http", 4);
		
		// If URL does not start with the procotol specification,
		// assume http://
		// -> tmp[512] -> we can "only" use max 504 chars from url ("http://" == 7).
		if ((strncasecmp (url, "http://", 7) != 0) &&
			(strncasecmp (url, "https://", 8) != 0) &&
			(strncasecmp (url, "exec:", 5) != 0)) {
			if (strlen (url) < 504) {
				strcpy (tmp, "http://");
				strncat (tmp, url, 504);
				free (url);
				url = strdup (tmp);
			} else {
				free (url);
				return 2;
			}
		}
	} else
		url = strdup (newurl);

	new_ptr = newFeedStruct();
	
	// getnstr does not return newline... says the docs.
	new_ptr->feedurl = malloc (strlen(url)+1);
	
	// Attach to feed pointer chain.
	strncpy (new_ptr->feedurl, url, strlen(url)+1);
	new_ptr->next_ptr = first_ptr;
	if (first_ptr != NULL)
		first_ptr->prev_ptr = new_ptr;
	new_ptr->prev_ptr = NULL;
	first_ptr = new_ptr;
	
	// Tag execurl.
	if (strncasecmp (url, "exec:", 5) == 0)
		new_ptr->execurl = 1;
	
	if (strncasecmp (url, "smartfeed:", 10) == 0)
		new_ptr->smartfeed = 1;
		
	// Don't need url text anymore.
	free (url);

	// Download new feed and DeXMLize it. */	
	if ((UpdateFeed (new_ptr)) != 0) {
		UIStatus (_("The feed could not be parsed. Do you need a filter script for this feed? (y/n)"), 0, 0);
		
		if (getch() == 'n')
			return -1;
		else {
			UIPerFeedFilter (new_ptr);
			FilterPipeNG (new_ptr);
			if ((DeXML (new_ptr)) != 0)
				return -1;
			else {
				new_ptr->problem = 0;
				return 0;
			}
		}
	}
	return 0;
}

void FeedInfo (struct feed * current_feed) {
	int centerx, len;
	char *hashme;	// Hashed filename.
	char *file;
	char *categories = NULL;
	char *url;	// feedurl - authinfo.
	char *tmp = NULL;
	char *tmp2;
	struct stat filetest;
	
	url = strdup (current_feed->feedurl);
	
	// Remove authinfo from URL.
	tmp = strstr (url, "@");
	if (tmp != NULL) {
		tmp2 = strstr (url, "://");
		tmp2 += 3;
		memmove (tmp2, tmp+1, strlen(tmp));
	}
	
	centerx = COLS / 2;
	
	hashme = Hashify(current_feed->feedurl);
	len = (strlen(getenv("HOME")) + strlen(hashme) + 18);
	file = malloc (len);
	snprintf (file, len, "%s/.snownews/cache/%s", getenv("HOME"), hashme);

	UISupportDrawBox (5, 4, COLS-6, 13);
	
	attron (WA_REVERSE);
	mvaddstr (5, centerx-((strlen(current_feed->title))/2), current_feed->title);
	mvaddnstr (7, centerx-(COLS/2-7), url, COLS-14);
	if (current_feed->lastmodified != NULL)
		mvprintw (8, centerx-(COLS/2-7), _("Last updated: %s"), current_feed->lastmodified);	
	else
		mvaddstr (8, centerx-(COLS/2-7), _("No modification date."));
	
	if ((stat (file, &filetest)) != -1) {
		mvprintw (9, centerx-(COLS/2-7), _("In disk cache: %lld bytes"), (long long int) filetest.st_size);
	} else
		mvaddstr (9, centerx-(COLS/2-7), _("Not in disk cache."));
		
	// Print category info
	mvaddstr (10, centerx-(COLS/2-7), _("Categories:"));
	if (current_feed->feedcategories == NULL)
		mvaddstr (10, centerx-(COLS/2-7)+strlen(_("Categories:"))+1, _("none"));
	else {
		categories = GetCategoryList (current_feed);
		mvprintw (10, centerx-(COLS/2-7)+strlen(_("Categories:"))+1, categories);
		free (categories);
	}
	
	// Tell user if feed uses auth, but don't display the string.
	if (tmp != NULL)
		mvaddstr (11, centerx-(COLS/2-7), _("Feed uses authentication."));
	else
		mvaddstr (11, centerx-(COLS/2-7), _("Feed does not use authentication."));
	
	// Add a smiley indicator to the http status telling the overall status
	// so you don't have to know what the HTTP return codes mean.
	// Yes I think I got the idea from cdparanoia. :)
	if (current_feed->lasthttpstatus != 0) {
		if (current_feed->content_type == NULL) {
			len = strlen(_("Last webserver status: %d"));
			mvprintw (12, centerx-(COLS/2-7), _("Last webserver status: %d"), current_feed->lasthttpstatus);
		} else {
			len = strlen(_("Last webserver status: (%s) %d")) + strlen(current_feed->content_type) - 2; // -2 == %s
			mvprintw (12, centerx-(COLS/2-7), _("Last webserver status: (%s) %d"),
				current_feed->content_type, current_feed->lasthttpstatus);
		}
	
		switch (current_feed->lasthttpstatus) {
			case 200:
			case 304:
				mvaddstr (12, centerx-(COLS/2-7)+len+2, ":-)");
				break;
			case 401:
			case 403:
			case 500:
			case 503:
				mvaddstr (12, centerx-(COLS/2-7)+len+2, ":-P");
				break;
			case 404:
			case 410:
				mvaddstr (12, centerx-(COLS/2-7)+len+2, ":-(");
				break;
			default:
				mvaddstr (12, centerx-(COLS/2-7)+len+2, "8-X");
				break;
		}
	}
	
	// Display filter script if any.
	if (current_feed->perfeedfilter != NULL) {
		UISupportDrawBox (5, 13, COLS-6, 14);
		attron (WA_REVERSE);
		mvaddstr (13, 7, _("Filtered through:"));
		mvaddnstr (13, 7+strlen(_("Filtered through:"))+1, current_feed->perfeedfilter, COLS-14-strlen(_("Filtered through:")));
	}
	
	UIStatus (_("Displaying feed information."), 0, 0);
	
	free (file);
	free (hashme);
	free (url);

	// Wait for the any key.
	getch();
}

int UIDeleteFeed (char * feedname) {
	
	UISupportDrawBox (3, 5, COLS-3, 8);
	
	attron (WA_REVERSE);
	mvaddstr (6, COLS/2-21, _("Are you sure you want to delete this feed?"));
	mvprintw (7, 5, "%s", feedname);
	
	UIStatus (_("Type 'y' to delete, any other key to abort."), 0, 0);
	
	if (getch() == 'y')
		return 1;
	else
		return 0;
}

void UIHelpScreen (void) {
	int centerx, centery;				// Screen center x/y coordinate.
	int userinput;
	int offset = 18;
	int offsetstr = 12;
	
	centerx = COLS / 2;
	centery = LINES / 2;
	
	UISupportDrawBox ((COLS/2)-20, (LINES/2)-10, (COLS/2)+24, (LINES/2)+10);
	
	attron (WA_REVERSE);
	// Keys
	mvprintw (centery-9, centerx-offset, "%c:", keybindings.addfeed);	
	mvprintw (centery-8, centerx-offset, "%c:", keybindings.deletefeed);
	mvprintw (centery-7, centerx-offset, "%c:", keybindings.changefeedname);
	mvprintw (centery-6, centerx-offset, "%c:", keybindings.reloadall);
	mvprintw (centery-5, centerx-offset, "%c:", keybindings.reload);
	mvprintw (centery-4, centerx-offset, "%c:", keybindings.markallread);
	mvprintw (centery-3, centerx-offset, "%c:", keybindings.dfltbrowser);
	mvprintw (centery-2, centerx-offset, "%c, %c:", keybindings.moveup, keybindings.movedown);
	mvprintw (centery-1, centerx-offset, "%c:", keybindings.sortfeeds);
	mvprintw (centery,   centerx-offset, "%c:", keybindings.categorize);
	mvprintw (centery+1, centerx-offset, "%c:", keybindings.filter);
	mvprintw (centery+2, centerx-offset, "%c:", keybindings.filtercurrent);
	mvprintw (centery+3, centerx-offset, "%c:", keybindings.nofilter);
	mvprintw (centery+4, centerx-offset, "%c:", keybindings.newheadlines);
	mvprintw (centery+5, centerx-offset, "%c:", keybindings.perfeedfilter);
	mvaddstr (centery+6, centerx-offset, _("tab:"));
	mvprintw (centery+7, centerx-offset, "%c:", keybindings.about);
	mvprintw (centery+8, centerx-offset, "%c:", 'E');
	mvprintw (centery+9, centerx-offset, "%c:", keybindings.quit);
	// Descriptions
	mvaddstr (centery-9, centerx-offsetstr, _("Add RSS feed..."));
	mvaddstr (centery-8, centerx-offsetstr, _("Delete highlighted RSS feed..."));
	mvaddstr (centery-7, centerx-offsetstr, _("Rename feed..."));
	mvaddstr (centery-6, centerx-offsetstr, _("Reload all feeds"));
	mvaddstr (centery-5, centerx-offsetstr, _("Reload this feed"));
	mvaddstr (centery-4, centerx-offsetstr, _("Mark all read"));
	mvaddstr (centery-3, centerx-offsetstr, _("Change default browser..."));
	mvaddstr (centery-2, centerx-offsetstr, _("Move item up, down"));
	mvaddstr (centery-1, centerx-offsetstr, _("Sort feed list alphabetically"));
	mvaddstr (centery,   centerx-offsetstr, _("Categorize feed..."));
	mvaddstr (centery+1, centerx-offsetstr, _("Apply filter..."));
	mvaddstr (centery+2, centerx-offsetstr, _("Only current category"));
	mvaddstr (centery+3, centerx-offsetstr, _("Remove filter"));
	mvaddstr (centery+4, centerx-offsetstr, _("Show new headlines"));
	mvaddstr (centery+5, centerx-offsetstr, _("Add conversion filter..."));
	mvaddstr (centery+6, centerx-offsetstr, _("Type Ahead Find"));
	mvaddstr (centery+7, centerx-offsetstr, _("About"));
	mvaddstr (centery+8, centerx-offsetstr, _("Show error log..."));
	mvaddstr (centery+9, centerx-offsetstr, _("Quit program"));
	attroff (WA_REVERSE);

	UIStatus (_("Press the any(tm) key to exit help screen."), 0, 0);
	userinput = getch();
	
	// Return input back into input queue so it gets automatically executed.
	if ((userinput != '\n') && (userinput != 'h') && (userinput != 'q'))
		ungetch(userinput);
}

void UIDisplayFeedHelp (void) {
	int centerx, centery;				// Screen center x/y coordinate.
	int userinput;
	int offset = 18;
	int offsetstr = 7;
	
	centerx = COLS / 2;
	centery = LINES / 2;
	
	UISupportDrawBox ((COLS/2)-20, (LINES/2)-6, (COLS/2)+24, (LINES/2)+7);
	
	attron (WA_REVERSE);
	// Keys
	mvprintw (centery-5, centerx-offset, _("%c, up:"), keybindings.prev);	
	mvprintw (centery-4, centerx-offset, _("%c, down:"), keybindings.next);
	mvaddstr (centery-3, centerx-offset, _("enter:"));
	mvprintw (centery-2, centerx-offset, "%c:", keybindings.reload);
	mvprintw (centery-1, centerx-offset, "%c:", keybindings.forcereload);
	mvprintw (centery,   centerx-offset, "%c:", keybindings.urljump);
	mvprintw (centery+1, centerx-offset, "%c:", keybindings.urljump2);
	mvprintw (centery+2, centerx-offset, "%c:", keybindings.markread);
	mvprintw (centery+3, centerx-offset, "%c:", keybindings.markunread);
	mvprintw (centery+4, centerx-offset, "%c:", keybindings.feedinfo);
	mvaddstr (centery+5, centerx-offset, _("tab:"));
	mvprintw (centery+6, centerx-offset, "%c:", keybindings.prevmenu);
	// Descriptions
	mvprintw (centery-5, centerx-offsetstr, _("Previous item"));	
	mvprintw (centery-4, centerx-offsetstr, _("Next item"));
	mvaddstr (centery-3, centerx-offsetstr, _("View item"));
	mvprintw (centery-2, centerx-offsetstr, _("Reload this feed"));
	mvprintw (centery-1, centerx-offsetstr, _("Force reload this feed"));
	mvprintw (centery,   centerx-offsetstr, _("Open homepage"));
	mvprintw (centery+1, centerx-offsetstr, _("Open link"));
	mvprintw (centery+2, centerx-offsetstr, _("Mark all read"));
	mvprintw (centery+3, centerx-offsetstr, _("Toggle item read status"));
	mvprintw (centery+4, centerx-offsetstr, _("Show feed info..."));
	mvaddstr (centery+5, centerx-offsetstr, _("Type Ahead Find"));
	mvprintw (centery+6, centerx-offsetstr, _("Return to main menu"));
	attroff (WA_REVERSE);

	UIStatus (_("Press the any(tm) key to exit help screen."), 0, 0);
	userinput = getch();
	if ((userinput != '\n') && (userinput != 'h'))
		ungetch(userinput);
}

void UIDisplayItemHelp (void) {
	int centerx, centery;				// Screen center x/y coordinate.
	int userinput;
	int offset = 16;
	int offsetstr = 6;
	
	centerx = COLS / 2;
	centery = LINES / 2;
	
	UISupportDrawBox ((COLS/2)-18, (LINES/2)-2, (COLS/2)+18, (LINES/2)+3);
	
	attron (WA_REVERSE);
	// Keys
	mvprintw (centery-1, centerx-offset, "%c, <-:", keybindings.prev);	
	mvprintw (centery,   centerx-offset, "%c, ->:", keybindings.next);
	mvprintw (centery+1, centerx-offset, "%c:", keybindings.urljump);
	mvprintw (centery+2, centerx-offset, _("%c, enter:"), keybindings.prevmenu);
	// Descriptions
	mvaddstr (centery-1, centerx-offsetstr, _("Previous item"));	
	mvaddstr (centery,   centerx-offsetstr, _("Next item"));
	mvaddstr (centery+1, centerx-offsetstr, _("Open link"));
	mvaddstr (centery+2, centerx-offsetstr, _("Return to overview"));
	attroff (WA_REVERSE);

	UIStatus (_("Press the any(tm) key to exit help screen."), 0, 0);
	userinput = getch();
	if ((userinput != '\n') && (userinput != 'h'))
		ungetch(userinput);
}


// Add/remove categories for given feed. This takes over the main interface while running.
void CategorizeFeed (struct feed * current_feed) {
	int i, n, nglobal, y;
	int uiinput;
	char tmp[512];
	char *newcategory;
	struct feedcategories *category;
	struct categories *cur_ptr;
	
	// Return if we got passed a NULL pointer (no feeds added to main program).
	if (current_feed == NULL)
		return;
		
	// Determine number of global categories.
	nglobal = 0;
	for (cur_ptr = first_category; cur_ptr != NULL; cur_ptr = cur_ptr->next_ptr) {
		nglobal++;
	}

	// We're taking over the program!
	while (1) {
		// Determine number of categories for current_feed.
		n = 0;
		for (category = current_feed->feedcategories; category != NULL; category = category->next_ptr) {
			n++;
		}

		UISupportDrawBox ((COLS/2)-37, 2, (COLS/2)+37, 2+3+nglobal+1);
		
		attron (WA_REVERSE);
		mvprintw (3, (COLS/2)-((strlen(_("Category configuration for %s"))+strlen(current_feed->title))/2),
			_("Category configuration for %s"), current_feed->title);
		

		// No category defined yet
		if (current_feed->feedcategories == NULL) {
			i = 49;
			y = 5;
			mvaddstr (y, (COLS/2)-33, _("No category defined. Select one or add a new category:"));
			for (cur_ptr = first_category; cur_ptr != NULL; cur_ptr = cur_ptr->next_ptr) {
				mvprintw (y+1, (COLS/2)-33, "%c. %s", i, cur_ptr->name);
				y++;
				i++;
				// Fast forward to 'a' if we hit ASCII 58 ':'
				if (i == 58)
					i += 39;
			}
			snprintf (tmp, sizeof(tmp), _("Select category number to add, press 'A' to add a new category or '%c' to quit."), keybindings.quit);
			UIStatus (tmp, 0, 0);
		} else {
			i = 49;
			y = 5;
			mvaddstr (y, (COLS/2)-33, _("Categories defined for this feed:"));
			for (category = current_feed->feedcategories; category != NULL; category = category->next_ptr) {
				mvprintw (y+1, (COLS/2)-33, "%c. %s", i, category->name);
				y++;
				i++;
				// Fast forward to 'a' if we hit ASCII 58 ':'
				if (i == 58)
					i += 39;
			}
			snprintf (tmp, sizeof(tmp), _("Select a category number to delete, press 'A' to add a new one or '%c' to quit."), keybindings.quit);
			UIStatus (tmp, 0, 0);
		}
		
		refresh();
		
		uiinput = getch();
		if (uiinput == keybindings.quit)
			return;
		if (uiinput == 'A') {
			if ((n > 0) && (nglobal > 0)) {
				// Clear screen area we want to "draw" to.
				UISupportDrawBox ((COLS/2)-37, 5, (COLS/2)+37, 2+3+nglobal+1);
			
				attron (WA_REVERSE);
				i = 49;
				y = 5;
				mvaddstr (y, (COLS/2)-33, _("Select a new category or add a new one:"));
				for (cur_ptr = first_category; cur_ptr != NULL; cur_ptr = cur_ptr->next_ptr) {
					mvprintw (y+1, (COLS/2)-33, "%c. %s", i, cur_ptr->name);
					y++;
					i++;
					// Fast forward to 'a' if we hit ASCII 58 ':'
					if (i == 58)
						i += 39;
				}
				snprintf (tmp, sizeof(tmp), _("Select category number to add, press 'A' to add a new category or '%c' to quit."), keybindings.quit);
				UIStatus (tmp, 0, 0);
				
				uiinput = getch();
				
			}
			
			if (uiinput == 'A') {
				// Clear screen area we want to "draw" to.
				UISupportDrawBox ((COLS/2)-37, 5, (COLS/2)+37, 2+3+nglobal+1);

				attron (WA_REVERSE);
				UIStatus (_("Enter new category name."), 0, 0);
				newcategory = UIOneLineEntryField ((COLS/2)-33, 5);
				if (strlen(newcategory) == 0)
					UIStatus (_("Aborted."), 1, 0);
				else {
					if (!FeedCategoryExists (current_feed, newcategory))
						FeedCategoryAdd (current_feed, newcategory);
					else
						UIStatus (_("Category already defined for this feed!"), 1, 0);
				}
				free (newcategory);
			} else {
				// To add category below.
				n = 0;
			}
		}
		// If uiinput is 1-9,a-z
		// If uiinput < i (ASCII of max selectable element) ignore event.
		if ((((uiinput >= 49) && (uiinput <= 57)) ||
			((uiinput >= 97) && (uiinput <= 122))) &&
			(uiinput < i)) {
			
			if (uiinput <= 57)
				i = uiinput - 48;
			else
				i = uiinput - 87;
			
			if (n > 0) {
				// Delete category code for n (number of categories defined for current feed) > 0
				// Decrease i by one while walking the category linked list.
				// If we hit 0, break. category->name will now contain the category we want to remove.
				for (category = current_feed->feedcategories; category != NULL; category = category->next_ptr) {
					if (i == 1)
						break;
					i--;
				}
			
				// Delete entry from feed categories.
				FeedCategoryDelete (current_feed, category->name);
			} else {
				// If n == 0 add category with number i to feed.
				for (cur_ptr = first_category; cur_ptr != NULL; cur_ptr = cur_ptr->next_ptr) {
					if (i == 1)
						break;
					i--;
				}
				
				// Add category to current feed
				FeedCategoryAdd (current_feed, cur_ptr->name);
			}
		}
	}
}

// Allocates and returns a filter string the user has chosen from the list of
// available categories.
// If no filter is chosen a NULL pointer is returned.
char * DialogGetCategoryFilter (void) {
	int i, nglobal, y;
	char *filterstring = NULL;
	int uiinput;
	struct categories *cur_ptr;
	
	// Determine number of global categories.
	nglobal = 0;
	for (cur_ptr = first_category; cur_ptr != NULL; cur_ptr = cur_ptr->next_ptr) {
		nglobal++;
	}
	
	UISupportDrawBox ((COLS/2)-35, 2, (COLS/2)+35, nglobal+4);
	
	attron (WA_REVERSE);
	mvaddstr (3, (COLS/2)-(strlen(_("Select filter to apply"))/2), _("Select filter to apply"));
	
	i = 49;
	y = 3;
	for (cur_ptr = first_category; cur_ptr != NULL; cur_ptr = cur_ptr->next_ptr) {
		mvprintw (y+1, (COLS/2)-33, "%c. %s", i, cur_ptr->name);
		y++;
		i++;
		// Fast forward to 'a' if we hit ASCII 58 ':'
		if (i == 58)
			i += 39;
	}
			
	UIStatus (_("Select a category number or any other key to reset filters."), 0, 0);
	
	refresh();
		
	uiinput = getch();

	// If uiinput is 1-9,a-z
	// If uiinput < i (ASCII of max selectable element) ignore event.
	if ((((uiinput >= 49) && (uiinput <= 57)) ||
		((uiinput >= 97) && (uiinput <= 123))) &&
		(uiinput < i)) {
		
		if (uiinput <= 57)
			i = uiinput - 48;
		else
			i = uiinput - 87;
		
		for (cur_ptr = first_category; cur_ptr != NULL; cur_ptr = cur_ptr->next_ptr) {
			if (i == 1)
				break;
			i--;
		}
		filterstring = strdup (cur_ptr->name);
	}
	
	return filterstring;
}

int UIPerFeedFilter (struct feed * current_feed) {
	char *newstring = NULL;
	
	if (current_feed->smartfeed != 0)
		return -1;

	// Clear screen area we want to "draw" to.
	attron (WA_REVERSE);
	UISupportDrawBox (3, 5, COLS-4, 7);

	UIStatus (_("Enter script to pipe feed through."), 0, 0);
	
	newstring = UIOneLineEntryField (5, 6);
	
	// If strlen is zero, return 1.
	if (strlen(newstring) == 0) {
		free (newstring);
		return 1;
	}
	
	// If newname contains "|", abort since this is used as a delimiter for the config file.
	if (strstr (newstring, "|") != NULL) {
		free (newstring);
		return 3;
	}
	
	free (current_feed->perfeedfilter);
	current_feed->perfeedfilter = strdup (newstring);
	
	free (newstring);
	return 0;
}
