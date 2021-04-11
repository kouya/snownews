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

#include "setup.h"
#include "main.h"
#include "cat.h"
#include "feedio.h"
#include "uiutil.h"
#include "conv.h"
#include <ncurses.h>

// Load browser command from ~./snownews/browser.
static void SetupBrowser (const char* filename)
{
    char linebuf[128] = "lynx %s";
    FILE* browserfile = fopen (filename, "r");
    if (browserfile) {
	fgets (linebuf, sizeof (linebuf), browserfile);
	if (linebuf[strlen (linebuf) - 1] == '\n')
	    linebuf[strlen (linebuf) - 1] = '\0';
	fclose (browserfile);
    }
    _settings.browser = strdup (linebuf);
}

void SaveBrowserSetting (void)
{
    char browserfilename[PATH_MAX];
    snprintf (browserfilename, sizeof (browserfilename), SNOWNEWS_CONFIG_DIR "browser", getenv ("HOME"));
    FILE* browserfile = fopen (browserfilename, "w");
    if (!browserfile)
	MainQuit (_("Save settings (browser)"), strerror (errno));
    fputs (_settings.browser, browserfile);
    fclose (browserfile);
}

// Parse http_proxy environment variable and define proxyname and proxyport.
static void SetupProxy (void)
{
    // Check for proxy environment variable.
    const char* proxyenv = getenv ("http_proxy");
    if (!proxyenv)
	return;
    // The pointer returned by getenv must not be altered.
    // What about mentioning this in the manpage of getenv?
    char* proxystring = strdup (proxyenv);
    char* proxystrbase = proxystring;
    strsep (&proxystring, "/");
    if (proxystring) {
	strsep (&proxystring, "/");
	if (proxystring) {
	    _settings.proxyname = strdup (strsep (&proxystring, ":"));
	    if (proxystring)
		_settings.proxyport = atoi (strsep (&proxystring, "/"));
	}
    }
    free (proxystrbase);
}

// Construct the user agent string that snownews sends to the webserver.
// This includes Snownews/VERSION, OS name and language setting.
static void SetupUserAgent (void)
{
    // Constuct the User-Agent string of snownews. This is done here in program init,
    // because we need to do it exactly once and it will never change while the program
    // is running.
    const char* lang = getenv ("LANG");
    if (!lang)
	lang = "C";
    // Snonews/VERSION (Linux; de_DE; (http://kiza.kcore.de/software/snownews/)
    static const char url[] = "http://snownews.kcore.de/software/snownews/";
    const unsigned urllen = strlen (url);
    unsigned ualen = strlen ("Snownews/" SNOWNEWS_VERSION) + 2 + strlen (lang) + 2 + strlen (OS) + 2 + urllen + 2;
    _settings.useragent = malloc (ualen);
    snprintf (_settings.useragent, ualen, "Snownews/" SNOWNEWS_VERSION " (" OS "; %s; %s)", lang, url);
}

// Define global keybindings and load user customized bindings.
static void SetupKeybindings (const char* filename)
{
    FILE* configfile = fopen (filename, "r");
    if (configfile) {
	// Read keybindings and populate keybindings struct.
	while (!feof (configfile)) {
	    char linebuf[128];
	    if (!fgets (linebuf, sizeof (linebuf), configfile))
		break;
	    if (linebuf[0] == '#')
		continue;
	    linebuf[strlen (linebuf) - 1] = 0;	// chop newline
	    char* value = linebuf;
	    strsep (&value, ":");

	    if (strcmp (linebuf, "next item") == 0)
		_settings.keybindings.next = value[0];
	    else if (strcmp (linebuf, "previous item") == 0)
		_settings.keybindings.prev = value[0];
	    else if (strcmp (linebuf, "return to previous menu") == 0)
		_settings.keybindings.prevmenu = value[0];
	    else if (strcmp (linebuf, "quit") == 0)
		_settings.keybindings.quit = value[0];
	    else if (strcmp (linebuf, "add feed") == 0)
		_settings.keybindings.addfeed = value[0];
	    else if (strcmp (linebuf, "delete feed") == 0)
		_settings.keybindings.deletefeed = value[0];
	    else if (strcmp (linebuf, "mark feed as read") == 0)
		_settings.keybindings.markread = value[0];
	    else if (strcmp (linebuf, "mark item unread") == 0)
		_settings.keybindings.markunread = value[0];
	    else if (strcmp (linebuf, "mark all as read") == 0)
		_settings.keybindings.markallread = value[0];
	    else if (strcmp (linebuf, "change default browser") == 0)
		_settings.keybindings.dfltbrowser = value[0];
	    else if (strcmp (linebuf, "sort feeds") == 0)
		_settings.keybindings.sortfeeds = value[0];
	    else if (strcmp (linebuf, "move item up") == 0)
		_settings.keybindings.moveup = value[0];
	    else if (strcmp (linebuf, "move item down") == 0)
		_settings.keybindings.movedown = value[0];
	    else if (strcmp (linebuf, "show feedinfo") == 0)
		_settings.keybindings.feedinfo = value[0];
	    else if (strcmp (linebuf, "reload feed") == 0)
		_settings.keybindings.reload = value[0];
	    else if (strcmp (linebuf, "force reload feed") == 0)
		_settings.keybindings.forcereload = value[0];
	    else if (strcmp (linebuf, "reload all feeds") == 0)
		_settings.keybindings.reloadall = value[0];
	    else if (strcmp (linebuf, "open url") == 0)
		_settings.keybindings.urljump = value[0];
	    else if (strcmp (linebuf, "open item url in overview") == 0)
		_settings.keybindings.urljump2 = value[0];
	    else if (strcmp (linebuf, "change feedname") == 0)
		_settings.keybindings.changefeedname = value[0];
	    else if (strcmp (linebuf, "page up") == 0)
		_settings.keybindings.pup = value[0];
	    else if (strcmp (linebuf, "page down") == 0)
		_settings.keybindings.pdown = value[0];
	    else if (strcmp (linebuf, "top") == 0)
		_settings.keybindings.home = value[0];
	    else if (strcmp (linebuf, "bottom") == 0)
		_settings.keybindings.end = value[0];
	    else if (strcmp (linebuf, "categorize feed") == 0)
		_settings.keybindings.categorize = value[0];
	    else if (strcmp (linebuf, "apply filter") == 0)
		_settings.keybindings.filter = value[0];
	    else if (strcmp (linebuf, "only current category") == 0)
		_settings.keybindings.filtercurrent = value[0];
	    else if (strcmp (linebuf, "remove filter") == 0)
		_settings.keybindings.nofilter = value[0];
	    else if (strcmp (linebuf, "per feed filter") == 0)
		_settings.keybindings.perfeedfilter = value[0];
	    else if (strcmp (linebuf, "help menu") == 0)
		_settings.keybindings.help = value[0];
	    else if (strcmp (linebuf, "about") == 0)
		_settings.keybindings.about = value[0];
	    else if (strcmp (linebuf, "toggle AND/OR filtering") == 0)
		_settings.keybindings.andxor = value[0];
	    else if (strcmp (linebuf, "enter") == 0)
		_settings.keybindings.enter = value[0];
	    else if (strcmp (linebuf, "show new headlines") == 0)
		_settings.keybindings.newheadlines = value[0];
	    else if (strcmp (linebuf, "type ahead find") == 0)
		_settings.keybindings.typeahead = value[0];
	}
	// Override old default settings and make sure there is no clash.
	// Default browser is now B; b moved to page up.
	if (_settings.keybindings.dfltbrowser == 'b')
	    _settings.keybindings.dfltbrowser = 'B';
	fclose (configfile);
    } else {
	// Write default bindings if the file is not there
	configfile = fopen (filename, "w");
	fputs ("# Snownews keybindings configfile\n", configfile);
	fputs ("# Main menu bindings\n", configfile);
	fprintf (configfile, "add feed:%c\n", _settings.keybindings.addfeed);
	fprintf (configfile, "delete feed:%c\n", _settings.keybindings.deletefeed);
	fprintf (configfile, "reload all feeds:%c\n", _settings.keybindings.reloadall);
	fprintf (configfile, "change default browser:%c\n", _settings.keybindings.dfltbrowser);
	fprintf (configfile, "move item up:%c\n", _settings.keybindings.moveup);
	fprintf (configfile, "move item down:%c\n", _settings.keybindings.movedown);
	fprintf (configfile, "change feedname:%c\n", _settings.keybindings.changefeedname);
	fprintf (configfile, "sort feeds:%c\n", _settings.keybindings.sortfeeds);
	fprintf (configfile, "categorize feed:%c\n", _settings.keybindings.categorize);
	fprintf (configfile, "apply filter:%c\n", _settings.keybindings.filter);
	fprintf (configfile, "only current category:%c\n", _settings.keybindings.filtercurrent);
	fprintf (configfile, "mark all as read:%c\n", _settings.keybindings.markallread);
	fprintf (configfile, "remove filter:%c\n", _settings.keybindings.nofilter);
	fprintf (configfile, "per feed filter:%c\n", _settings.keybindings.perfeedfilter);
	fprintf (configfile, "toggle AND/OR filtering:%c\n", _settings.keybindings.andxor);
	fprintf (configfile, "quit:%c\n", _settings.keybindings.quit);
	fputs ("# Feed display menu bindings\n", configfile);
	fprintf (configfile, "show feedinfo:%c\n", _settings.keybindings.feedinfo);
	fprintf (configfile, "mark feed as read:%c\n", _settings.keybindings.markread);
	fprintf (configfile, "mark item unread:%c\n", _settings.keybindings.markunread);
	fputs ("# General keybindungs\n", configfile);
	fprintf (configfile, "next item:%c\n", _settings.keybindings.next);
	fprintf (configfile, "previous item:%c\n", _settings.keybindings.prev);
	fprintf (configfile, "return to previous menu:%c\n", _settings.keybindings.prevmenu);
	fprintf (configfile, "reload feed:%c\n", _settings.keybindings.reload);
	fprintf (configfile, "force reload feed:%c\n", _settings.keybindings.forcereload);
	fprintf (configfile, "open url:%c\n", _settings.keybindings.urljump);
	fprintf (configfile, "open item url in overview:%c\n", _settings.keybindings.urljump2);
	fprintf (configfile, "page up:%c\n", _settings.keybindings.pup);
	fprintf (configfile, "page down:%c\n", _settings.keybindings.pdown);
	fprintf (configfile, "top:%c\n", _settings.keybindings.home);
	fprintf (configfile, "bottom:%c\n", _settings.keybindings.end);
	fprintf (configfile, "enter:%c\n", _settings.keybindings.enter);
	fprintf (configfile, "show new headlines:%c\n", _settings.keybindings.newheadlines);
	fprintf (configfile, "help menu:%c\n", _settings.keybindings.help);
	fprintf (configfile, "about:%c\n", _settings.keybindings.about);
	fprintf (configfile, "type ahead find:%c\n", _settings.keybindings.typeahead);
	fclose (configfile);
    }
}

// Set up colors and load user customized colors.
static void SetupColors (const char* filename)
{
    // If there is a config file, read it.
    FILE* configfile = fopen (filename, "r");
    if (configfile) {
	while (!feof (configfile)) {
	    char linebuf[128];
	    if (!fgets (linebuf, sizeof (linebuf), configfile))
		break;
	    if (linebuf[0] == '#')
		continue;
	    linebuf[strlen (linebuf) - 1] = 0;	// chop newline
	    char* value = linebuf;
	    strsep (&value, ":");
	    unsigned cval = atoi (value);
	    bool cvalbold = (cval > 7);
	    if (strcmp (linebuf, "enabled") == 0)
		_settings.monochrome = !cval;
	    else if (strcmp (linebuf, "new item") == 0) {
		_settings.color.newitems = cval;
		_settings.color.newitemsbold = cvalbold;
	    } else if (strcmp (linebuf, "goto url") == 0) {
		_settings.color.urljump = cval;
		_settings.color.urljumpbold = cvalbold;
	    } else if (strcmp (linebuf, "feedtitle") == 0) {
		_settings.color.feedtitle = cval;
		_settings.color.feedtitlebold = cvalbold;
	    }
	}
	fclose (configfile);
    } else {
	// Write the file. This will write all setting with their
	// values read before and new ones with the defaults.
	configfile = fopen (filename, "w");
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
	fprintf (configfile, "enabled:%d\n", !_settings.monochrome);
	fprintf (configfile, "new item:%d\n", _settings.color.newitems);
	fprintf (configfile, "goto url:%d\n", _settings.color.urljump);
	fprintf (configfile, "feedtitle:%d\n", _settings.color.feedtitle);
	fclose (configfile);
    }
    if (!_settings.monochrome) {
	start_color();

	// The following call will automagically implement -1 as the terminal's
	// default color for fg/bg in init_pair.
	use_default_colors();

	// Internally used color pairs.
	// Use with WA_BOLD to get bright color (orange->yellow, gray->white, etc).
	init_pair (10, 1, -1); // red
	init_pair (11, 2, -1); // green
	init_pair (12, 3, -1); // orange
	init_pair (13, 4, -1); // blue
	init_pair (14, 5, -1); // magenta
	init_pair (15, 6, -1); // cyan
	init_pair (16, 7, -1); // gray

	// Default terminal color color pair
	init_pair (63, -1, -1);

	// Initialize all color pairs we're gonna use.
	init_pair (2, _settings.color.newitems - 8 * _settings.color.newitemsbold, -1);	// New item.
	init_pair (3, _settings.color.urljump - 8 * _settings.color.urljumpbold, -1);	// Goto url line
	init_pair (4, _settings.color.feedtitle - 8 * _settings.color.feedtitlebold, -1);	// Feed title header
    }

}

// Load user customized entity conversion table.
static void SetupEntities (const char* file)
{
    FILE* configfile = fopen (file, "r");
    if (!configfile) {
	configfile = fopen (file, "w");
	enum {			// Non-ascii character codes that should not be in source code
	    Umlauta = 0xe4, Umlauto = 0xf6, Umlautu = 0xfc,
	    UmlautA = 0xc4, UmlautO = 0xd6, UmlautU = 0xdc,
	    GermanB = 0xdf
	};
	fprintf (configfile,
		 "# HTML entity conversion table\n" "#\n" "# Put all entities you want to have converted into this file.\n" "# File format: &entity;[converted string/char]\n" "# Example:     &mdash;--\n" "#\n"
		 "# XML defined entities are converted by default. These are:\n" "# &amp;, &lt;, &gt;, &apos;, &quot;\n" "#\n" "&auml;%c\n" "&ouml;%c\n" "&uuml;%c\n" "&Auml;%c\n" "&Ouml;%c\n" "&Uuml;%c\n" "&szlig;%c\n" "&nbsp; \n" "&mdash;--\n"
		 "&hellip;...\n" "&#8220;\"\n" "&#8221;\"\n" "&raquo;\"\n" "&laquo;\"\n", Umlauta, Umlauto, Umlautu, UmlautA, UmlautO, UmlautU, GermanB);
	fclose (configfile);
	configfile = fopen (file, "r");
    }
    while (!feof (configfile)) {
	char linebuf[128];
	if (!fgets (linebuf, sizeof (linebuf), configfile))
	    break;
	if (linebuf[0] != '&')
	    continue;	       // Entities must start with '&'
	linebuf[strlen (linebuf) - 1] = 0;	// chop newline

	char* parse = &linebuf[1];	// Go past the '&'
	const char* ename = strsep (&parse, ";");
	const char* evalue = parse;

	struct entity* entity = calloc (1, sizeof (struct entity));
	entity->entity = strdup (ename);
	entity->converted_entity = strdup (evalue);
	entity->entity_length = strlen (entity->converted_entity);

	// Add entity to linked list structure.
	if (!_settings.html_entities)
	    _settings.html_entities = entity;
	else {
	    entity->next = _settings.html_entities;
	    _settings.html_entities = entity;
	}
    }
}

static unsigned SetupFeedList (const char* filename)
{
    unsigned numfeeds = 0;
    FILE* configfile = fopen (filename, "r");
    if (!configfile)
	return numfeeds;       // no feeds
    while (!feof (configfile)) {
	char linebuf[256];
	if (!fgets (linebuf, sizeof (linebuf), configfile))
	    break;
	if (linebuf[0] == '\n')
	    break;
	linebuf[strlen (linebuf) - 1] = 0;	// chop newline

	// File format is url|custom name|comma seperated categories|filters
	char* parse = linebuf;
	const char* url = strsep (&parse, "|");
	if (!url || !url[0])
	    continue;	       // no url
	const char* cname = strsep (&parse, "|");
	char* categories = strsep (&parse, "|");
	const char* filters = parse;

	++numfeeds;

	struct feed* new_ptr = newFeedStruct();
	new_ptr->feedurl = strdup (url);
	if (strncasecmp (new_ptr->feedurl, "exec:", strlen ("exec:")) == 0)
	    new_ptr->execurl = true;
	else if (strncasecmp (new_ptr->feedurl, "smartfeed:", strlen ("smartfeed:")) == 0)
	    new_ptr->smartfeed = true;
	if (cname && cname[0])
	    new_ptr->custom_title = strdup (cname);
	if (filters && filters[0])
	    new_ptr->perfeedfilter = strdup (filters);
	if (categories && categories[0])	// Put categories into cat struct.
	    for (char *catnext = categories, *catname; (catname = strsep (&catnext, ","));)
		FeedCategoryAdd (new_ptr, catname);

	// Add to bottom of pointer chain.
	if (!_feed_list)
	    _feed_list = new_ptr;
	else {
	    new_ptr->prev = _feed_list;
	    while (new_ptr->prev->next)
		new_ptr->prev = new_ptr->prev->next;
	    new_ptr->prev->next = new_ptr;
	}
    }
    fclose (configfile);
    return numfeeds;
}

// Create snownews' config directories if they do not exist yet,
// load global configuration settings and caches.
//
// Returns number of feeds successfully loaded.
//
unsigned Config (void)
{
    // Set umask to 077 for all created files.
    umask (0077);

    SetupProxy();
    SetupUserAgent();

    // Setup config directories.
    char dirname[PATH_MAX];
    snprintf (dirname, sizeof (dirname), SNOWNEWS_CONFIG_DIR, getenv ("HOME"));
    struct stat dirtest;
    if (0 > stat (dirname, &dirtest)) {
	if (0 > mkdir (dirname, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH))
	    MainQuit ("Creating config directory " SNOWNEWS_CONFIG_DIR, strerror (errno));
    } else if (!S_ISDIR (dirtest.st_mode))
	MainQuit ("Creating config directory " SNOWNEWS_CONFIG_DIR, "A file with the name \"" SNOWNEWS_CONFIG_DIR "\" exists!");

    snprintf (dirname, sizeof (dirname), SNOWNEWS_CACHE_DIR, getenv ("HOME"));
    if (0 > stat (dirname, &dirtest)) {
	if (0 > mkdir (dirname, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH))
	    MainQuit (_("Creating config directory " SNOWNEWS_CACHE_DIR), strerror (errno));
    } else if (!S_ISDIR (dirtest.st_mode))
	MainQuit ("Creating config directory " SNOWNEWS_CACHE_DIR, "A file with the name \"" SNOWNEWS_CACHE_DIR "\" exists!");

    UIStatus (_("Reading configuration settings..."), 0, 0);

    char filename[PATH_MAX];
    snprintf (filename, sizeof (filename), SNOWNEWS_CONFIG_DIR "browser", getenv ("HOME"));
    SetupBrowser (filename);

    snprintf (filename, sizeof (filename), SNOWNEWS_CONFIG_DIR "urls", getenv ("HOME"));
    unsigned numfeeds = SetupFeedList (filename);

    snprintf (filename, sizeof (filename), SNOWNEWS_CONFIG_DIR "keybindings", getenv ("HOME"));
    SetupKeybindings (filename);

    snprintf (filename, sizeof (filename), SNOWNEWS_CONFIG_DIR "colors", getenv ("HOME"));
    SetupColors (filename);

    snprintf (filename, sizeof (filename), SNOWNEWS_CONFIG_DIR "html_entities", getenv ("HOME"));
    SetupEntities (filename);

    return numfeeds;
}
