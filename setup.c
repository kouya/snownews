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
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "config.h"

#include "cookies.h"
#include "categories.h"
#include "ui-support.h"
#include "main.h"
#include "setup.h"
#include "io-internal.h"

struct feed *first_ptr = NULL;
struct entity *first_entity = NULL;

struct keybindings keybindings;
struct color color;

char *browser;			// Browser command. lynx is standard.
char *proxyname;		// Hostname of proxyserver.
char *useragent;		// Snownews User-Agent string.
unsigned short proxyport = 0;	// Port on proxyserver to use.
int use_colors = 1;		// Default to enabled.

// Load browser command from ~./snownews/browser.
void SetupBrowser (const char * file) {
	char filebuf[256];
	FILE *configfile;

	configfile = fopen (file, "r");
	if (configfile == NULL) {
		UIStatus (_("Creating new config \"browser\"..."), 0, 0);
		configfile = fopen (file, "w+");
		if (configfile == NULL)
			MainQuit (_("Create initial configfile \"config\""), strerror(errno)); // Still didn't work?
		browser = strdup("lynx %s");
		fputs (browser, configfile);
		fclose (configfile);
	} else {
		// Careful not to overflow char browser!
		fgets (filebuf, sizeof(filebuf), configfile);
		browser = strdup(filebuf);
		fclose (configfile);
		// Die newline, die!
		if (browser[strlen(browser)-1] == '\n')
			browser[strlen(browser)-1] = '\0';
	}
}


// Parse http_proxy environment variable and define proxyname and proxyport.
void SetupProxy (void) {
	char *freeme;
	char *proxystring;
	char *tmp;
	
	// Check for proxy environment variable.
	if (getenv("http_proxy") != NULL) {
		// The pointer returned by getenv must not be altered.
		// What about mentioning this in the manpage of getenv?
		proxystring = strdup(getenv("http_proxy"));
		freeme = proxystring;
		strsep (&proxystring, "/");
		if (proxystring != NULL) {
			strsep (&proxystring, "/");
		} else {
			free (freeme);
			return;
		}
		if (proxystring != NULL) {
			tmp = strsep (&proxystring, ":");
			proxyname = strdup(tmp);
		} else {
			free (freeme);
			return;
		}
		if (proxystring != NULL) {
			proxyport = atoi (strsep (&proxystring, "/"));
		} else {
			free (freeme);
			return;
		}
		free (freeme);
	}
}


// Construct the user agent string that snownews sends to the webserver.
// This includes Snownews/VERSION, OS name and language setting.
void SetupUserAgent (void) {
	char *lang;
	char url[] = "http://snownews.kcore.de/";
	int ualen, urllen;

	urllen = strlen(url);
	
	// Constuct the User-Agent string of snownews. This is done here in program init,
	// because we need to do it exactly once and it will never change while the program
	// is running.
	if (getenv("LANG") != NULL) {
		lang = getenv("LANG");
		// Snonews/VERSION (Linux; de_DE; (http://kiza.kcore.de/software/snownews/)
		ualen = strlen("Snownews/") + strlen(SNOWNEWS_VERSION) + 2 + strlen(lang) + 2 + strlen(OS)+2 + urllen + 2;
		useragent = malloc(ualen);
		snprintf (useragent, ualen, "Snownews/" SNOWNEWS_VERSION " (" OS "; %s; %s)", lang, url);
	} else {
		// "Snownews/" + VERSION + "(http://kiza.kcore.de/software/snownews/)"
		ualen = strlen("Snownews/") + strlen(SNOWNEWS_VERSION) + 2 + strlen(OS) + 2 + urllen + 2;
		useragent = malloc(ualen);
		snprintf (useragent, ualen, "Snownews/" SNOWNEWS_VERSION " (" OS "; %s)", url);
	}
}

// Define global keybindings and load user customized bindings.
void SetupKeybindings (const char * file) {
	char filebuf[128];
	char *freeme, *tmpbuf;
	FILE *configfile;

	// Define default values for keybindings. If some are defined differently
	// in the keybindings file they will be overwritten. If some are missing or broken/wrong
	// these are sane defaults.
	keybindings.next = 'n';
	keybindings.prev = 'p';
	keybindings.prevmenu = 'q';
	keybindings.quit = 'q';
	keybindings.addfeed = 'a';
	keybindings.deletefeed = 'D';
	keybindings.markread = 'm';
	keybindings.markallread = 'm';
	keybindings.markunread = 'M';
	keybindings.dfltbrowser = 'B';
	keybindings.moveup = 'P';
	keybindings.movedown = 'N';
	keybindings.feedinfo = 'i';
	keybindings.reload = 'r';
	keybindings.reloadall = 'R';
	keybindings.forcereload = 'T';
	keybindings.urljump = 'o';
	keybindings.urljump2 = 'O';
	keybindings.changefeedname = 'c';
	keybindings.sortfeeds = 's';
	keybindings.pup = 'b';
	keybindings.pdown = ' ';
	keybindings.home = '<';
	keybindings.end = '>';
	keybindings.categorize = 'C';
	keybindings.filter = 'f';
	keybindings.filtercurrent = 'g';
	keybindings.nofilter = 'F';
	keybindings.help = 'h';
	keybindings.about = 'A';
	keybindings.perfeedfilter = 'e';
	keybindings.andxor = 'X';
	keybindings.enter = 'l';
	keybindings.newheadlines = 'H';
	keybindings.typeahead = '/';
	
	configfile = fopen (file, "r");

	if (configfile != NULL) {
		// Read keybindings and populate keybindings struct.
		while (!feof(configfile)) {
			if ((fgets (filebuf, sizeof(filebuf), configfile)) == NULL)
				break;
			if (strstr(filebuf, "#") != NULL)
				continue;
			tmpbuf = strdup(filebuf);	// strsep only works on *ptr
			freeme = tmpbuf;		// Save start pos. This is also the string we need to compare. strsep will \0 the delimiter.
			strsep (&tmpbuf, ":");

			// Munch trailing newline and avoid \n being bound to a function if no key is defined.
			if (tmpbuf != NULL)
				tmpbuf[strlen(tmpbuf)-1] = 0;

			if (strcmp (freeme, "next item") == 0)
				keybindings.next = tmpbuf[0];
			else if (strcmp (freeme, "previous item") == 0)
				keybindings.prev = tmpbuf[0];
			else if (strcmp (freeme, "return to previous menu") == 0)
				keybindings.prevmenu = tmpbuf[0];
			else if (strcmp (freeme, "quit") == 0)
				keybindings.quit = tmpbuf[0];
			else if (strcmp (freeme, "add feed") == 0)
				keybindings.addfeed = tmpbuf[0];
			else if (strcmp (freeme, "delete feed") == 0)
				keybindings.deletefeed = tmpbuf[0];
			else if (strcmp (freeme, "mark feed as read") == 0)
				keybindings.markread = tmpbuf[0];
			else if (strcmp (freeme, "mark item unread") == 0)
				keybindings.markunread = tmpbuf[0];
			else if (strcmp (freeme, "mark all as read") == 0)
				keybindings.markallread = tmpbuf[0];
			else if (strcmp (freeme, "change default browser") == 0)
				keybindings.dfltbrowser = tmpbuf[0];
			else if (strcmp (freeme, "sort feeds") == 0)
				keybindings.sortfeeds = tmpbuf[0];
			else if (strcmp (freeme, "move item up") == 0)
				keybindings.moveup = tmpbuf[0];
			else if (strcmp (freeme, "move item down") == 0)
				keybindings.movedown = tmpbuf[0];
			else if (strcmp (freeme, "show feedinfo") == 0)
				keybindings.feedinfo = tmpbuf[0];
			else if (strcmp (freeme, "reload feed") == 0)
				keybindings.reload = tmpbuf[0];
			else if (strcmp (freeme, "force reload feed") == 0)
				keybindings.forcereload = tmpbuf[0];
			else if (strcmp (freeme, "reload all feeds") == 0)
				keybindings.reloadall = tmpbuf[0];
			else if (strcmp (freeme, "open url") == 0)
				keybindings.urljump = tmpbuf[0];
			else if (strcmp (freeme, "open item url in overview") == 0)
				keybindings.urljump2 = tmpbuf[0];
			else if (strcmp (freeme, "change feedname") == 0)
				keybindings.changefeedname = tmpbuf[0];
			else if (strcmp (freeme, "page up") == 0)
				keybindings.pup = tmpbuf[0];
			else if (strcmp (freeme, "page down") == 0)
				keybindings.pdown = tmpbuf[0];
			else if (strcmp (freeme, "top") == 0)
				keybindings.home = tmpbuf[0];
			else if (strcmp (freeme, "bottom") == 0)
				keybindings.end = tmpbuf[0];
			else if (strcmp (freeme, "categorize feed") == 0)
				keybindings.categorize = tmpbuf[0];
			else if (strcmp (freeme, "apply filter") == 0)
				keybindings.filter = tmpbuf[0];
			else if (strcmp (freeme, "only current category") == 0)
				keybindings.filtercurrent = tmpbuf[0];
			else if (strcmp (freeme, "remove filter") == 0)
				keybindings.nofilter = tmpbuf[0];
			else if (strcmp (freeme, "per feed filter") == 0)
				keybindings.perfeedfilter = tmpbuf[0];
			else if (strcmp (freeme, "help menu") == 0)
				keybindings.help = tmpbuf[0];
			else if (strcmp (freeme, "about") == 0)
				keybindings.about = tmpbuf[0];
			else if (strcmp (freeme, "toggle AND/OR filtering") == 0)
				keybindings.andxor = tmpbuf[0];
			else if (strcmp (freeme, "enter") == 0)
				keybindings.enter = tmpbuf[0];
			else if (strcmp (freeme, "show new headlines") == 0)
				keybindings.newheadlines = tmpbuf[0];
			else if (strcmp (freeme, "type ahead find") == 0)
				keybindings.typeahead = tmpbuf[0];
			
			free (freeme);
		}
		
		// Override old default settings and make sure there is no clash.
		// Default browser is now B; b moved to page up.
		if (keybindings.dfltbrowser == 'b') {
			keybindings.dfltbrowser = 'B';
		}
		fclose (configfile);
	}

	configfile = fopen (file, "w+");
	fputs ("# Snownews keybindings configfile\n", configfile);
	fputs ("# Main menu bindings\n", configfile);
	fprintf (configfile, "add feed:%c\n", keybindings.addfeed);
	fprintf (configfile, "delete feed:%c\n", keybindings.deletefeed);
	fprintf (configfile, "reload all feeds:%c\n", keybindings.reloadall);
	fprintf (configfile, "change default browser:%c\n", keybindings.dfltbrowser);
	fprintf (configfile, "move item up:%c\n", keybindings.moveup);
	fprintf (configfile, "move item down:%c\n", keybindings.movedown);
	fprintf (configfile, "change feedname:%c\n", keybindings.changefeedname);
	fprintf (configfile, "sort feeds:%c\n", keybindings.sortfeeds);
	fprintf (configfile, "categorize feed:%c\n", keybindings.categorize);
	fprintf (configfile, "apply filter:%c\n", keybindings.filter);
	fprintf (configfile, "only current category:%c\n", keybindings.filtercurrent);
	fprintf (configfile, "mark all as read:%c\n", keybindings.markallread);
	fprintf (configfile, "remove filter:%c\n", keybindings.nofilter);
	fprintf (configfile, "per feed filter:%c\n", keybindings.perfeedfilter);
	fprintf (configfile, "toggle AND/OR filtering:%c\n", keybindings.andxor);
	fprintf (configfile, "quit:%c\n", keybindings.quit);
	fputs ("# Feed display menu bindings\n", configfile);
	fprintf (configfile, "show feedinfo:%c\n", keybindings.feedinfo);
	fprintf (configfile, "mark feed as read:%c\n", keybindings.markread);
	fprintf (configfile, "mark item unread:%c\n", keybindings.markunread);
	fputs ("# General keybindungs\n", configfile);
	fprintf (configfile, "next item:%c\n", keybindings.next);
	fprintf (configfile, "previous item:%c\n", keybindings.prev);
	fprintf (configfile, "return to previous menu:%c\n", keybindings.prevmenu);
	fprintf (configfile, "reload feed:%c\n", keybindings.reload);
	fprintf (configfile, "force reload feed:%c\n", keybindings.forcereload);
	fprintf (configfile, "open url:%c\n", keybindings.urljump);
	fprintf (configfile, "open item url in overview:%c\n", keybindings.urljump2);
	fprintf (configfile, "page up:%c\n", keybindings.pup);
	fprintf (configfile, "page down:%c\n", keybindings.pdown);
	fprintf (configfile, "top:%c\n", keybindings.home);
	fprintf (configfile, "bottom:%c\n", keybindings.end);
	fprintf (configfile, "enter:%c\n", keybindings.enter);
	fprintf (configfile, "show new headlines:%c\n", keybindings.newheadlines);
	fprintf (configfile, "help menu:%c\n", keybindings.help);
	fprintf (configfile, "about:%c\n", keybindings.about);
	fprintf (configfile, "type ahead find:%c\n", keybindings.typeahead);
	fclose (configfile);
}

// Set up colors and load user customized colors.
void SetupColors (const char * file) {
	char filebuf[128];
	char *freeme, *tmpbuf;
	FILE *configfile;

	color.newitems = 5;
	color.newitemsbold = 0;
	color.urljump = 4;
	color.urljumpbold = 0;
	color.feedtitle = -1;
	color.feedtitlebold = 0;
	
	configfile = fopen (file, "r");
	
	// If there is a config file, read it.
	if (configfile != NULL) {
		while (!feof(configfile)) {
			if ((fgets (filebuf, sizeof(filebuf), configfile)) == NULL)
				break;
			if (strstr(filebuf, "#") != NULL)
				continue;
			tmpbuf = strdup(filebuf);	// strsep only works on *ptr
			freeme = tmpbuf;		// Save start pos. This is also the string we need to compare. strsep will \0 the delimiter.
			strsep (&tmpbuf, ":");

			// Munch trailing newline.
			if (tmpbuf != NULL)
				tmpbuf[strlen(tmpbuf)-1] = 0;
				
			if (strcmp (freeme, "enabled") == 0) 
				use_colors = atoi(tmpbuf);
			if (strcmp (freeme, "new item") == 0) {
				color.newitems = atoi(tmpbuf);
				if (color.newitems > 7) {
					color.newitemsbold = 1;
				} else
					color.newitemsbold = 0;
			}
			if (strcmp (freeme, "goto url") == 0) {
				color.urljump = atoi(tmpbuf);
				if (color.urljump > 7) {
					color.urljumpbold = 1;
				} else
					color.urljumpbold = 0;
			}
			if (strcmp (freeme, "feedtitle") == 0) {
				color.feedtitle = atoi(tmpbuf);
				if (color.feedtitle > 7)
					color.feedtitlebold = 1;
				else
					color.feedtitlebold = 0;
			}
						
			free (freeme);
		}
		fclose (configfile);
	}
	
	// Write the file. This will write all setting with their
	// values read before and new ones with the defaults.
	configfile = fopen (file, "w+");
	fputs ("# Snownews color definitons\n", configfile);
	fputs ("# black:0\n", configfile);
	fputs ("# red:1\n", configfile);
	fputs ("# green:2\n", configfile);
	fputs ("# orange:3\n", configfile);
	fputs ("# blue:4\n", configfile);
	fputs ("# magenta(tm):5\n", configfile);
	fputs ("# cyan:6\n", configfile);
	fputs ("# gray:7\n", configfile);
	fputs ("# brightred:9\n", configfile);
	fputs ("# brightgreen:10\n", configfile);
	fputs ("# yellow:11\n", configfile);
	fputs ("# brightblue:12\n", configfile);
	fputs ("# brightmagenta:13\n", configfile);
	fputs ("# brightcyan:14\n", configfile);
	fputs ("# white:15\n", configfile);
	fputs ("# no color:-1\n", configfile);
	fprintf (configfile, "enabled:%d\n", use_colors);
	fprintf (configfile, "new item:%d\n", color.newitems);
	fprintf (configfile, "goto url:%d\n", color.urljump);
	fprintf (configfile, "feedtitle:%d\n", color.feedtitle);
	fclose (configfile);
	
	if (use_colors) {
		start_color();
		
		// The following call will automagically implement -1 as the terminal's
		// default color for fg/bg in init_pair.
		use_default_colors();
		
		// Internally used color pairs.
		// Use with WA_BOLD to get bright color (orange->yellow, gray->white, etc).
		init_pair (10, 1, -1);		// red
		init_pair (11, 2, -1);		// green
		init_pair (12, 3, -1);		// orange
		init_pair (13, 4, -1);		// blue
		init_pair (14, 5, -1);		// magenta
		init_pair (15, 6, -1);		// cyan
		init_pair (16, 7, -1);		// gray
		
		// Default terminal color color pair
		init_pair (63, -1, -1);
		
		// Initialize all color pairs we're gonna use.
		// New item.
		if (color.newitemsbold == 1)
			init_pair (2, color.newitems-8, -1);
		else
			init_pair (2, color.newitems, -1);
			
		// Goto url line
		if (color.urljumpbold == 1)
			init_pair (3, color.urljump-8, -1);
		else
			init_pair (3, color.urljump, -1);
		
		// Feed title header
		if (color.feedtitlebold == 1)
			init_pair (4, color.feedtitle-8, -1);
		else
			init_pair (4, color.feedtitle, -1);
		
	}

}

// Load user customized entity conversion table.
void SetupEntities (const char * file) {
	char filebuf[128];
	char *freeme, *tmpbuf, *tmppos;
	FILE *configfile;
	struct entity *entity;
	
	configfile = fopen (file, "r");
	
	if (configfile == NULL) {
		configfile = fopen (file, "w+");
		
		fputs ("# HTML entity conversion table\n", configfile);
		fputs ("# \n", configfile);
		fputs ("# Put all entities you want to have converted into this file.\n", configfile);
		fputs ("# File format: &entity;[converted string/char]\n", configfile);
		fputs ("# Example:     &mdash;--\n", configfile);
		fputs ("# \n", configfile);
		fputs ("# XML defined entities are converted by default. These are:\n# &amp;, &lt;, &gt;, &apos;, &quot;\n", configfile);
		fputs ("# \n", configfile);
		fprintf (configfile, "&auml;%c\n&ouml;%c\n&uuml;%c\n", 0xe4, 0xf6, 0xfc);	// umlauted a,o,u
		fprintf (configfile, "&Auml;%c\n&Ouml;%c\n&Uuml;%c\n", 0xc4, 0xd6, 0xdc);	// umlauted A,O,U
		fprintf (configfile, "&szlig;%c\n", 0xdf);	// german b
		fputs ("&nbsp; \n&mdash;--\n&hellip;...\n&#8220;\"\n&#8221;\"\n", configfile);
		fputs ("&raquo;\"\n&laquo;\"\n", configfile);
		
		fclose (configfile);
		configfile = fopen (file, "r");
	}
	
	while (!feof(configfile)) {
		if ((fgets (filebuf, sizeof(filebuf), configfile)) == NULL)
			break;
		
		// Discard comment lines.
		if (filebuf[0] == '#')
			continue;
		
		// Entities must start with '&', otherwise discard.
		if (filebuf[0] == '&') {
			entity = malloc (sizeof (struct entity));
			
			tmpbuf = strdup (filebuf);
			freeme = tmpbuf;
			
			// Munch trailing newline.
			if (tmpbuf[strlen(tmpbuf)-1] == '\n')
				tmpbuf[strlen(tmpbuf)-1] = '\0';
			
			// Delete starting '&' and set pointer to beginning of entity.
			tmpbuf[0] = '\0';
			tmpbuf++;
			tmppos = tmpbuf;
			
			strsep (&tmpbuf, ";");
			
			entity->entity = strdup (tmppos);
			entity->converted_entity = strdup (tmpbuf);
			entity->entity_length = strlen (entity->converted_entity);
			entity->next_ptr = NULL;
			
			free (freeme);
			
			// Add entity to linked list structure.
			if (first_entity == NULL)
				first_entity = entity;
			else {
				entity->next_ptr = first_entity;
				first_entity = entity;
			}
			
		} else
			continue;
	}
}

// Create snownews' config directories if they do not exist yet,
// load global configuration settings and caches.
//
// Returns number of feeds successfully loaded.
//
int Config (void) {
	char file[512];			// File locations.
	char filebuf[2048];
	char *freeme, *tmpbuf, *tmppos, *tmpstr;
	char *categories = NULL;	// Holds comma seperated category string.
	FILE *configfile;
	FILE *errorlog;
	struct feed *new_ptr;
	struct stat dirtest;
	int numfeeds = 0;		// Number of feeds we have.
	
	SetupProxy();
	
	SetupUserAgent();	
		
	// Set umask to 077 for all created files.
	umask(63);
	
	// Setup config directories.
	snprintf (file, sizeof(file), "%s/.snownews", getenv("HOME"));
	if ((stat (file, &dirtest)) == -1 ) {
		// Create directory.
		if (mkdir (file, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0)
			MainQuit ("Creating config directory ~/.snownews/", strerror(errno));
	} else {
		// Something with the name .snownews exists, let's see what it is.
		if ((dirtest.st_mode & S_IFDIR) != S_IFDIR) {
			MainQuit ("Creating config directory ~/.snownews/",
				"A file with the name \"~/.snownews/\" exists!");
		}
	}
	
	snprintf (file, sizeof(file), "%s/.snownews/cache", getenv("HOME"));
	if ((stat (file, &dirtest)) == -1) {
		// Create directory.
		if (mkdir (file, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0)
			MainQuit (_("Creating config directory ~/.snownews/cache/"), strerror(errno));
	} else {
		if ((dirtest.st_mode & S_IFDIR) != S_IFDIR) {
			MainQuit ("Creating config directory ~/.snownews/cache/",
				"A file with the name \"~/.snownews/cache/\" exists!");
		}
	}
	
	// Redirect stderr to ~/.snownews/error.log
	// Be sure to call _after_ the directory checks above!
	snprintf (file, sizeof(file), "%s/.snownews/error.log", getenv("HOME"));
	errorlog = fopen (file, "w+");
	dup2 (fileno(errorlog), STDERR_FILENO);
	
	UIStatus (_("Reading configuration settings..."), 0, 0);
	
	snprintf (file, sizeof(file), "%s/.snownews/browser", getenv("HOME"));
	SetupBrowser (file);

	//------------
	// Feed list
	//------------
	
	snprintf (file, sizeof(file), "%s/.snownews/urls", getenv("HOME"));
	configfile = fopen (file, "r");
	if (configfile == NULL) {
		UIStatus (_("Creating new configfile."), 0, 0);
		configfile = fopen (file, "w+");
		if (configfile == NULL)
			MainQuit (_("Create initial configfile \"urls\""), strerror(errno));	// Still didn't work?
	} else {
		while (!feof(configfile)) {
			if ((fgets (filebuf, sizeof(filebuf), configfile)) == NULL)
				break;
			if (filebuf[0] == '\n')
				break;
			
			numfeeds++;
			new_ptr = newFeedStruct();

			// Reset to NULL on every loop run!
			categories = NULL;
			
			// File format is url|custom name|comma seperated categories
			tmpbuf = strdup (filebuf);
			
			// Munch trailing newline.
			if (tmpbuf[strlen(tmpbuf)-1] == '\n')
				tmpbuf[strlen(tmpbuf)-1] = '\0';
				
			freeme = tmpbuf;
			tmpstr = strsep (&tmpbuf, "|");
			
			// The first | was squished with \0 so we can strdup the first part.
			new_ptr->feedurl = strdup (freeme);
			
			if (strncasecmp (new_ptr->feedurl, "exec:", 5) == 0) {
				new_ptr->execurl = 1;
			} else if (strncasecmp (new_ptr->feedurl, "smartfeed:", 10) == 0) {
				new_ptr->smartfeed = 1;
			}
			
			// Save start position of override string
			tmpstr = strsep (&tmpbuf, "|");
			if (tmpstr != NULL) {
				if (*tmpstr != '\0')
					new_ptr->override = strdup (tmpstr);
			}
			tmpstr = strsep (&tmpbuf, "|");
			if (tmpstr != NULL) {
				if (*tmpstr != '\0')
					categories = strdup (tmpstr);
			}
			
			tmpstr = strsep (&tmpbuf, "|");
			if (tmpstr != NULL) {
				if (*tmpstr != '\0')
					new_ptr->perfeedfilter = strdup (tmpstr);
			}
			
			// We don't need freeme anymore.
			free (freeme);
			
			// Put categories into cat struct.
			if (categories != NULL) {
				freeme = categories;
				
				while (1) {
					tmppos = strsep (&categories, ",");
					if (tmppos == NULL)
						break;
					
					FeedCategoryAdd (new_ptr, tmppos);
				}
				free (freeme);
			} else
				new_ptr->feedcategories = NULL;
			
			// Load cookies for this feed.
			// But skip loading cookies for execurls.
			if (new_ptr->execurl != 1)
				LoadCookies (new_ptr);
			
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
	fclose (configfile);
	
	snprintf (file, sizeof(file), "%s/.snownews/keybindings", getenv("HOME"));
	SetupKeybindings (file);
	
	snprintf (file, sizeof(file), "%s/.snownews/colors", getenv("HOME"));
	SetupColors (file);
	
	snprintf (file, sizeof(file), "%s/.snownews/html_entities", getenv("HOME"));
	SetupEntities (file);
	
	return numfeeds;
}
