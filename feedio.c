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

#include "feedio.h"
#include "conv.h"
#include "filters.h"
#include "netio.h"
#include "uiutil.h"
#include "parse.h"
#include "setup.h"
#include "cat.h"
#include <ncurses.h>
#include <libxml/parser.h>

struct feed* newFeedStruct (void)
{
    return calloc (1, sizeof (struct feed));
}

// Update given feed from server.
// Reload XML document and replace in memory cur_ptr->xmltext with it.
int UpdateFeed (struct feed* cur_ptr)
{
    if (cur_ptr == NULL)
	return 1;

    // Smart feeds are generated in the fly.
    if (cur_ptr->smartfeed == 1)
	return 0;

    // If current feed is an exec URL, run that command, otherwise fetch resource from network.
    if (cur_ptr->execurl)
	FilterExecURL (cur_ptr);
    else {
	DownloadFeed (cur_ptr->feedurl, cur_ptr);

	// Set title and link structure to something.
	// To the feedurl in this case so the program show something
	// as placeholder instead of crash.
	if (cur_ptr->title == NULL)
	    cur_ptr->title = strdup (cur_ptr->feedurl);
	if (cur_ptr->link == NULL)
	    cur_ptr->link = strdup (cur_ptr->feedurl);

	// If the download function returns a NULL pointer return from here.
	if (!cur_ptr->xmltext)
	    return 1;
    }

    // Feed downloaded content through the defined filter.
    if (cur_ptr->perfeedfilter != NULL)
	FilterPipeNG (cur_ptr);

    // If there is no feed, return.
    if (!cur_ptr->xmltext)
	return 1;

    if (DeXML (cur_ptr)) {
	UIStatus (_("Invalid XML! Cannot parse this feed!"), 2, 1);
	// Activate feed problem flag.
	cur_ptr->problem = true;
	return 1;
    }
    // We don't need these anymore. Free the raw XML to save some memory.
    free (cur_ptr->xmltext);
    cur_ptr->xmltext = NULL;
    cur_ptr->content_length = 0;

    // Mark the time to detect modifications
    cur_ptr->mtime = time (NULL);
    return 0;
}

int UpdateAllFeeds (void)
{
    for (struct feed* f = _feed_list; f; f = f->next)
	if (0 != UpdateFeed (f))
	    continue;
    return 0;
}

// Load feed from disk. And call UpdateFeed if neccessary.
int LoadFeed (struct feed* cur_ptr)
{
    // Smart feeds are generated in the fly.
    if (cur_ptr->smartfeed == 1)
	return 0;

    char* hashme = Hashify (cur_ptr->feedurl);
    char cachefilename [PATH_MAX];
    CacheFilePath (hashme, cachefilename, sizeof(cachefilename));
    free (hashme);
    FILE* cache = fopen (cachefilename, "r");
    if (cache == NULL) {
	char msgbuf[128];
	snprintf (msgbuf, sizeof (msgbuf), _("Cache for %s is toast. Reloading from server..."), cur_ptr->feedurl);
	UIStatus (msgbuf, 0, 0);

	if (UpdateFeed (cur_ptr) != 0)
	    return 1;
	return 0;
    }

    struct stat cachest;
    fstat (fileno (cache), &cachest);

    // Read complete cachefile.
    int len = 0;		// Internal usage for realloc.
    char filebuf[BUFSIZ];	// File I/O block buffer.
    while (!feof (cache)) {
	// Use binary read, UTF-8 data!
	size_t retval = fread (filebuf, 1, sizeof (filebuf), cache);
	if (retval == 0)
	    break;
	cur_ptr->xmltext = realloc (cur_ptr->xmltext, len + retval + 1);
	memcpy (cur_ptr->xmltext + len, filebuf, retval);
	len += retval;
	if (retval != sizeof (filebuf))
	    break;
    }
    fclose (cache);
    cur_ptr->content_length = len;
    cur_ptr->xmltext[len] = '\0';

    if (!cur_ptr->xmltext)
	return 1;

    // After loading DeXMLize the mess.
    // If loading the feed from the disk fails, try downloading from the net.
    if (DeXML (cur_ptr) != 0 && UpdateFeed (cur_ptr) != 0) {
	// And if that fails as well, just continue without this feed.
	char msgbuf[64];
	snprintf (msgbuf, sizeof (msgbuf), _("Could not load %s!"), cur_ptr->feedurl);
	UIStatus (msgbuf, 2, 1);
    }

    free (cur_ptr->xmltext);
    cur_ptr->xmltext = NULL;
    cur_ptr->content_length = 0;
    cur_ptr->mtime = cachest.st_mtime;
    return 0;
}

int LoadAllFeeds (unsigned numfeeds)
{
    if (!numfeeds)
	return 0;
    UIStatus (_("Loading cache ["), 0, 0);
    unsigned titlestrlen = strlen (_("Loading cache ["));
    int oldnumobjects = 0;
    unsigned count = 1;
    for (struct feed* f = _feed_list; f; f = f->next) {
	// Progress bar
	int numobjects = count * (COLS - titlestrlen - 2) / numfeeds - 2;
	if (numobjects < 1)
	    numobjects = 1;
	if (numobjects > oldnumobjects) {
	    DrawProgressBar (numobjects, titlestrlen);
	    oldnumobjects = numobjects;
	}
	if (LoadFeed (f) != 0)
	    continue;
	++count;
    }
    return 0;
}

void AddFeedToList (struct feed* new_feed)
{
    if (!_feed_list)
	_feed_list = new_feed;
    else {
	new_feed->prev = _feed_list;
	while (new_feed->prev->next)
	    new_feed->prev = new_feed->prev->next;
	new_feed->prev->next = new_feed;
    }
}

void AddFeed (const char* url, const char* cname, const char* categories, const char* filter)
{
    struct feed* new_ptr = newFeedStruct();
    new_ptr->feedurl = strdup (url);
    if (strncasecmp (new_ptr->feedurl, "exec:", strlen ("exec:")) == 0)
	new_ptr->execurl = true;
    else if (strncasecmp (new_ptr->feedurl, "smartfeed:", strlen ("smartfeed:")) == 0)
	new_ptr->smartfeed = true;
    if (cname && cname[0])
	new_ptr->custom_title = strdup (cname);
    if (filter && filter[0])
	new_ptr->perfeedfilter = strdup (filter);
    if (categories && categories[0]) {	// Put categories into cat struct.
	char* catlist = strdup (categories);
	for (char* catnext = catlist; catnext;)
	    FeedCategoryAdd (new_ptr, strsep (&catnext, ","));
	free (catlist);
    }
    AddFeedToList (new_ptr);
}

static void WriteFeedUrls (void)
{
    if (!_feed_list_changed)
	return;

    // Make a backup of urls.
    char urlsfilename[PATH_MAX];
    ConfigFilePath ("urls.opml", urlsfilename, sizeof(urlsfilename));

    // Write urls
    FILE* urlfile = fopen (urlsfilename, "w");
    if (!urlfile) {
	syslog (LOG_ERR, "error saving urls: %s", strerror (errno));
	return;
    }

    // Write header
    fputs (
	"<?xml version=\"1.0\"?>\n"
	"<opml version=\"2.0\"", urlfile);

    // See if the custom namespace is needed
    bool needNs = false;
    for (const struct feed* f = _feed_list; f; f = f->next)
	if (f->perfeedfilter)
	    needNs = true;
    if (needNs)
	fputs (" xmlns:snow=\"http://snownews.kcore.de/ns/1.0/\"", urlfile);

    fputs (
	">\n"
	"    <head>\n"
	"	<title>" SNOWNEWS_NAME " subscriptions</title>\n"
	"    </head>\n"
	"    <body>\n"
	, urlfile);

    // Write one outline element per feed
    for (const struct feed* f = _feed_list; f; f = f->next) {
	fputs ("\t<outline", urlfile);
	if (f->custom_title)
	    fprintf (urlfile, " text=\"%s\"", f->custom_title);
	else if (f->title)
	    fprintf (urlfile, " text=\"%s\"", f->title);
	if (f->feedurl)
	    fprintf (urlfile, " xmlUrl=\"%s\"", f->feedurl);
	if (f->feedcategories) {
	    fputs (" category=\"", urlfile);
	    for (const struct feedcategories* c = f->feedcategories; c; c = c->next) {
		fputs (c->name, urlfile);
		if (c->next)
		    fputc (',', urlfile);
	    }
	    fputs ("\"", urlfile);
	}
	if (f->perfeedfilter)
	    fprintf (urlfile, " snow:filter=\"%s\"", f->perfeedfilter);
	fputs ("/>\n", urlfile);
    }
    fputs (
	"    </body>\n"
	"</opml>\n", urlfile);
    fclose (urlfile);
    _feed_list_changed = false;
}

static void WriteFeedCache (const struct feed* feed)
{
    char* hashme = Hashify (feed->feedurl);
    char cachefilename [PATH_MAX];
    CacheFilePath (hashme, cachefilename, sizeof(cachefilename));
    free (hashme);

    // Check if the feed has been modified since last loaded from this file
    struct stat cachest;
    if (0 == stat (cachefilename, &cachest) && cachest.st_mtime >= feed->mtime)
	return;

    FILE* cache = fopen (cachefilename, "w");
    if (!cache) {
	syslog (LOG_ERR, "error writing cache file '%s': %s", cachefilename, strerror (errno));
	return;
    }

    fputs (
	"<?xml version=\"1.0\" ?>\n\n"
	"<rdf:RDF\n"
	"\txmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"\n"
	"\txmlns=\"http://purl.org/rss/1.0/\"\n"
	"\txmlns:snow=\"http://snownews.kcore.de/ns/1.0/\">\n\n", cache);

    if (feed->lastmodified)
	fprintf (cache, "<snow:lastmodified>%ld</snow:lastmodified>\n", feed->lastmodified);

    char* encoded = (char*) xmlEncodeEntitiesReentrant (NULL, (xmlChar*) feed->feedurl);
    fprintf (cache, "<channel rdf:about=\"%s\">\n<title>", encoded);
    free (encoded);

    if (feed->original)
	fprintf (cache, "<![CDATA[%s]]>", feed->original);
    else if (feed->title)
	fprintf (cache, "<![CDATA[%s]]>", feed->title);
    fputs ("</title>\n<link>", cache);
    if (feed->link) {
	encoded = (char*) xmlEncodeEntitiesReentrant (NULL, (xmlChar*) feed->link);
	fputs (encoded, cache);
	free (encoded);
    }
    fputs ("</link>\n<description>", cache);
    if (feed->description)
	fprintf (cache, "<![CDATA[%s]]>", feed->description);
    fputs ("</description>\n</channel>\n\n", cache);

    for (const struct newsitem * item = feed->items; item; item = item->next) {
	fputs ("<item rdf:about=\"", cache);
	if (item->data->link) {
	    encoded = (char*) xmlEncodeEntitiesReentrant (NULL, (xmlChar*) item->data->link);
	    fputs (encoded, cache);
	    free (encoded);
	}
	fputs ("\">\n<title>", cache);
	if (item->data->title)
	    fprintf (cache, "<![CDATA[%s]]>", item->data->title);
	fputs ("</title>\n<link>", cache);
	if (item->data->link) {
	    encoded = (char*) xmlEncodeEntitiesReentrant (NULL, (xmlChar*) item->data->link);
	    fputs (encoded, cache);
	    free (encoded);
	}
	fputs ("</link>\n<description>", cache);
	if (item->data->description)
	    fprintf (cache, "<![CDATA[%s]]>", item->data->description);
	fputs ("</description>\n<snow:readstatus>", cache);
	putc ('0' + item->data->readstatus, cache);
	fputs ("</snow:readstatus>\n<snow:hash>", cache);
	if (item->data->hash)
	    fputs (item->data->hash, cache);
	fputs ("</snow:hash>\n", cache);
	fprintf (cache, "<snow:date>%u</snow:date>\n", item->data->date);
	fputs ("</item>\n\n", cache);
    }
    fputs ("</rdf:RDF>", cache);
    fclose (cache);
}

// Write in memory structures to disk cache.
// Usually called before program exit.
void WriteCache (void)
{
    UIStatus (_("Saving settings ["), 0, 0);

    WriteFeedUrls();

    // Save feed cache
    unsigned numfeeds = 0;
    for (const struct feed* f = _feed_list; f; f = f->next)
	++numfeeds;

    unsigned count = 1;
    int oldnumobjects = 0;
    unsigned titlestrlen = strlen (_("Saving settings ["));

    for (const struct feed* cur_ptr = _feed_list; cur_ptr; cur_ptr = cur_ptr->next) {
	// Progress bar
	int numobjects = count * (COLS - titlestrlen - 2) / numfeeds - 2;
	if (numobjects < 1)
	    numobjects = 1;
	if (numobjects > oldnumobjects) {
	    DrawProgressBar (numobjects, titlestrlen);
	    oldnumobjects = numobjects;
	}
	++count;

	// Discard smart feeds from cache.
	if (cur_ptr->smartfeed)
	    continue;

	// Write cache.
	WriteFeedCache (cur_ptr);
    }
    return;
}
