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

#include "config.h"
#include <ctype.h>
#include <time.h>

//-----------------------------------------------------------------------------
// This is a replacement for strsep which is not portable (missing on Solaris).
//
// http://www.winehq.com/hypermail/wine-patches/2001/11/0024.html
//
// The following function was written by Francois Gouget.
#ifdef SUN
char* strsep (char** str, const char* delims)
{
	if (!*str) // No more tokens
		return NULL;
	char* token = *str;
	while (**str) {
		if (strchr (delims, **str)) {
			*(*str)++ = '\0';
			return token;
		}
		(*str)++;
	}
	// There is no other token
	*str = NULL;
	return token;
}

// timegm() is not available on Solaris
time_t timegm (struct tm *t)
{
	time_t tl = mktime (t);
	if (tl == -1) {
		--t->tm_hour;
		tl = mktime (t);
		if (tl == -1)
			return -1; // can't deal with output from strptime
		tl += 3600;
	}
	struct tm* tg = gmtime (&tl);
	tg->tm_isdst = 0;
	time_t tb = mktime (tg);
	if (tb == -1) {
		tg->tm_hour--;
		tb = mktime (tg);
		if (tb == -1)
			return -1; // can't deal with output from gmtime
		tb += 3600;
	}
	return tl - (tb - tl);
}
#endif

// strcasestr stolen from: http://www.unixpapa.com/incnote/string.html
const char* s_strcasestr (const char* a, const char* b) {
	const size_t lena = strlen(a), lenb = strlen(b);
	char f[3];
	snprintf(f, sizeof(f), "%c%c", tolower(*b), toupper(*b));
	for (size_t l = strcspn(a, f); l != lena; l += strcspn(a + l + 1, f) + 1)
		if (strncasecmp(a + l, b, lenb) == 0)
			return a + l;
	return NULL;
}
