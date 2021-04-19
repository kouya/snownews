// This file is part of Snownews - A lightweight console RSS newsreader
//
// Copyright (c) 2003-2004 Oliver Feiler <kiza@kcore.de>
// Copyright (c) 2003-2004 Rene Puls <rpuls@gmx.net>
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

#pragma once
#include "config.h"

char* iconvert (const char* inbuf);
char* UIDejunk (const char* feed_description);
char* WrapText (const char* text, unsigned width);
void CleanupString (char* string, bool fullclean);
char* Hashify (const char* url);
char* genItemHash (const char* const* hashitems, unsigned items);
time_t ISODateToUnix (const char* ISODate);
time_t pubDateToUnix (const char* pubDate);
char* unixToPostDateString (time_t unixDate);
const char* s_strcasestr (const char* a, const char* b);
#ifdef SUN
char* strsep (char** str, const char* delims);
#endif
