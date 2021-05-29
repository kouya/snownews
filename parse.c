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

#include "parse.h"
#include "feedio.h"
#include "conv.h"
#include <libxml/parser.h>

//{{{ Local variables --------------------------------------------------

static bool saverestore = false;
static struct newsitem* copy = NULL;
static struct newsitem* firstcopy = NULL;

static const char dcNs[] = "http://purl.org/dc/elements/1.1/";
static const char snowNs[] = "http://snownews.kcore.de/ns/1.0/";
static const char contentNs[] = "http://purl.org/rss/1.0/modules/content/";

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
//{{{ libXML accessors
//
// libXML made the strange choice of making xmlChar an unsigned char.
// Since all the text passed to it is char, a sea of casts results.

static bool node_name_is (xmlNodePtr pn, const char* name)
{
    return 0 == xmlStrcmp (pn->name, (const xmlChar*) name);
}

static bool node_ns_name_is (xmlNodePtr pn, const char* ns, const char* name)
{
    return pn->ns
	&& 0 == xmlStrcmp (pn->ns->href, (const xmlChar*) ns)
	&& node_name_is (pn, name);
}

static void copy_node_text_to (xmlDocPtr doc, xmlNodePtr pn, char** pd, bool fullclean)
{
    char* nt = (char*) xmlNodeListGetString (doc, pn->children, 1);
    if (!nt)
	return; // don't overwrite with empty
    if (!nt[0])
	return free (nt);
    if (*pd)
	free (*pd);
    *pd = nt;
    CleanupString (*pd, fullclean);
}

static long number_from_node_text (xmlDocPtr doc, xmlNodePtr pn)
{
    char* s = NULL;
    copy_node_text_to (doc, pn, &s, false);
    long v = atol(s);
    free (s);
    return v;
}

static time_t pubDate_from_node_text (xmlDocPtr doc, xmlNodePtr pn)
{
    char* s = NULL;
    copy_node_text_to (doc, pn, &s, false);
    time_t t = pubDateToUnix (s);
    free (s);
    return t;
}

static time_t ISODate_from_node_text (xmlDocPtr doc, xmlNodePtr pn)
{
    char* s = NULL;
    copy_node_text_to (doc, pn, &s, false);
    time_t t = ISODateToUnix (s);
    free (s);
    return t;
}

static void copy_node_prop_to (xmlNodePtr pn, const char* name, char** pd, bool fullclean)
{
    if (*pd)
	xmlFree (*pd);
    *pd = (char*) xmlGetProp (pn, (const xmlChar*) name);
    CleanupString (*pd, fullclean);
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
	if (node_name_is (cur, "title"))
	    copy_node_text_to (doc, cur, &item->data->title, true);
	else if (node_name_is (cur, "link"))
	    copy_node_text_to (doc, cur, &item->data->link, false);
	else if (node_name_is (cur, "description"))
	    copy_node_text_to (doc, cur, &item->data->description, false);

	// Userland extensions (No namespace!)
	else if (node_name_is (cur, "guid"))
	    copy_node_text_to (doc, cur, &guid, true);
	else if (node_name_is (cur, "pubDate"))
	    item->data->date = pubDate_from_node_text (doc, cur);
	else if (node_name_is (cur, "readstatus"))
	    item->data->readstatus = number_from_node_text (doc, cur);

	// content:encoded
	else if (node_ns_name_is (cur, contentNs, "encoded"))
	    copy_node_text_to (doc, cur, &item->data->description, false);

	// Dublin Core dc:date
	else if (node_ns_name_is (cur, dcNs, "date"))
	    item->data->date = ISODate_from_node_text (doc, cur);

	// Using snow namespace
	else if (node_ns_name_is (cur, snowNs, "hash"))
	    copy_node_text_to (doc, cur, &item->data->hash, true);
	else if (node_ns_name_is (cur, snowNs, "date"))
	    item->data->date = number_from_node_text (doc, cur);
    }

    // If we have loaded the hash from disk cache, don't regenerate it.
    // <guid> is not saved in the cache, thus we would generate a different
    // hash than the one from the live feed.
    if (!item->data->hash) {
	const char* hashitems[] = { item->data->title, item->data->link, guid, NULL };
	item->data->hash = genItemHash (hashitems, 3);
    }
    if (!item->data->title)
	item->data->title = strdup ("Untitled");
    if (guid) {
	xmlFree (guid);
	guid = NULL;
    }

    // If saverestore == true, restore readstatus.
    if (saverestore) {
	for (struct newsitem* i = firstcopy; i; i = i->next) {
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
	if (node_name_is (cur, "title"))
	    copy_node_text_to (doc, cur, &feed->title, true);
	else if (node_name_is (cur, "link"))
	    copy_node_text_to (doc, cur, &feed->link, false);
	else if (node_name_is (cur, "description"))
	    copy_node_text_to (doc, cur, &feed->description, false);
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
	if (node_name_is (cur, "title"))
	    copy_node_text_to (doc, cur, &feed->title, true);
	else if (node_name_is (cur, "link"))
	    copy_node_text_to (doc, cur, &feed->link, false);
	else if (node_name_is (cur, "description"))
	    copy_node_text_to (doc, cur, &feed->description, false);
	else if (node_name_is (cur, "item"))
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

	if (node_name_is (cur, "title"))
	    copy_node_text_to (doc, cur, &item->data->title, true);
	else if (node_name_is (cur, "link")) {
	    char* rel = NULL;
	    copy_node_prop_to (cur, "rel", &rel, false );
	    if (!rel || 0 == strcmp (rel, "alternate"))
		copy_node_prop_to (cur, "href", &item->data->link, false);
	    free (rel);
	} else if (node_name_is (cur, "summary") && !item->data->description)
	    copy_node_text_to (doc, cur, &item->data->description, false);
	else if (node_name_is (cur, "content"))
	    copy_node_text_to (doc, cur, &item->data->description, false);
	else if (node_name_is (cur, "id"))
	    copy_node_text_to (doc, cur, &guid, false);
	else if (node_name_is (cur, "updated"))
	    item->data->date = ISODate_from_node_text (doc, cur);
    }

    // If we have loaded the hash from disk cache, don't regenerate it.
    // <guid> is not saved in the cache, thus we would generate a different
    // hash than the one from the live feed.
    if (!item->data->hash) {
	const char* hashitems[] = { item->data->title, item->data->link, guid, NULL };
	item->data->hash = genItemHash (hashitems, 3);
    }
    if (!item->data->title)
	item->data->title = strdup ("Untitled");
    if (guid) {
	free (guid);
	guid = NULL;
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
	if (node_name_is (cur, "title"))
	    copy_node_text_to (doc, cur, &feed->title, true);
	else if (node_name_is (cur, "link"))
	    copy_node_prop_to (cur, "href", &feed->link, false);
	else if (node_name_is (cur, "entry"))
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
	    copy = calloc (1, sizeof (struct newsitem));
	    copy->data = calloc (1, sizeof (struct newsdata));
	    copy->data->readstatus = cur_item->data->readstatus;
	    if (cur_item->data->hash)
		copy->data->hash = strdup (cur_item->data->hash);

	    copy->next = NULL;
	    if (!firstcopy) {
		copy->prev = NULL;
		firstcopy = copy;
	    } else {
		copy->prev = firstcopy;
		while (copy->prev->next)
		    copy->prev = copy->prev->next;
		copy->prev->next = copy;
	    }
	}
    }
    // xmlRecoverMemory:
    // parse an XML in-memory document and build a tree.
    // In case the document is not Well Formed, a tree is built anyway.
    xmlDocPtr doc = xmlRecoverMemory (cur_ptr->xmltext, strlen (cur_ptr->xmltext));
    if (!doc)
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
    if (node_name_is (cur, "RDF")) {
	// Now we go through all the elements in the document. This loop however,
	// only the highest level elements work (HTML would only be HEAD and
	// BODY), so do not wander entire structure down through. The functions
	// are responsible for this, which we then call in the loop itself.
	for (xmlNodePtr c = cur->children; c; c = c->next) {
	    if (c->type != XML_ELEMENT_NODE)
		continue;
	    if (node_name_is (c, "channel"))
		parse_rdf10_channel (cur_ptr, doc, c->children);
	    if (node_name_is (c, "item"))
		parse_rdf10_item (cur_ptr, doc, c->children);
	    // Last-Modified is only used when reading from internal feeds (disk cache).
	    if (node_ns_name_is (c, snowNs, "lastmodified"))
		cur_ptr->lastmodified = number_from_node_text (doc, c);
	}
    } else if (node_name_is (cur, "rss")) {
	for (xmlNodePtr c = cur->children; c; c = c->next) {
	    if (c->type != XML_ELEMENT_NODE)
		continue;
	    if (node_name_is (c, "channel"))
		parse_rdf20_channel (cur_ptr, doc, c->children);
	}
    } else if (node_name_is (cur, "feed")) {
	parse_atom_channel (cur_ptr, doc, cur->children);
    } else {
	xmlFreeDoc (doc);
	return 3;
    }

    xmlFreeDoc (doc);

    if (saverestore) {
	// free struct newsitem *copy.
	while (firstcopy->next) {
	    firstcopy = firstcopy->next;
	    free (firstcopy->prev->data->hash);
	    free (firstcopy->prev->data);
	    free (firstcopy->prev);
	}
	free (firstcopy->data->hash);
	free (firstcopy->data);
	free (firstcopy);
	firstcopy = NULL;
    }

    if (cur_ptr->custom_title) {
	free (cur_ptr->title);
	cur_ptr->title = strdup (cur_ptr->custom_title);
    } else {
	if (!cur_ptr->title)
	    cur_ptr->title = strdup ("Untitled");
	char* converted = iconvert (cur_ptr->title);
	if (converted) {
	    free (cur_ptr->title);
	    cur_ptr->title = converted;
	}
    }
    if (cur_ptr->original)
	free (cur_ptr->original);
    cur_ptr->original = strdup (cur_ptr->title);
    return 0;
}

unsigned ParseOPMLFile (const char* flbuf)
{
    unsigned nfeeds = 0;
    xmlDocPtr doc = xmlRecoverMemory (flbuf, strlen (flbuf));
    if (!doc)
	return nfeeds;

    xmlNodePtr rootnode = xmlDocGetRootElement (doc);
    if (!rootnode) {
	xmlFreeDoc (doc);
	return nfeeds;
    }
    if (xmlStrcmp (rootnode->name, (const xmlChar*) "opml") == 0) {
	for (xmlNodePtr body = rootnode->children; body; body = body->next) {
	    if (body->type != XML_ELEMENT_NODE || !node_name_is (body, "body"))
		continue;
	    for (xmlNodePtr outline = body->children; outline; outline = outline->next) {
		if (outline->type != XML_ELEMENT_NODE || !node_name_is (outline, "outline"))
		    continue;

		char *text = NULL, *xmlUrl = NULL, *categories = NULL, *filter = NULL;
		copy_node_prop_to (outline, "text", &text, false);
		copy_node_prop_to (outline, "xmlUrl", &xmlUrl, false);
		copy_node_prop_to (outline, "category", &categories, false);
		copy_node_prop_to (outline, "filter", &filter, false);

		if (xmlUrl && text) {
		    AddFeed (xmlUrl, text, categories, filter);
		    ++nfeeds;
		}

		if (text)	free (text);
		if (xmlUrl)	free (xmlUrl);
		if (categories)	free (categories);
		if (filter)	free (filter);
	    }
	}
    }
    xmlFreeDoc (doc);
    return nfeeds;
}
