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

#include "io-internal.h"
#include "conversions.h"
#include "filters.h"
#include "io-internal.h"
#include "netio.h"
#include "ui-support.h"
#include "xmlparse.h"
#include <errno.h>
#include <ncurses.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libxml/parser.h>
#include <syslog.h>

struct feed * newFeedStruct (void) {
	struct feed* new = calloc (1, sizeof(struct feed));
	new->netio_error = NET_ERR_OK;
	return new;
}

static void GetHTTPErrorString (char* errorstring, size_t size, unsigned httpstatus) {
	const char* statusmsg = "HTTP %u!";
	switch (httpstatus) {
		case 400: statusmsg = "Bad request";		break;
		case 402: statusmsg = "Payment required";	break;
		case 403: statusmsg = "Access denied";		break;
		case 500: statusmsg =  "Internal server error"; break;
		case 501: statusmsg = "Not implemented";	break;
		case 502:
		case 503: statusmsg = "Service unavailable";	break;
	}
	snprintf (errorstring, size, statusmsg, httpstatus);
}

static void PrintUpdateError (const struct feed* cur_ptr) {
	enum netio_error err = cur_ptr->netio_error;
	char errstr[256];
	switch (err) {
		case NET_ERR_URL_INVALID:
			snprintf(errstr, sizeof(errstr), _("%s: Invalid URL!"), cur_ptr->title);
			break;
		case NET_ERR_SOCK_ERR:
			snprintf(errstr, sizeof(errstr), _("%s: Couldn't create network socket!"), cur_ptr->title);
			break;
		case NET_ERR_HOST_NOT_FOUND:
			snprintf(errstr, sizeof(errstr), _("%s: Can't resolve host!"), cur_ptr->title);
			break;
		case NET_ERR_CONN_REFUSED:
			snprintf(errstr, sizeof(errstr), _("%s: Connection refused!"), cur_ptr->title);
			break;
		case NET_ERR_CONN_FAILED:
			snprintf(errstr, sizeof(errstr), _("%s: Couldn't connect to server: %s"),
				cur_ptr->title,
				(strerror(cur_ptr->connectresult) ? strerror(cur_ptr->connectresult) : "(null)"));
			break;
		case NET_ERR_TIMEOUT:
			snprintf(errstr, sizeof(errstr), _("%s: Connection timed out."), cur_ptr->title);
			break;
		case NET_ERR_UNKNOWN:
			break;
		case NET_ERR_REDIRECT_COUNT_ERR:
			snprintf(errstr, sizeof(errstr), _("%s: Too many HTTP redirects encountered! Giving up."), cur_ptr->title);
			break;
		case NET_ERR_REDIRECT_ERR:
			snprintf(errstr, sizeof(errstr), _("%s: Server sent an invalid redirect!"), cur_ptr->title);
			break;
		case NET_ERR_HTTP_410:
		case NET_ERR_HTTP_404:
			snprintf(errstr, sizeof(errstr), _("%s: This feed no longer exists. Please unsubscribe!"), cur_ptr->title);
			break;
		case NET_ERR_HTTP_NON_200: {
			char httperrstr[64];
			GetHTTPErrorString(httperrstr, sizeof(httperrstr), cur_ptr->lasthttpstatus);
			snprintf(errstr, sizeof(errstr), _("%s: Could not download feed: %s"), cur_ptr->title, httperrstr);
			} break;
		case NET_ERR_HTTP_PROTO_ERR:
			snprintf(errstr, sizeof(errstr), _("%s: Error in server reply."), cur_ptr->title);
			break;
		case NET_ERR_AUTH_FAILED:
			snprintf(errstr, sizeof(errstr), _("%s: Authentication failed!"), cur_ptr->title);
			break;
		case NET_ERR_AUTH_NO_AUTHINFO:
			snprintf(errstr, sizeof(errstr), _("%s: URL does not contain authentication information!"), cur_ptr->title);
			break;
		case NET_ERR_AUTH_GEN_AUTH_ERR:
			snprintf(errstr, sizeof(errstr), _("%s: Could not generate authentication information!"), cur_ptr->title);
			break;
		case NET_ERR_AUTH_UNSUPPORTED:
			snprintf(errstr, sizeof(errstr), _("%s: Unsupported authentication method requested by server!"), cur_ptr->title);
			break;
		case NET_ERR_GZIP_ERR:
			snprintf(errstr, sizeof(errstr), _("%s: Error decompressing server reply!"), cur_ptr->title);
			break;
		case NET_ERR_CHUNKED:
			snprintf(errstr, sizeof(errstr), _("%s: Error in server reply. Chunked encoding is not supported."), cur_ptr->title);
			break;
		default:
			snprintf(errstr, sizeof(errstr), _("%s: Some error occured for which no specific error message was written."), cur_ptr->title);
			break;
	}
	UIStatus(errstr, 2, 1);
	syslog (LOG_ERR, errstr);
}

// Update given feed from server.
// Reload XML document and replace in memory cur_ptr->xmltext with it.
int UpdateFeed (struct feed * cur_ptr) {
	if (cur_ptr == NULL)
		return 1;

	// Smart feeds are generated in the fly.
	if (cur_ptr->smartfeed == 1)
		return 0;

	// If current feed is an exec URL, run that command, otherwise fetch resource from network.
	if (cur_ptr->execurl)
		FilterExecURL (cur_ptr);
	else {
		// Need to work on a copy of ->feedurl, because DownloadFeed() changes the pointer.
		char* feedurl = strdup (cur_ptr->feedurl);
		free (cur_ptr->xmltext);

		cur_ptr->xmltext = DownloadFeed (feedurl, cur_ptr, 0);

		free (feedurl);

		// Set title and link structure to something.
		// To the feedurl in this case so the program show something
		// as placeholder instead of crash.
		if (cur_ptr->title == NULL)
			cur_ptr->title = strdup (cur_ptr->feedurl);
		if (cur_ptr->link == NULL)
			cur_ptr->link = strdup (cur_ptr->feedurl);

		// If the download function returns a NULL pointer return from here.
		if (!cur_ptr->xmltext) {
			if (cur_ptr->problem)
				PrintUpdateError (cur_ptr);
			return 1;
		}
	}

	// Feed downloaded content through the defined filter.
	if (cur_ptr->perfeedfilter != NULL)
		FilterPipeNG (cur_ptr);

	// If there is no feed, return.
	if (!cur_ptr->xmltext)
		return 1;

	if (DeXML(cur_ptr)) {
		UIStatus (_("Invalid XML! Cannot parse this feed!"), 2, 1);
		// Activate feed problem flag.
		cur_ptr->problem = true;
		return 1;
	}

	// We don't need these anymore. Free the raw XML to save some memory.
	free (cur_ptr->xmltext);
	cur_ptr->xmltext = NULL;

	// Mark the time to detect modifications
	cur_ptr->mtime = time(NULL);
	return 0;
}

int UpdateAllFeeds (void) {
	for (struct feed* f = _feed_list; f; f = f->next)
		if (0 != UpdateFeed (f))
			continue;
	return 0;
}

// Load feed from disk. And call UpdateFeed if neccessary.
int LoadFeed (struct feed * cur_ptr) {
	// Smart feeds are generated in the fly.
	if (cur_ptr->smartfeed == 1)
		return 0;

	char* hashme = Hashify(cur_ptr->feedurl);
	char cachefilename [PATH_MAX];
	snprintf (cachefilename, sizeof(cachefilename), "%s/.snownews/cache/%s", getenv("HOME"), hashme);
	free (hashme);
	FILE* cache = fopen (cachefilename, "r");
	if (cache == NULL) {
		char msgbuf[128];
		snprintf (msgbuf, sizeof(msgbuf), _("Cache for %s is toast. Reloading from server..."), cur_ptr->feedurl);
		UIStatus (msgbuf, 0, 0);

		if (UpdateFeed (cur_ptr) != 0)
			return 1;
		return 0;
	}

	struct stat cachest;
	fstat (fileno(cache), &cachest);

	// Read complete cachefile.
	int len = 0;		// Internal usage for realloc.
	char filebuf[BUFSIZ];	// File I/O block buffer.
	while (!feof(cache)) {
		// Use binary read, UTF-8 data!
		size_t retval = fread (filebuf, 1, sizeof(filebuf), cache);
		if (retval == 0)
			break;
		cur_ptr->xmltext = realloc (cur_ptr->xmltext, len+retval + 1);
		memcpy (cur_ptr->xmltext+len, filebuf, retval);
		len += retval;
		if (retval != sizeof(filebuf))
			break;
	}
	fclose (cache);
	cur_ptr->content_length = len;
	cur_ptr->xmltext[len] = '\0';

	if (!cur_ptr->xmltext)
		return 1;

	// After loading DeXMLize the mess.
	// If loading the feed from the disk fails, try downloading from the net.
	if (DeXML(cur_ptr) != 0 && UpdateFeed(cur_ptr) != 0) {
		// And if that fails as well, just continue without this feed.
		char msgbuf [64];
		snprintf (msgbuf, sizeof(msgbuf), _("Could not load %s!"), cur_ptr->feedurl);
		UIStatus (msgbuf, 2, 1);
	}

	free (cur_ptr->xmltext);
	cur_ptr->xmltext = NULL;
	cur_ptr->mtime = cachest.st_mtime;
	return 0;
}

int LoadAllFeeds (unsigned numfeeds) {
	if (!numfeeds)
		return 0;
	UIStatus (_("Loading cache ["), 0, 0);
	unsigned titlestrlen = strlen (_("Loading cache ["));
	int oldnumobjects = 0;
	unsigned count = 1;
	for (struct feed* f = _feed_list; f; f = f->next) {
		// Progress bar
		int numobjects = count*(COLS-titlestrlen-2)/numfeeds-2;
		if (numobjects < 1)
			numobjects = 1;
		if (numobjects > oldnumobjects) {
			DrawProgressBar(numobjects, titlestrlen);
			oldnumobjects = numobjects;
		}
		if (LoadFeed(f) != 0)
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
	char urlsfilename [PATH_MAX];
	snprintf (urlsfilename, sizeof(urlsfilename), "%s/.snownews/urls", getenv("HOME"));

	// Write urls
	FILE* urlfile = fopen (urlsfilename, "w");
	if (!urlfile) {
		syslog (LOG_ERR, "error saving urls: %s", strerror(errno));
		return;
	}
	for (const struct feed* f = _feed_list; f; f = f->next) {
		fputs (f->feedurl, urlfile);
		fputc ('|', urlfile);
		if (f->custom_title)
			fputs (f->title, urlfile);
		fputc ('|', urlfile);
		for (const struct feedcategories* c = f->feedcategories; c; c = c->next) {
			fputs (c->name, urlfile);
			if (c->next)	// Only add a colon of we run the loop again!
				fputc (',', urlfile);
		}
		fputc ('|', urlfile);
		if (f->perfeedfilter != NULL)
			fputs (f->perfeedfilter, urlfile);

		fputc ('\n', urlfile);		// Add newline character.
	}
	fclose (urlfile);
	_feed_list_changed = false;
}

static void WriteFeedCache (const struct feed* feed)
{
	char* hashme = Hashify(feed->feedurl);
	char cachefilename [PATH_MAX];
	snprintf (cachefilename, sizeof(cachefilename), "%s/.snownews/cache/%s", getenv("HOME"), hashme);
	free (hashme);

	// Check if the feed has been modified since last loaded from this file
	struct stat cachest;
	if (0 == stat (cachefilename, &cachest) && cachest.st_mtime >= feed->mtime)
		return;

	FILE* cache = fopen (cachefilename, "w");
	if (!cache) {
		syslog (LOG_ERR, "error writing cache file '%s': %s", cachefilename, strerror(errno));
		return;
	}

	fputs ("<?xml version=\"1.0\" ?>\n\n<rdf:RDF\n  xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"\n  xmlns=\"http://purl.org/rss/1.0/\"\n  xmlns:snow=\"http://snownews.kcore.de/ns/1.0/\">\n\n", cache);

	if (feed->lastmodified != NULL) {
		fputs ("<snow:lastmodified>", cache);
		fputs (feed->lastmodified, cache);
		fputs ("</snow:lastmodified>\n", cache);
	}

	fputs ("<channel rdf:about=\"", cache);

	char* encoded = (char*) xmlEncodeEntitiesReentrant (NULL, (xmlChar*) feed->feedurl);
	fputs (encoded, cache);
	free (encoded);

	fputs ("\">\n<title>", cache);
	if (feed->original != NULL) {
		encoded = (char *)xmlEncodeEntitiesReentrant (NULL, (xmlChar *)feed->original);
		fputs (encoded, cache);
		free (encoded);
	} else if (feed->title != NULL) {
		encoded = (char *)xmlEncodeEntitiesReentrant (NULL, (xmlChar *)feed->title);
		fputs (encoded, cache);
		free (encoded);
	}
	fputs ("</title>\n<link>", cache);
	if (feed->link != NULL) {
		encoded = (char *)xmlEncodeEntitiesReentrant (NULL, (xmlChar *)feed->link);
		fputs (encoded, cache);
		free (encoded);
	}
	fputs ("</link>\n<description>", cache);
	if (feed->description != NULL) {
		encoded = (char *)xmlEncodeEntitiesReentrant (NULL, (xmlChar *)feed->description);
		fputs (encoded, cache);
		free (encoded);
	}
	fputs ("</description>\n</channel>\n\n", cache);

	for (const struct newsitem* item = feed->items; item; item = item->next) {
		fputs ("<item rdf:about=\"", cache);

		if (item->data->link != NULL) {
			encoded = (char *)xmlEncodeEntitiesReentrant (NULL, (xmlChar *)item->data->link);
			fputs (encoded, cache);
			free (encoded);
		}
		fputs ("\">\n<title>", cache);
		if (item->data->title != NULL) {
			encoded = (char *)xmlEncodeEntitiesReentrant (NULL, (xmlChar *)item->data->title);
			fputs (encoded, cache);
			free (encoded);
		}
		fputs ("</title>\n<link>", cache);
		if (item->data->link != NULL) {
			encoded = (char *)xmlEncodeEntitiesReentrant (NULL, (xmlChar *)item->data->link);
			fputs (encoded, cache);
			free (encoded);
		}
		fputs ("</link>\n<description>", cache);
		if (item->data->description != NULL) {
			encoded = (char *)xmlEncodeEntitiesReentrant (NULL, (xmlChar *)item->data->description);
			fputs (encoded, cache);
			free (encoded);
		}
		fputs ("</description>\n<snow:readstatus>", cache);
		putc ('0'+item->data->readstatus, cache);
		fputs ("</snow:readstatus>\n<snow:hash>", cache);
		if (item->data->hash)
			fputs (item->data->hash, cache);
		fputs ("</snow:hash>\n<snow:date>", cache);
		fprintf (cache, "%u", item->data->date);
		fputs ("</snow:date>\n</item>\n\n", cache);
	}
	fputs ("</rdf:RDF>", cache);
	fclose (cache);
}

// Write in memory structures to disk cache.
// Usually called before program exit.
void WriteCache (void) {
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
		int numobjects = count*(COLS-titlestrlen-2)/numfeeds-2;
		if (numobjects < 1)
			numobjects = 1;
		if (numobjects > oldnumobjects) {
			DrawProgressBar(numobjects, titlestrlen);
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
