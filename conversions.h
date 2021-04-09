// This file is part of Snownews - A lightweight console RSS newsreader
//
// Copyright (c) 2003-2004 Oliver Feiler <kiza@kcore.de>
// Copyright (c) 2003-2004 Rene Puls <rpuls@gmx.net>
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

char* iconvert (const char* inbuf);
char* UIDejunk (const char* feed_description);
char* WrapText (const char* text, unsigned width);
char* base64encode (const char* inbuf, unsigned int inbuf_size);
void CleanupString (char* string, int tidyness);
char* Hashify (const char* url);
char* genItemHash (const char* const* hashitems, unsigned items);
time_t ISODateToUnix (const char* ISODate);
time_t pubDateToUnix (const char* pubDate);
char* unixToPostDateString (time_t unixDate);

#ifdef USE_UNSUPPORTED_AND_BROKEN_CODE
char* decodechunked (char* chunked, unsigned int* inputlen);
#endif
