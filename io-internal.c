/* Snownews - A lightweight console RSS newsreader
 * $Id: io-internal.c 1146 2005-09-10 10:05:21Z kiza $
 * 
 * Copyright 2003-2004 Oliver Feiler <kiza@kcore.de>
 * http://kiza.kcore.de/software/snownews/
 *
 * io-internal.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
 
#include <ncurses.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>

#include "config.h"

#include "main.h"
#include "conversions.h"
#include "filters.h"
#include "categories.h"
#include "netio.h"
#include "xmlparse.h"
#include "ui-support.h"
#include "io-internal.h"

extern char *browser;

struct feed * newFeedStruct (void) {
	struct feed *new;
	
	new = malloc(sizeof(struct feed));
	
	new->feed = NULL;
	new->content_length = 0;
	new->title = NULL;
	new->link = NULL;
	new->description = NULL;
	new->lastmodified = NULL;
	new->lasthttpstatus = 0;
	new->content_type = NULL;
	new->netio_error = NET_ERR_OK;
	new->connectresult = 0;
	new->cookies = NULL;
	new->authinfo = NULL;
	new->servauth = NULL;
	new->items = NULL;
	new->problem = 0;
	new->override = NULL;
	new->original = NULL;
	new->perfeedfilter = NULL;
	new->execurl = 0;
	new->smartfeed = 0;
	new->feedcategories = NULL;
	
	return new;
}

void GetHTTPErrorString (char * errorstring, int size, int httpstatus) {
	switch (httpstatus) {
		case 400:
			snprintf(errorstring, size, "Bad request");
			break;
		case 402:
			snprintf(errorstring, size, "Payment required");
			break;
		case 403:
			snprintf(errorstring, size, "Access denied");
			break;
		case 500:
			snprintf(errorstring, size, "Internal server error");
			break;
		case 501:
			snprintf(errorstring, size, "Not implemented");
			break;
		case 502:
		case 503:
			snprintf(errorstring, size, "Service unavailable");
			break;
		default:
			sprintf(errorstring, "HTTP %d!", httpstatus);
	}
}

void PrintUpdateError (int suppressoutput, struct feed * cur_ptr) {
	netio_error_type err;
	char errstr[256];
	char httperrstr[64];

	err = cur_ptr->netio_error;
	
	if (!suppressoutput) {
		switch (err) {
			case NET_ERR_OK:
				break;
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
			case NET_ERR_HTTP_NON_200:
				GetHTTPErrorString(httperrstr, sizeof(httperrstr), cur_ptr->lasthttpstatus);
				snprintf(errstr, sizeof(errstr), _("%s: Could not download feed: %s"), cur_ptr->title, httperrstr);
				break;
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
		/* Must be inside if(!suppressoutput) statement! */
		UIStatus(errstr, 2, 1);
	}
	printlog(cur_ptr, errstr);
}


/* Update given feed from server.
 * Reload XML document and replace in memory cur_ptr->feed with it.
 */
int UpdateFeed (struct feed * cur_ptr) {
	char *tmpname;
	char *freeme;

	if (cur_ptr == NULL)
		return 1;
	
	/* Smart feeds are generated in the fly. */
	if (cur_ptr->smartfeed == 1)
		return 0;
		
	/* If current feed is an exec URL, run that command, otherwise fetch
	   resource from network. */
	if (cur_ptr->execurl) {
		FilterExecURL (cur_ptr);
	} else {
		/* Need to work on a copy of ->feedurl, because DownloadFeed()
		   changes the pointer. */
		tmpname = strdup (cur_ptr->feedurl);
		freeme = tmpname;		/* Need to make a copy, otherwise we cannot free
								   all RAM. */
	
		free (cur_ptr->feed);

		cur_ptr->feed = DownloadFeed (tmpname, cur_ptr, 0);

		free (freeme);

		/* Set title and link structure to something.
		 * To the feedurl in this case so the program show something
		 * as placeholder instead of crash. */
		if (cur_ptr->title == NULL)
			cur_ptr->title = strdup (cur_ptr->feedurl);
		if (cur_ptr->link == NULL)
			cur_ptr->link = strdup (cur_ptr->feedurl);

		/* If the download function returns a NULL pointer return from here. */
		if (cur_ptr->feed == NULL) {
			if (cur_ptr->problem == 1)
				PrintUpdateError (0, cur_ptr);
			return 1;
		}
	}
	
	/* Feed downloaded content through the defined filter. */
	if (cur_ptr->perfeedfilter != NULL)
		FilterPipeNG (cur_ptr);
	
	/* If there is no feed, return. */
	if (cur_ptr->feed == NULL)
		return 1;
	
	if ((DeXML (cur_ptr)) != 0) {
		UIStatus (_("Invalid XML! Cannot parse this feed!"), 2, 1);
		/* Activate feed problem flag. */
		cur_ptr->problem = 1;
		return 1;
	}
	
	/* We don't need these anymore. Free the raw XML to save some memory. */
	free (cur_ptr->feed);
	cur_ptr->feed = NULL;
		
	return 0;
}


int UpdateAllFeeds (void) {
	struct feed *cur_ptr;
	
	for (cur_ptr = first_ptr; cur_ptr != NULL; cur_ptr = cur_ptr->next_ptr) {
		if ((UpdateFeed (cur_ptr)) != 0)
			continue;
	}
	return 0;
}

/* Load feed from disk. And call UpdateFeed if neccessary. */
int LoadFeed (struct feed * cur_ptr) {
	int len = 0;				/* Internal usage for realloc. */
	int retval;
	char filebuf[4096];			/* File I/O block buffer. */
	char file[512];
 	char *hashme;				/* Hashed filename. */
 	char tmp[1024];
	FILE *cache;
	
	/* Smart feeds are generated in the fly. */
	if (cur_ptr->smartfeed == 1)
		return 0;
	
	hashme = Hashify(cur_ptr->feedurl);
	snprintf (file, sizeof(file), "%s/.snownews/cache/%s", getenv("HOME"), hashme);
	free (hashme);
	cache = fopen (file, "r");
	
	if (cache == NULL) {
		snprintf (tmp, sizeof(tmp), _("Cache for %s is toast. Reloading from server..."), cur_ptr->feedurl);
		UIStatus (tmp, 0, 0);
		
		if ((UpdateFeed (cur_ptr)) != 0)
			return 1;
		return 0;
	}

	/* Read complete cachefile. */
	while (!feof(cache)) {
		/* Use binary read, UTF-8 data! */
		retval = fread (filebuf, 1, sizeof(filebuf), cache);
		if (retval == 0)
			break;
		cur_ptr->feed = realloc (cur_ptr->feed, len+retval + 1);
		memcpy (cur_ptr->feed+len, filebuf, retval);
		len += retval;
		if (retval != 4096)
			break;
	}
	
	cur_ptr->content_length = len;
	
	if (cur_ptr->feed == NULL)
		return 1;
		
	cur_ptr->feed[len] = '\0';
	
	fclose (cache);
	
	/* After loading DeXMLize the mess. */
	if ((DeXML (cur_ptr)) != 0) {
		/* If loading the feed from the disk fails, try downloading from the net. */
		if ((UpdateFeed (cur_ptr)) != 0) {
			/* And if that fails as well, just continue without this feed. */
			snprintf (tmp, sizeof(tmp), _("Could not load %s!"), cur_ptr->feedurl);
			UIStatus (tmp, 2, 1);
		}
	}
	
	free (cur_ptr->feed);
	cur_ptr->feed = NULL;
	
	return 0;
}


int LoadAllFeeds (int numfeeds) {
	float numobjperfeed = 0;				/* Number of "progress bar" objects to draw per feed. */
	int count = 1;
	int numobjects = 0;
	int oldnumobjects = 0;
	int titlestrlen;
	struct feed *cur_ptr;
	
	UIStatus (_("Loading cache ["), 0, 0);
	titlestrlen = strlen (_("Loading cache ["));
	
	if (numfeeds > 0)
		numobjperfeed = (COLS-titlestrlen-2)/(double) numfeeds;
	
	for (cur_ptr = first_ptr; cur_ptr != NULL; cur_ptr = cur_ptr->next_ptr) {
		/* Progress bar */
		numobjects = (count * numobjperfeed) - 2;
		if (numobjects < 1)
			numobjects = 1;
		
		if (numobjects > oldnumobjects) {
			DrawProgressBar(numobjects, titlestrlen);
			oldnumobjects = numobjects;
		}
		
		if ((LoadFeed (cur_ptr)) != 0) {
			continue;
		}
		count++;
	}
	return 0;
}


/* Write in memory structures to disk cache.
 * Usually called before program exit.
 */
void WriteCache (void) {
	char file[512];				/* File locations. */
	char *hashme;				/* Cache file name. */
	char readstatus[2];
	char snowdate[32];
	char syscall[512];
	FILE *configfile;
	FILE *cache;
	struct feed *cur_ptr;
	struct newsitem *item;
	struct stat filetest;
	struct feedcategories *category;
	char *encoded;
	float numobjperfeed = 0;				/* Number of "progress bar" objects to draw per feed. */
	int count = 1;
	int numobjects = 0;
	int oldnumobjects = 0;
	int numfeeds = 0;
	int titlestrlen;
	
	UIStatus (_("Saving settings ["), 0, 0);
	titlestrlen = strlen (_("Saving settings ["));
	
	snprintf (file, sizeof(file), "%s/.snownews/browser", getenv("HOME"));
	configfile = fopen (file, "w+");
	if (configfile == NULL) {
		MainQuit (_("Save settings (browser)"), strerror(errno));
	}
	fputs (browser, configfile);
	fclose (configfile);
	
	snprintf (file, sizeof(file), "%s/.snownews/urls", getenv("HOME"));
	
	/* Make a backup of urls. This approach is really broken! */
	if ((stat (file, &filetest)) != -1) {
		if ((filetest.st_mode & S_IFREG) == S_IFREG) {
			snprintf (syscall, sizeof(file), "mv -f %s/.snownews/urls %s/.snownews/urls.bak", getenv("HOME"), getenv("HOME"));
			system (syscall);
		}
	}
	
	snprintf (file, sizeof(file), "%s/.snownews/urls.new", getenv("HOME"));
	configfile = fopen (file, "w+");
	if (configfile == NULL) {
		MainQuit (_("Save settings (urls)"), strerror(errno));
	}
	
	/* Unoptimized vs. KISS */
	for (cur_ptr = first_ptr; cur_ptr != NULL; cur_ptr = cur_ptr->next_ptr)
		numfeeds++;
	
	if (numfeeds > 0)
		numobjperfeed = (COLS-titlestrlen-2)/(double) numfeeds;
	
	for (cur_ptr = first_ptr; cur_ptr != NULL; cur_ptr = cur_ptr->next_ptr) {
		/* Progress bar */
		numobjects = (count * numobjperfeed) - 2;
		if (numobjects < 1)
			numobjects = 1;
			
		if (numobjects > oldnumobjects) {
			DrawProgressBar(numobjects, titlestrlen);
			oldnumobjects = numobjects;
		}
		count++;
		
		fputs (cur_ptr->feedurl, configfile);
		
		fputc ('|', configfile);
		if (cur_ptr->override != NULL)
			fputs (cur_ptr->title, configfile);
		
		fputc ('|', configfile);
		if (cur_ptr->feedcategories != NULL) {
			for (category = cur_ptr->feedcategories; category != NULL; category = category->next_ptr) {
				fputs (category->name, configfile);
				/* Only add a colon of we run the loop again! */
				if (category->next_ptr != NULL)
					fputc (',', configfile);
			}
		}
		
		fputc ('|', configfile);
		if (cur_ptr->perfeedfilter != NULL)
			fputs (cur_ptr->perfeedfilter, configfile);
			
		fputc ('\n', configfile);		/* Add newline character. */
		
		/* Discard smart feeds from cache. */
		if (cur_ptr->smartfeed)
			continue;
		
		/* 
		 * Write cache.
		 */
		hashme = Hashify(cur_ptr->feedurl);
		snprintf (file, sizeof(file), "%s/.snownews/cache/%s", getenv("HOME"), hashme);
		free (hashme);
		cache = fopen (file, "w+");

		if (cache == NULL)
			MainQuit (_("Writing cache file"), strerror(errno));
		
		fputs ("<?xml version=\"1.0\" ?>\n\n<rdf:RDF\n  xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"\n  xmlns=\"http://purl.org/rss/1.0/\"\n  xmlns:snow=\"http://snownews.kcore.de/ns/1.0/\">\n\n", cache);
		
		if (cur_ptr->lastmodified != NULL) {
			fputs ("<snow:lastmodified>", cache);
			fputs (cur_ptr->lastmodified, cache);
			fputs ("</snow:lastmodified>\n", cache);
		}
				
		fputs ("<channel rdf:about=\"", cache);
		
		encoded = (char *)xmlEncodeEntitiesReentrant (NULL, (xmlChar *)cur_ptr->feedurl);
		fputs (encoded, cache);
		free (encoded);
		
		fputs ("\">\n<title>", cache);
		if (cur_ptr->original != NULL) {
			encoded = (char *)xmlEncodeEntitiesReentrant (NULL, (xmlChar *)cur_ptr->original);
			fputs (encoded, cache);
			free (encoded);
		} else if (cur_ptr->title != NULL) {
			encoded = (char *)xmlEncodeEntitiesReentrant (NULL, (xmlChar *)cur_ptr->title);
			fputs (encoded, cache);
			free (encoded);
		}
		fputs ("</title>\n<link>", cache);
		if (cur_ptr->link != NULL) {
			encoded = (char *)xmlEncodeEntitiesReentrant (NULL, (xmlChar *)cur_ptr->link);
			fputs (encoded, cache);
			free (encoded);
		}
		fputs ("</link>\n<description>", cache);
		if (cur_ptr->description != NULL) {
			encoded = (char *)xmlEncodeEntitiesReentrant (NULL, (xmlChar *)cur_ptr->description);
			fputs (encoded, cache);
			free (encoded);
		}
		fputs ("</description>\n</channel>\n\n", cache);
		
		for (item = cur_ptr->items; item != NULL; item = item->next_ptr) {
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
			snprintf (readstatus, sizeof(readstatus), "%d", item->data->readstatus);
			fputs (readstatus, cache);			
			fputs ("</snow:readstatus>\n<snow:hash>", cache);
			if (item->data->hash != NULL) {
				fputs (item->data->hash, cache);
			}
			fputs ("</snow:hash>\n<snow:date>", cache);
			snprintf (snowdate, sizeof(snowdate), "%d", item->data->date);
			fputs (snowdate, cache);
			fputs ("</snow:date>\n</item>\n\n", cache);
		}
		
		fputs ("</rdf:RDF>", cache);
		fclose (cache);
	}
	fclose (configfile);
	
	snprintf (syscall, sizeof(file), "mv -f %s/.snownews/urls.new %s/.snownews/urls", getenv("HOME"), getenv("HOME"));
	system (syscall);
	
	return;
}

void printlog (struct feed * feed, const char * text) {
	char *timestring;
	time_t t;
	
	t = time(NULL);
	timestring = ctime(&t);
	timestring[strlen(timestring)-1] = 0;
	
	fprintf (stderr, "%s: (%s) %s\n", timestring, feed->feedurl, text);
}

void printlogSimple (const char * text) {
	char *timestring;
	time_t t;
	
	t = time(NULL);
	timestring = ctime(&t);
	timestring[strlen(timestring)-1] = 0;
	
	fprintf (stderr, "%s: %s\n", timestring, text);
}
