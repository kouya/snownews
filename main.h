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

#pragma once
#include "config.h"

//----------------------------------------------------------------------

enum netio_error {
       NET_ERR_OK,
       // Init errors
       NET_ERR_URL_INVALID,
       // Connect errors
       NET_ERR_SOCK_ERR,
       NET_ERR_HOST_NOT_FOUND,
       NET_ERR_CONN_REFUSED,
       NET_ERR_CONN_FAILED,
       NET_ERR_TIMEOUT,
       NET_ERR_UNKNOWN,
       // Transfer errors
       NET_ERR_REDIRECT_COUNT_ERR,
       NET_ERR_REDIRECT_ERR,
       NET_ERR_HTTP_410,
       NET_ERR_HTTP_404,
       NET_ERR_HTTP_NON_200,
       NET_ERR_HTTP_PROTO_ERR,
       NET_ERR_AUTH_FAILED,
       NET_ERR_AUTH_NO_AUTHINFO,
       NET_ERR_AUTH_GEN_AUTH_ERR,
       NET_ERR_AUTH_UNSUPPORTED,
       NET_ERR_GZIP_ERR,
       NET_ERR_CHUNKED
};

struct feed {
	char *feedurl;			// Non hashified URL
	char *feed;			// Raw XML
	unsigned content_length;
	char *title;
	char *link;
	char *description;
	char *lastmodified;		// Content of header as sent by the server.
	int lasthttpstatus;
	char *content_type;
	enum netio_error netio_error;	// See netio.h
	int connectresult;		// Socket errno
	char *cookies;			// Login cookies for this feed.
	char *authinfo;			// HTTP authinfo string.
	char *servauth;			// Server supplied authorization header.
	struct newsitem * items;
	struct feed *next_ptr, *prev_ptr;
	int problem;			// Set if there was a problem downloading the feed.
	char *override;			// Custom feed title.
	char *original;			// Original feed title.
	char *perfeedfilter;		// Pipe feed through this program before parsing.
	int execurl;			// Execurl?
	int smartfeed;			// 1: new items feed.
	struct feedcategories * feedcategories;
};

struct newsitem {
	struct newsdata *data;
	struct newsitem *next_ptr, *prev_ptr;	// Pointer to next/prev item in double linked list
};

struct newsdata {
	struct feed *parent;
	int readstatus;			// 0: unread, 1: read
	char *title;
	char *link;
	char *description;
	char *hash;
	int date;
};

// Global program keybindings. Loaded from setup:Config()
struct keybindings {
	char next;
	char prev;
	char prevmenu;
	char quit;
	char addfeed;
	char deletefeed;
	char markread;
	char markunread;
	char markallread;
	char dfltbrowser;
	char moveup;
	char movedown;
	char feedinfo;
	char reload;
	char forcereload;
	char reloadall;
	char urljump;
	char urljump2;
	char changefeedname;
	char sortfeeds;
	char pdown;
	char pup;
	char categorize;
	char filter;
	char filtercurrent;
	char nofilter;
	char help;
	char about;
	char perfeedfilter;
	char andxor;
	char home;
	char end;
	char enter;
	char newheadlines;
	char typeahead;
};

// Color definitions
struct color {
	int newitems;
	int newitemsbold;
	int urljump;
	int urljumpbold;
	int feedtitle;
	int feedtitlebold;
};

struct entity {
	char*	entity;
	char*	converted_entity;
	int	entity_length;
	struct entity*	next_ptr;
};

// A feeds categories
struct feedcategories {
	char* name;		// Category name
	struct feedcategories * next_ptr;
};

// Global list of all defined categories, their refcounts and color labels.
struct categories {
	char*	name;		// Category name
	int	refcount;	// Number of feeds using this category.
	int	label;		// Color label of this category.
	int	labelbold;
	struct categories * next_ptr;
};

struct settings {
	struct entity*		html_entities;
	struct categories*	global_categories;
	const char*		global_charset;
	char*			browser;	// Browser command. lynx is standard.
	char*			proxyname;	// Hostname of proxyserver.
	char*			useragent;	// Snownews User-Agent string.
	struct color		color;
	unsigned short		proxyport;	// Port on proxyserver to use.
	struct keybindings	keybindings;
	bool			monochrome;
	bool			cursor_always_visible;
};

//----------------------------------------------------------------------
// Global variables

extern struct feed* _feed_list;
extern struct feed* _unfiltered_feed_list;
extern struct settings _settings;

//----------------------------------------------------------------------

_Noreturn void MainQuit (const char* func, const char* error);
