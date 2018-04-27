// This file is part of Snownews - A lightweight console RSS newsreader
//
// Copyright (c) 2003-2004 Rene Puls <rpuls@gmx.net>
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

#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "xmlparse.h"
#include "conversions.h"

int saverestore;
struct newsitem *copy;
struct newsitem *firstcopy;

static xmlChar const * const dcNs = (unsigned char *)"http://purl.org/dc/elements/1.1/";
static xmlChar const * const snowNs = (unsigned char *)"http://snownews.kcore.de/ns/1.0/";

/* Wird während des Parsens aufgerufen, falls wir auf ein <channel> Element
   treffen. Die Funktion gibt ein neues Struct für den Newsfeed zurück. */

void parse_rdf10_channel(struct feed * feed, xmlDocPtr doc, xmlNodePtr node) {
	xmlNodePtr cur;
	
	/* Free everything before we write to it again. */
	free (feed->title);
	free (feed->link);
	free (feed->description);
		
	if (feed->items != NULL) {
		while (feed->items->next_ptr != NULL) {
			feed->items = feed->items->next_ptr;
			free (feed->items->prev_ptr->data->title);
			free (feed->items->prev_ptr->data->link);
			free (feed->items->prev_ptr->data->description);
			free (feed->items->prev_ptr->data->hash);
			free (feed->items->prev_ptr->data);
			free (feed->items->prev_ptr);
		}
		free (feed->items->data->title);
		free (feed->items->data->link);
		free (feed->items->data->description);
		free (feed->items->data->hash);
		free (feed->items->data);
		free (feed->items);
	}
	
	/* Im Augenblick haben wir noch keine Items, also die Liste auf NULL setzen. */
	feed->items = NULL;
	feed->title = NULL;
	feed->link= NULL;
	feed->description = NULL;
	
	/* Alle Tags im <channel> Tag durchgehen und die Informationen extrahieren */
	for (cur = node; cur != NULL; cur = cur->next) {
		if (cur->type != XML_ELEMENT_NODE)
			continue;
		if (xmlStrcmp(cur->name, (unsigned char *)"title") == 0) {
			feed->title = (char *)xmlNodeListGetString(doc, cur->children, 1);
			CleanupString (feed->title, 1);
			/* Remove trailing newline */
			if (feed->title != NULL) {
				if (strlen(feed->title) > 1) {
					if (feed->title[strlen(feed->title)-1] == '\n')
						feed->title[strlen(feed->title)-1] = '\0';
				}
			}
		}
		else if (xmlStrcmp(cur->name, (unsigned char *)"link") == 0) {
			feed->link = (char *)xmlNodeListGetString(doc, cur->children, 1);
			/* Remove trailing newline */
			if (feed->link != NULL) {
				if (strlen(feed->link) > 1) {
					if (feed->link[strlen(feed->link)-1] == '\n')
						feed->link[strlen(feed->link)-1] = '\0';
				}
			}
		}
		else if (xmlStrcmp(cur->name, (unsigned char *)"description") == 0) {
			feed->description = (char *)xmlNodeListGetString(doc, cur->children, 1);
			CleanupString (feed->description, 0);
		}
	}
}


void parse_rdf20_channel(struct feed * feed, xmlDocPtr doc, xmlNodePtr node)
{
	xmlNodePtr cur;
	
	/* Free everything before we write to it again. */
	free (feed->title);
	free (feed->link);
	free (feed->description);
		
	if (feed->items != NULL) {
		while (feed->items->next_ptr != NULL) {
			feed->items = feed->items->next_ptr;
			free (feed->items->prev_ptr->data->title);
			free (feed->items->prev_ptr->data->link);
			free (feed->items->prev_ptr->data->description);
			free (feed->items->prev_ptr->data->hash);
			free (feed->items->prev_ptr->data);
			free (feed->items->prev_ptr);
		}
		free (feed->items->data->title);
		free (feed->items->data->link);
		free (feed->items->data->description);
		free (feed->items->data->hash);
		free (feed->items->data);
		free (feed->items);
	}
	
	/* Im Augenblick haben wir noch keine Items, also die Liste auf NULL setzen. */
	feed->items = NULL;
	feed->title = NULL;
	feed->link = NULL;
	feed->description = NULL;
	
	/* Alle Tags im <channel> Tag durchgehen und die Informationen extrahieren */
	for (cur = node; cur != NULL; cur = cur->next) {
		if (cur->type != XML_ELEMENT_NODE)
			continue;
		if (xmlStrcmp(cur->name, (unsigned char *)"title") == 0) {
			feed->title = (char *)xmlNodeListGetString(doc, cur->children, 1);
			CleanupString (feed->title, 1);
			/* Remove trailing newline */
			if (feed->title != NULL) {
				if (strlen(feed->title) > 1) {
					if (feed->title[strlen(feed->title)-1] == '\n')
						feed->title[strlen(feed->title)-1] = '\0';
				}
			}
		}
		else if (xmlStrcmp(cur->name, (unsigned char *)"link") == 0) {
			feed->link = (char *)xmlNodeListGetString(doc, cur->children, 1);
			/* Remove trailing newline */
			if (feed->link != NULL) {
				if (strlen(feed->link) > 1) {
					if (feed->link[strlen(feed->link)-1] == '\n')
						feed->link[strlen(feed->link)-1] = '\0';
				}
			}
		}
		else if (xmlStrcmp(cur->name, (unsigned char *)"description") == 0) {
			feed->description = (char *)xmlNodeListGetString(doc, cur->children, 1);
			CleanupString (feed->description, 0);
		} else if (xmlStrcmp(cur->name, (unsigned char *)"item") == 0) {
			parse_rdf10_item(feed, doc, cur->children);
		}
	}
}

/* Diese Funktion wird jedes Mal aufgerufen, wenn wir auf ein <item> treffen.
   Als Parameter benötigt sie den aktuellen Newsfeed (struct newsfeed *),
   sowie das aktuelle XML Document-Handle und das aktuelle Element, beide
   kommen direkt von der libxml. */

void parse_rdf10_item(struct feed *feed, xmlDocPtr doc, xmlNodePtr node) 
{
	xmlNodePtr cur;
	xmlChar *readstatusstring;
	char *guid = NULL;
	char *date_str = NULL;
	struct newsitem *item;
	struct newsitem *current;
	
	/* Speicher für ein neues Newsitem reservieren */
	item = malloc(sizeof (struct newsitem));
	item->data = malloc (sizeof (struct newsdata));
	
	item->data->title = NULL;
	item->data->link = NULL;
	item->data->description = NULL;
	item->data->hash = NULL;
	item->data->readstatus = 0;
	item->data->parent = feed;
	item->data->date = 0;
	
	/* Alle Tags im <item> Tag durchgehen und die Informationen extrahieren.
	   Selbe Vorgehensweise wie in der parse_channel() Funktion */
	for (cur = node; cur != NULL; cur = cur->next) {
		if (cur->type != XML_ELEMENT_NODE)
			continue;
/* Basic RSS */
/* Title */
		if (xmlStrcmp(cur->name, (unsigned char *)"title") == 0) {
			item->data->title = (char *)xmlNodeListGetString(doc, cur->children, 1);
			CleanupString (item->data->title, 1);
			/* Remove trailing newline */
			if (item->data->title != NULL) {
				if (strlen(item->data->title) > 1) {
					if (item->data->title[strlen(item->data->title)-1] == '\n')
						item->data->title[strlen(item->data->title)-1] = '\0';
				}
			}
/* link */
		} else if (xmlStrcmp(cur->name, (unsigned char *)"link") == 0) {
			item->data->link = (char *)xmlNodeListGetString(doc, cur->children, 1);
			/* Remove trailing newline */
			if (item->data->link != NULL) {
				if (strlen(item->data->link) > 1) {
					if (item->data->link[strlen(item->data->link)-1] == '\n')
						item->data->link[strlen(item->data->link)-1] = '\0';
				}
			}
/* Description */
		} else if (xmlStrcmp(cur->name, (unsigned char *)"description") == 0) {
			item->data->description = (char *)xmlNodeListGetString(doc, cur->children, 1);
			CleanupString (item->data->description, 0);
/* Userland extensions (No namespace!)*/
/* guid */
		} else if (xmlStrcmp(cur->name, (unsigned char *)"guid") == 0) {
			guid = (char *)xmlNodeListGetString(doc, cur->children, 1);
/* pubDate */
		} else if (xmlStrcmp(cur->name, (unsigned char *)"pubDate") == 0) {
			date_str = (char *)xmlNodeListGetString(doc, cur->children, 1);
			item->data->date = pubDateToUnix(date_str);
			xmlFree (date_str);
/* Dublin Core */
/* dc:date */
		} else if (cur->ns &&
		           (xmlStrcmp(cur->ns->href, dcNs) == 0) && 
		           (xmlStrcmp(cur->name, (unsigned char *)"date") == 0)) {
		    date_str = (char *)xmlNodeListGetString(doc, cur->children, 1);
			item->data->date = ISODateToUnix(date_str);
			xmlFree (date_str);
/* Internal usage */
/* Obsolete/backware compat/migration code */
		} else if (xmlStrcmp(cur->name, (unsigned char *)"readstatus") == 0) {
			/* Will cause memory leak otherwise, xmlNodeListGetString must be freed. */
			readstatusstring = xmlNodeListGetString(doc, cur->children, 1);
			item->data->readstatus = atoi ((char *)readstatusstring);
			xmlFree (readstatusstring);
/* Using snow namespace */
		} else if (cur->ns && (xmlStrcmp(cur->ns->href, snowNs) == 0) &&
		           (xmlStrcmp(cur->name, (unsigned char *)"readstatus") == 0)) {
			readstatusstring = xmlNodeListGetString(doc, cur->children, 1);
			item->data->readstatus = atoi ((char *)readstatusstring);
			xmlFree (readstatusstring);
		} else if (cur->ns && (xmlStrcmp(cur->ns->href, snowNs) == 0) &&
		           (xmlStrcmp(cur->name, (unsigned char *)"hash") == 0)) {
			item->data->hash = (char *)xmlNodeListGetString(doc, cur->children, 1);
		} else if (cur->ns && (xmlStrcmp(cur->ns->href, snowNs) == 0) &&
		           (xmlStrcmp(cur->name, (unsigned char *)"date") == 0)) {
			date_str = (char *)xmlNodeListGetString(doc, cur->children, 1);
			item->data->date = strtoll(date_str, NULL, 10);
			xmlFree (date_str);
		}
	}
	
	/* If we have loaded the hash from disk cache, don't regenerate it.
	   <guid> is not saved in the cache, thus we would generate a different
	   hash than the one from the live feed. */
	if (item->data->hash == NULL) {
		const char* hashitems[] = { item->data->title, item->data->link, guid, NULL };
		item->data->hash = genItemHash (hashitems, 3);
		xmlFree (guid);
	}
	
	/* If saverestore == 1, restore readstatus. */
	if (saverestore == 1) {
		for (current = firstcopy; current != NULL; current = current->next_ptr) {
			if (strcmp(item->data->hash, current->data->hash) == 0) {
				item->data->readstatus = current->data->readstatus;
				break;
			}
		}
	}
	
	item->next_ptr = NULL;
	if (feed->items == NULL) {
		item->prev_ptr = NULL;
		feed->items = item;
	} else {
		item->prev_ptr = feed->items;
		while (item->prev_ptr->next_ptr != NULL)
			item->prev_ptr = item->prev_ptr->next_ptr;
		item->prev_ptr->next_ptr = item;
	}
}


/* rrr */

int DeXML (struct feed * cur_ptr) {
	xmlDocPtr doc;
	xmlNodePtr cur;
	struct newsitem *cur_item;
	char *converted;
	
	if (cur_ptr->feed == NULL)
		return -1;
	
	saverestore = 0;
	/* Wenn cur_ptr->items != NULL dann können wir uns item->readstatus
	   zwischenspeichern. */
	if (cur_ptr->items != NULL) {
		saverestore = 1;
	
		firstcopy = NULL;
		
		/* Copy current newsitem struct. */	
		for (cur_item = cur_ptr->items; cur_item != NULL; cur_item = cur_item->next_ptr) {
			copy = malloc (sizeof(struct newsitem));
			copy->data = malloc (sizeof (struct newsdata));
			copy->data->title = NULL;
			copy->data->link = NULL;
			copy->data->description = NULL;
			copy->data->hash = NULL;
			copy->data->readstatus = cur_item->data->readstatus;
			if (cur_item->data->hash != NULL)
				copy->data->hash = strdup (cur_item->data->hash);
			
			copy->next_ptr = NULL;
			if (firstcopy == NULL) {
				copy->prev_ptr = NULL;
				firstcopy = copy;
			} else {
				copy->prev_ptr = firstcopy;
				while (copy->prev_ptr->next_ptr != NULL)
					copy->prev_ptr = copy->prev_ptr->next_ptr;
				copy->prev_ptr->next_ptr = copy;
			}
		}
	}
	
	/* xmlRecoverMemory:
	   parse an XML in-memory document and build a tree.
       In case the document is not Well Formed, a tree is built anyway. */
	doc = xmlRecoverMemory(cur_ptr->feed, strlen(cur_ptr->feed));
	
	if (doc == NULL)
		return 2;
	
	/* Das Root-Element finden (in unserem Fall sollte es "<RDF:RDF>" heißen.
	   Dabei wird das RDF: Prefix fürs Erste ignoriert, bis der Jaguar
	   herausfindet, wie man das genau auslesen kann (jau). */
	cur = xmlDocGetRootElement(doc);
	
	if (cur == NULL) {
		xmlFreeDoc (doc);
		return 2;
	}
	
	/* Überprüfen, ob das Element auch wirklich <RDF> heißt */
	if (xmlStrcmp(cur->name, (unsigned char *)"RDF") == 0) {
	
		/* Jetzt gehen wir alle Elemente im Dokument durch. Diese Schleife
		   selbst läuft jedoch nur durch die Elemente auf höchster Ebene
		   (bei HTML wären das nur HEAD und BODY), wandert also nicht die 
		   gesamte Struktur nach unten durch. Dafür sind die Funktionen zuständig, 
		   die wir dann in der Schleife selbst aufrufen. */
		for (cur = cur->children; cur != NULL; cur = cur->next) {
			if (cur->type != XML_ELEMENT_NODE)
				continue;
			if (xmlStrcmp(cur->name, (unsigned char *)"channel") == 0)
				parse_rdf10_channel(cur_ptr, doc, cur->children);
			if (xmlStrcmp(cur->name, (unsigned char *)"item") == 0)
				parse_rdf10_item(cur_ptr, doc, cur->children);
			/* Last-Modified is only used when reading from internal feeds (disk cache). */
			if (cur->ns && (xmlStrcmp(cur->ns->href, snowNs) == 0) &&
				(xmlStrcmp(cur->name, (unsigned char *)"lastmodified") == 0))
				cur_ptr->lastmodified = (char *)xmlNodeListGetString(doc, cur->children, 1);
		}
	} else if (xmlStrcmp(cur->name, (unsigned char *)"rss") == 0) {
		for (cur = cur->children; cur != NULL; cur = cur->next) {
			if (cur->type != XML_ELEMENT_NODE)
				continue;
			if (xmlStrcmp(cur->name, (unsigned char *)"channel") == 0)
				parse_rdf20_channel(cur_ptr, doc, cur->children);
		}
	} else {
		xmlFreeDoc(doc);
		return 3;
	}

	xmlFreeDoc(doc);
	
	if (saverestore == 1) {
		/* free struct newsitem *copy. */
		while (firstcopy->next_ptr != NULL) {
			firstcopy = firstcopy->next_ptr;
			free (firstcopy->prev_ptr->data->hash);
			free (firstcopy->prev_ptr->data);
			free (firstcopy->prev_ptr);
		}
		free (firstcopy->data->hash);
		free (firstcopy->data);
		free (firstcopy);
	}
	
	if (cur_ptr->original != NULL)
		free (cur_ptr->original);

	/* Set -> title to something if it's a NULL pointer to avoid crash with strdup below. */
	if (cur_ptr->title == NULL)
		cur_ptr->title = strdup (cur_ptr->feedurl);
	converted = iconvert (cur_ptr->title);
	if (converted != NULL) {
		free (cur_ptr->title);
		cur_ptr->title = converted;
	}
	cur_ptr->original = strdup (cur_ptr->title);
	
	/* Restore custom title. */
	if (cur_ptr->override != NULL) {
		free (cur_ptr->title);
		cur_ptr->title = strdup (cur_ptr->override);
	}
		
	return 0;
}
