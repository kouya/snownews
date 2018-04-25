/* Snownews - A lightweight console RSS newsreader
 * $Id: os-support.c 1146 2005-09-10 10:05:21Z kiza $
 * 
 * Copyright 2003-2004 Oliver Feiler <kiza@kcore.de>
 * http://kiza.kcore.de/software/snownews/
 *
 * os-support.c
 *
 * Library support functions.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include "main.h"

/******************************************************************************
 * This is a replacement for strsep which is not portable (missing on Solaris).
 *
 * http://www.winehq.com/hypermail/wine-patches/2001/11/0024.html
 *
 * The following function was written by François Gouget.
 */
#ifdef SUN
char* strsep (char** str, const char* delims)
{
    char* token;

    if (*str==NULL) {
        /* No more tokens */
        return NULL;
    }

    token=*str;
    while (**str!='\0') {
        if (strchr(delims,**str)!=NULL) {
            **str='\0';
            (*str)++;
            return token;
        }
        (*str)++;
    }
    /* There is no other token */
    *str=NULL;
   return token;
}

/* timegm() is not available on Solaris */
time_t timegm(struct tm *t)
{
	time_t tl, tb;
	struct tm *tg;

	tl = mktime (t);
	if (tl == -1)
	{
		t->tm_hour--;
		tl = mktime (t);
		if (tl == -1)
			return -1; /* can't deal with output from strptime */
		tl += 3600;
	}
	tg = gmtime (&tl);
	tg->tm_isdst = 0;
	tb = mktime (tg);
	if (tb == -1)
	{
		tg->tm_hour--;
		tb = mktime (tg);
		if (tb == -1)
			return -1; /* can't deal with output from gmtime */
		tb += 3600;
	}
	return (tl - (tb - tl));
}
#endif

/* strcasestr stolen from: http://www.unixpapa.com/incnote/string.html */
const char* s_strcasestr (const char* a, const char* b) {
	const size_t lena = strlen(a), lenb = strlen(b);
	char f[3];
	snprintf(f, sizeof(f), "%c%c", tolower(*b), toupper(*b));
	for (size_t l = strcspn(a, f); l != lena; l += strcspn(a + l + 1, f) + 1)
		if (strncasecmp(a + l, b, lenb) == 0)
			return a + l;
	return NULL;
}


/* Private malloc wrapper. Aborts program execution if malloc fails. */
void* s_malloc (size_t size) {
	void* newmem = malloc (size);
	if (!newmem)
		MainQuit ("Allocating memory", strerror(errno));
	return newmem;
}
