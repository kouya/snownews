// This file is part of Snownews - A lightweight console RSS newsreader
//
// Copyright (c) 2003-2004 Rene Puls <rpuls@gmx.net>
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

#include "parsefeed.h"
#include "conversions.h"
#include <libxml/parser.h>

//{{{ Local variables --------------------------------------------------

static bool saverestore = false;
static struct newsitem* copy = NULL;
static struct newsitem* firstcopy = NULL;

static const xmlChar* dcNs = (const xmlChar*) "http://purl.org/dc/elements/1.1/";
static const xmlChar* snowNs = (const xmlChar*) "http://snownews.kcore.de/ns/1.0/";

//}}}-------------------------------------------------------------------
//{{{ free_feed

static void free_feed (struct feed* feed)
{
    free (feed->title);
    free (feed->link);
    free (feed->description);
    if (feed->items) {
	while (feed->items->next) {
	    feed->items = feed->items->next;
	    free (feed->items->prev->data->title);
	    free (feed->items->prev->data->link);
	    free (feed->items->prev->data->description);
	    free (feed->items->prev->data->hash);
	    free (feed->items->prev->data);
	    free (feed->items->prev);
	}
	free (feed->items->data->title);
	free (feed->items->data->link);
	free (feed->items->data->description);
	free (feed->items->data->hash);
	free (feed->items->data);
	free (feed->items);
    }
    feed->items = NULL;
    feed->title = NULL;
    feed->link = NULL;
    feed->description = NULL;
}

//}}}-------------------------------------------------------------------
//{{{ RSS 1 parsing

// This function is called every time we hit an <item>.  As parameter it
// needs the current newsfeed (struct newsfeed*), as well as the current
// XML Document handle and the current element, both come directly from
// the libxml.

static void parse_rdf10_item (struct feed* feed, xmlDocPtr doc, xmlNodePtr node)
{
    // Reserve memory for a new news item
    struct newsitem* item = calloc (1, sizeof (struct newsitem));
    item->data = calloc (1, sizeof (struct newsdata));
    item->data->parent = feed;

    char* guid = NULL;

    // Go through all the tags in the <item> tag and extract the information.
    // same procedure as in the parse_channel() function
    for (xmlNodePtr cur = node; cur != NULL; cur = cur->next) {
	if (cur->type != XML_ELEMENT_NODE)
	    continue;
	// Basic RSS
	// Title
	if (xmlStrcmp (cur->name, (const xmlChar*) "title") == 0) {
	    item->data->title = (char*) xmlNodeListGetString (doc, cur->children, 1);
	    CleanupString (item->data->title, 1);
	    // Remove trailing newline
	    if (item->data->title != NULL) {
		if (strlen (item->data->title) > 1) {
		    if (item->data->title[strlen (item->data->title) - 1] == '\n')
			item->data->title[strlen (item->data->title) - 1] = '\0';
		}
	    }
	    // link
	} else if (xmlStrcmp (cur->name, (const xmlChar*) "link") == 0) {
	    item->data->link = (char*) xmlNodeListGetString (doc, cur->children, 1);
	    // Remove trailing newline
	    if (item->data->link != NULL) {
		if (strlen (item->data->link) > 1) {
		    if (item->data->link[strlen (item->data->link) - 1] == '\n')
			item->data->link[strlen (item->data->link) - 1] = '\0';
		}
	    }
	    // Description
	} else if (xmlStrcmp (cur->name, (const xmlChar*) "description") == 0) {
	    item->data->description = (char*) xmlNodeListGetString (doc, cur->children, 1);
	    CleanupString (item->data->description, 0);
	    // Userland extensions (No namespace!)
	    // guid
	} else if (xmlStrcmp (cur->name, (const xmlChar*) "guid") == 0) {
	    guid = (char*) xmlNodeListGetString (doc, cur->children, 1);
	    // pubDate
	} else if (xmlStrcmp (cur->name, (const xmlChar*) "pubDate") == 0) {
	    xmlChar* date_str = xmlNodeListGetString (doc, cur->children, 1);
	    item->data->date = pubDateToUnix ((const char*) date_str);
	    xmlFree (date_str);
	    // Dublin Core
	    // dc:date
	} else if (cur->ns && xmlStrcmp (cur->ns->href, dcNs) == 0 && xmlStrcmp (cur->name, (const xmlChar*) "date") == 0) {
	    xmlChar* date_str = xmlNodeListGetString (doc, cur->children, 1);
	    item->data->date = ISODateToUnix ((const char*) date_str);
	    xmlFree (date_str);
	    // Internal usage
	    // Obsolete/backware compat/migration code
	} else if (xmlStrcmp (cur->name, (const xmlChar*) "readstatus") == 0) {
	    // Will cause memory leak otherwise, xmlNodeListGetString must be freed.
	    xmlChar* readstatusstring = xmlNodeListGetString (doc, cur->children, 1);
	    item->data->readstatus = atoi ((const char*) readstatusstring);
	    xmlFree (readstatusstring);
	    // Using snow namespace
	} else if (cur->ns && xmlStrcmp (cur->ns->href, snowNs) == 0 && xmlStrcmp (cur->name, (const xmlChar*) "readstatus") == 0) {
	    xmlChar* readstatusstring = xmlNodeListGetString (doc, cur->children, 1);
	    item->data->readstatus = atoi ((const char*) readstatusstring);
	    xmlFree (readstatusstring);
	} else if (cur->ns && xmlStrcmp (cur->ns->href, snowNs) == 0 && xmlStrcmp (cur->name, (const xmlChar*) "hash") == 0) {
	    item->data->hash = (char*) xmlNodeListGetString (doc, cur->children, 1);
	} else if (cur->ns && xmlStrcmp (cur->ns->href, snowNs) == 0 && xmlStrcmp (cur->name, (const xmlChar*) "date") == 0) {
	    xmlChar* date_str = xmlNodeListGetString (doc, cur->children, 1);
	    item->data->date = atol ((const char*) date_str);
	    xmlFree (date_str);
	}
    }

    // If we have loaded the hash from disk cache, don't regenerate it.
    // <guid> is not saved in the cache, thus we would generate a different
    // hash than the one from the live feed.
    if (item->data->hash == NULL) {
	const char* hashitems[] = { item->data->title, item->data->link, guid, NULL };
	item->data->hash = genItemHash (hashitems, 3);
	xmlFree (guid);
    }
    // If saverestore == true, restore readstatus.
    if (saverestore) {
	for (struct newsitem * i = firstcopy; i; i = i->next) {
	    if (strcmp (item->data->hash, i->data->hash) == 0) {
		item->data->readstatus = i->data->readstatus;
		break;
	    }
	}
    }

    if (!feed->items)
	feed->items = item;
    else {
	item->prev = feed->items;
	while (item->prev->next)
	    item->prev = item->prev->next;
	item->prev->next = item;
    }
}

// Called during parsing, if we look for a <channel> element
// The function returns a new struct for the newsfeed.

static void parse_rdf10_channel (struct feed* feed, xmlDocPtr doc, xmlNodePtr node)
{
    // Free everything before we write to it again.
    free_feed (feed);
    // Go through all the tags in the <channel> tag and extract the information
    for (xmlNodePtr cur = node; cur; cur = cur->next) {
	if (cur->type != XML_ELEMENT_NODE)
	    continue;
	if (xmlStrcmp (cur->name, (const xmlChar*) "title") == 0) {
	    feed->title = (char*) xmlNodeListGetString (doc, cur->children, 1);
	    CleanupString (feed->title, 1);
	    if (feed->title && strlen (feed->title) > 1 && feed->title[strlen (feed->title) - 1] == '\n')
		feed->title[strlen (feed->title) - 1] = '\0';	// Remove trailing newline
	} else if (xmlStrcmp (cur->name, (const xmlChar*) "link") == 0) {
	    feed->link = (char*) xmlNodeListGetString (doc, cur->children, 1);
	    if (feed->link && strlen (feed->link) > 1 && feed->link[strlen (feed->link) - 1] == '\n')
		feed->link[strlen (feed->link) - 1] = '\0';	// Remove trailing newline
	} else if (xmlStrcmp (cur->name, (const xmlChar*) "description") == 0) {
	    feed->description = (char*) xmlNodeListGetString (doc, cur->children, 1);
	    CleanupString (feed->description, 0);
	}
    }
}

//}}}-------------------------------------------------------------------
//{{{ RSS 2 parsing

static void parse_rdf20_channel (struct feed* feed, xmlDocPtr doc, xmlNodePtr node)
{
    // Free everything before we write to it again.
    free_feed (feed);
    // Go through all the tags in the <channel> tag and extract the information
    for (xmlNodePtr cur = node; cur; cur = cur->next) {
	if (cur->type != XML_ELEMENT_NODE)
	    continue;
	if (xmlStrcmp (cur->name, (const xmlChar*) "title") == 0) {
	    feed->title = (char*) xmlNodeListGetString (doc, cur->children, 1);
	    CleanupString (feed->title, 1);
	    if (feed->title && strlen (feed->title) > 1 && feed->title[strlen (feed->title) - 1] == '\n')
		feed->title[strlen (feed->title) - 1] = '\0';	// Remove trailing newline
	} else if (xmlStrcmp (cur->name, (const xmlChar*) "link") == 0) {
	    feed->link = (char*) xmlNodeListGetString (doc, cur->children, 1);
	    if (feed->link && strlen (feed->link) > 1 && feed->link[strlen (feed->link) - 1] == '\n')
		feed->link[strlen (feed->link) - 1] = '\0';	// Remove trailing newline
	} else if (xmlStrcmp (cur->name, (const xmlChar*) "description") == 0) {
	    feed->description = (char*) xmlNodeListGetString (doc, cur->children, 1);
	    CleanupString (feed->description, 0);
	} else if (xmlStrcmp (cur->name, (const xmlChar*) "item") == 0)
	    parse_rdf10_item (feed, doc, cur->children);
    }
}

//}}}-------------------------------------------------------------------
//{{{ Atom parsing

static void parse_atom_entry (struct feed* feed, xmlDocPtr doc, xmlNodePtr node)
{
    // Reserve memory for a new news item
    struct newsitem* item = calloc (1, sizeof (struct newsitem));
    item->data = calloc (1, sizeof (struct newsdata));
    item->data->parent = feed;

    char* guid = NULL;

    // Go through all the tags in the <item> tag and extract the information.
    // same procedure as in the parse_channel() function
    for (xmlNodePtr cur = node; cur != NULL; cur = cur->next) {
	if (cur->type != XML_ELEMENT_NODE)
	    continue;
	// Basic RSS
	// Title
	if (xmlStrcmp (cur->name, (const xmlChar*) "title") == 0) {
	    item->data->title = (char*) xmlNodeListGetString (doc, cur->children, 1);
	    CleanupString (item->data->title, 1);
	    // Remove trailing newline
	    if (item->data->title != NULL) {
		if (strlen (item->data->title) > 1) {
		    if (item->data->title[strlen (item->data->title) - 1] == '\n')
			item->data->title[strlen (item->data->title) - 1] = '\0';
		}
	    }
	    // link
	} else if (xmlStrcmp (cur->name, (const xmlChar*) "link") == 0) {
	    item->data->link = (char*) xmlGetProp (cur, (const xmlChar*) "href");
	    // Remove trailing newline
	    if (item->data->link != NULL) {
		if (strlen (item->data->link) > 1) {
		    if (item->data->link[strlen (item->data->link) - 1] == '\n')
			item->data->link[strlen (item->data->link) - 1] = '\0';
		}
	    }
	    // Description
	} else if (xmlStrcmp (cur->name, (const xmlChar*) "summary") == 0 && !item->data->description) {
	    item->data->description = (char*) xmlNodeListGetString (doc, cur->children, 1);
	    CleanupString (item->data->description, 0);
	    // Userland extensions (No namespace!)
	    // guid
	} else if (xmlStrcmp (cur->name, (const xmlChar*) "content") == 0) {
	    if (item->data->description)
		xmlFree (item->data->description);
	    item->data->description = (char*) xmlNodeListGetString (doc, cur->children, 1);
	    CleanupString (item->data->description, 0);
	    // Userland extensions (No namespace!)
	    // guid
	} else if (xmlStrcmp (cur->name, (const xmlChar*) "id") == 0) {
	    guid = (char*) xmlNodeListGetString (doc, cur->children, 1);
	    // pubDate
	} else if (xmlStrcmp (cur->name, (const xmlChar*) "updated") == 0) {
	    xmlChar* date_str = xmlNodeListGetString (doc, cur->children, 1);
	    item->data->date = ISODateToUnix ((const char*) date_str);
	    xmlFree (date_str);
	}
    }

    // If we have loaded the hash from disk cache, don't regenerate it.
    // <guid> is not saved in the cache, thus we would generate a different
    // hash than the one from the live feed.
    if (item->data->hash == NULL) {
	const char* hashitems[] = { item->data->title, item->data->link, guid, NULL };
	item->data->hash = genItemHash (hashitems, 3);
	xmlFree (guid);
    }
    // If saverestore == true, restore readstatus.
    if (saverestore) {
	for (const struct newsitem* i = firstcopy; i; i = i->next) {
	    if (strcmp (item->data->hash, i->data->hash) == 0) {
		item->data->readstatus = i->data->readstatus;
		break;
	    }
	}
    }

    if (!feed->items)
	feed->items = item;
    else {
	item->prev = feed->items;
	while (item->prev->next)
	    item->prev = item->prev->next;
	item->prev->next = item;
    }
}

static void parse_atom_channel (struct feed* feed, xmlDocPtr doc, xmlNodePtr node)
{
    // Free everything before we write to it again.
    free_feed (feed);
    // Go through all the tags in the <channel> tag and extract the information
    for (xmlNodePtr cur = node; cur; cur = cur->next) {
	if (cur->type != XML_ELEMENT_NODE)
	    continue;
	if (xmlStrcmp (cur->name, (const xmlChar*) "title") == 0) {
	    feed->title = (char*) xmlNodeListGetString (doc, cur->children, 1);
	    CleanupString (feed->title, 1);
	    if (feed->title && strlen (feed->title) > 1 && feed->title[strlen (feed->title) - 1] == '\n')
		feed->title[strlen (feed->title) - 1] = '\0';	// Remove trailing newline
	} else if (xmlStrcmp (cur->name, (const xmlChar*) "link") == 0) {
	    feed->link = (char*) xmlGetProp (cur, (const xmlChar*) "href");
	    if (feed->link && strlen (feed->link) > 1 && feed->link[strlen (feed->link) - 1] == '\n')
		feed->link[strlen (feed->link) - 1] = '\0';	// Remove trailing newline
	} else if (xmlStrcmp (cur->name, (const xmlChar*) "entry") == 0)
	    parse_atom_entry (feed, doc, cur->children);
    }
}

//}}}-------------------------------------------------------------------

int DeXML (struct feed* cur_ptr)
{
    if (!cur_ptr->xmltext)
	return -1;

    saverestore = false;
    // If cur_ptr-> items! = NULL then we can cache item->readstatus
    if (cur_ptr->items != NULL) {
	saverestore = true;
	firstcopy = NULL;

	// Copy current newsitem struct. */
	for (struct newsitem * cur_item = cur_ptr->items; cur_item != NULL; cur_item = cur_item->next) {
	    copy = malloc (sizeof (struct newsitem));
	    copy->data = malloc (sizeof (struct newsdata));
	    copy->data->title = NULL;
	    copy->data->link = NULL;
	    copy->data->description = NULL;
	    copy->data->hash = NULL;
	    copy->data->readstatus = cur_item->data->readstatus;
	    if (cur_item->data->hash != NULL)
		copy->data->hash = strdup (cur_item->data->hash);

	    copy->next = NULL;
	    if (firstcopy == NULL) {
		copy->prev = NULL;
		firstcopy = copy;
	    } else {
		copy->prev = firstcopy;
		while (copy->prev->next != NULL)
		    copy->prev = copy->prev->next;
		copy->prev->next = copy;
	    }
	}
    }
    // xmlRecoverMemory:
    // parse an XML in-memory document and build a tree.
    // In case the document is not Well Formed, a tree is built anyway.
    xmlDocPtr doc = xmlRecoverMemory (cur_ptr->xmltext, strlen (cur_ptr->xmltext));

    if (doc == NULL)
	return 2;

    // Find the root element (in our case, it should read "<RDF: RDF>").
    // The RDF: prefix is ignored for now until the Jaguar
    // Find out how to read that exactly (jau).
    xmlNodePtr cur = xmlDocGetRootElement (doc);
    if (!cur) {
	xmlFreeDoc (doc);
	return 2;
    }
    // Check if the element really is called <RDF>
    if (xmlStrcmp (cur->name, (const xmlChar*) "RDF") == 0) {
	// Now we go through all the elements in the document. This loop however,
	// only the highest level elements work (HTML would only be HEAD and
	// BODY), so do not wander entire structure down through. The functions
	// are responsible for this, which we then call in the loop itself.
	for (xmlNodePtr c = cur->children; c; c = c->next) {
	    if (c->type != XML_ELEMENT_NODE)
		continue;
	    if (xmlStrcmp (c->name, (const xmlChar*) "channel") == 0)
		parse_rdf10_channel (cur_ptr, doc, c->children);
	    if (xmlStrcmp (c->name, (const xmlChar*) "item") == 0)
		parse_rdf10_item (cur_ptr, doc, c->children);
	    // Last-Modified is only used when reading from internal feeds (disk cache).
	    if (c->ns && (xmlStrcmp (c->ns->href, snowNs) == 0) && (xmlStrcmp (c->name, (const xmlChar*) "lastmodified") == 0))
		cur_ptr->lastmodified = (char*) xmlNodeListGetString (doc, c->children, 1);
	}
    } else if (xmlStrcmp (cur->name, (const xmlChar*) "rss") == 0) {
	for (xmlNodePtr c = cur->children; c; c = c->next) {
	    if (c->type != XML_ELEMENT_NODE)
		continue;
	    if (xmlStrcmp (c->name, (const xmlChar*) "channel") == 0)
		parse_rdf20_channel (cur_ptr, doc, c->children);
	}
    } else if (xmlStrcmp (cur->name, (const xmlChar*) "feed") == 0) {
	parse_atom_channel (cur_ptr, doc, cur->children);
    } else {
	xmlFreeDoc (doc);
	return 3;
    }

    xmlFreeDoc (doc);

    if (saverestore) {
	// free struct newsitem *copy.
	while (firstcopy->next != NULL) {
	    firstcopy = firstcopy->next;
	    free (firstcopy->prev->data->hash);
	    free (firstcopy->prev->data);
	    free (firstcopy->prev);
	}
	free (firstcopy->data->hash);
	free (firstcopy->data);
	free (firstcopy);
    }

    if (cur_ptr->original != NULL)
	free (cur_ptr->original);

    // Set -> title to something if it's a NULL pointer to avoid crash with strdup below.
    if (cur_ptr->title == NULL)
	cur_ptr->title = strdup (cur_ptr->feedurl);
    char* converted = iconvert (cur_ptr->title);
    if (converted) {
	free (cur_ptr->title);
	cur_ptr->title = converted;
    }
    cur_ptr->original = strdup (cur_ptr->title);

    // Restore custom title.
    if (cur_ptr->custom_title) {
	free (cur_ptr->title);
	cur_ptr->title = strdup (cur_ptr->custom_title);
    }
    return 0;
}
