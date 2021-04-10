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

#include "io-internal.h"
#include "conversions.h"
#include "filters.h"
#include "io-internal.h"
#include "netio.h"
#include "ui-support.h"
#include "parsefeed.h"
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
    for (struct feed * f = _feed_list; f; f = f->next)
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
    char cachefilename[PATH_MAX];
    snprintf (cachefilename, sizeof (cachefilename), SNOWNEWS_CACHE_DIR "%s", getenv ("HOME"), hashme);
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
    for (struct feed * f = _feed_list; f; f = f->next) {
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

static void WriteFeedUrls (void)
{
    if (!_feed_list_changed)
	return;

    // Make a backup of urls.
    char urlsfilename[PATH_MAX];
    snprintf (urlsfilename, sizeof (urlsfilename), SNOWNEWS_CONFIG_DIR "urls", getenv ("HOME"));

    // Write urls
    FILE* urlfile = fopen (urlsfilename, "w");
    if (!urlfile) {
	syslog (LOG_ERR, "error saving urls: %s", strerror (errno));
	return;
    }
    for (const struct feed * f = _feed_list; f; f = f->next) {
	fputs (f->feedurl, urlfile);
	fputc ('|', urlfile);
	if (f->custom_title)
	    fputs (f->title, urlfile);
	fputc ('|', urlfile);
	for (const struct feedcategories * c = f->feedcategories; c; c = c->next) {
	    fputs (c->name, urlfile);
	    if (c->next)       // Only add a colon of we run the loop again!
		fputc (',', urlfile);
	}
	fputc ('|', urlfile);
	if (f->perfeedfilter != NULL)
	    fputs (f->perfeedfilter, urlfile);

	fputc ('\n', urlfile); // Add newline character.
    }
    fclose (urlfile);
    _feed_list_changed = false;
}

static void WriteFeedCache (const struct feed* feed)
{
    char* hashme = Hashify (feed->feedurl);
    char cachefilename[PATH_MAX];
    snprintf (cachefilename, sizeof (cachefilename), SNOWNEWS_CACHE_DIR "%s", getenv ("HOME"), hashme);
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

    fputs ("<?xml version=\"1.0\" ?>\n\n<rdf:RDF\n  xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"\n  xmlns=\"http://purl.org/rss/1.0/\"\n  xmlns:snow=\"http://snownews.kcore.de/ns/1.0/\">\n\n", cache);

    if (feed->lastmodified)
	fprintf (cache, "<snow:lastmodified>%s</snow:lastmodified>\n", feed->lastmodified);

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
    for (const struct feed * f = _feed_list; f; f = f->next)
	++numfeeds;

    unsigned count = 1;
    int oldnumobjects = 0;
    unsigned titlestrlen = strlen (_("Saving settings ["));

    for (const struct feed * cur_ptr = _feed_list; cur_ptr; cur_ptr = cur_ptr->next) {
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
