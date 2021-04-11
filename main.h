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

struct feed {
    struct newsitem* items;
    struct feed* next;
    struct feed* prev;
    char* feedurl;		// Non hashified URL
    char* xmltext;		// Raw XML
    char* title;
    char* link;
    char* description;
    char* lasterror;
    char* custom_title;		// Custom feed title.
    char* original;		// Original feed title.
    char* perfeedfilter;	// Pipe feed through this program before parsing.
    time_t mtime;		// Last local modification time
    time_t lastmodified;	// Last modification time on the server
    unsigned content_length;
    bool problem;		// Set if there was a problem downloading the feed.
    bool execurl;		// Execurl?
    bool smartfeed;		// 1: new items feed.
    struct feedcategories* feedcategories;
};

struct newsitem {
    struct newsdata* data;
    struct newsitem* next;
    struct newsitem* prev;	// Pointer to next/prev item in double linked list
};

struct newsdata {
    struct feed* parent;
    char* title;
    char* link;
    char* description;
    char* hash;
    int date;
    bool readstatus;
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
    int8_t newitems;
    int8_t urljump;
    int8_t feedtitle;
    bool newitemsbold;
    bool urljumpbold;
    bool feedtitlebold;
};

struct entity {
    char* entity;
    char* converted_entity;
    int entity_length;
    struct entity* next;
};

// A feeds categories
struct feedcategories {
    char* name;			// Category name
    struct feedcategories* next;
};

// Global list of all defined categories, their refcounts and color labels.
struct categories {
    struct categories* next;
    char* name;			// Category name
    unsigned short refcount;	// Number of feeds using this category.
    unsigned short label;	// Color label of this category.
    bool labelbold;
};

struct settings {
    struct entity* html_entities;
    struct categories* global_categories;
    const char* global_charset;
    char* browser;		// Browser command. lynx is standard.
    char* proxyname;		// Hostname of proxyserver.
    unsigned short proxyport;	// Port on proxyserver to use.
    struct color color;
    struct keybindings keybindings;
    bool monochrome;
    bool cursor_always_visible;
};

//----------------------------------------------------------------------
// Global variables

extern struct feed* _feed_list;
extern struct feed* _unfiltered_feed_list;
extern struct settings _settings;
extern bool _feed_list_changed;

//----------------------------------------------------------------------

_Noreturn void MainQuit (const char* func, const char* error);
