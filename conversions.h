/* Snownews - A lightweight console RSS newsreader
 * $Id: conversions.h 910 2005-03-19 17:48:09Z kiza $
 * 
 * Copyright 2003-2004 Oliver Feiler <kiza@kcore.de> and
 *                     Rene Puls <rpuls@gmx.net>
 *
 * conversions.h
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

#ifndef CONVERSIONS_H
#define CONVERSIONS_H

#ifdef STATIC_CONST_ICONV
char * iconvert (const char * inbuf);
#else
char * iconvert (char * inbuf);
#endif
char * UIDejunk (char * feed_description);
char* WrapText (const char* text, unsigned width);
char *base64encode(char const *inbuf, unsigned int inbuf_size);
void CleanupString (char * string, int tidyness);
char * Hashify (const char * url);
char* genItemHash (const char* const* hashitems, int items);
int ISODateToUnix (char const * const ISODate);
int pubDateToUnix (char const * const pubDate);
char * unixToPostDateString (int unixDate);

#ifdef USE_UNSUPPORTED_AND_BROKEN_CODE
char* decodechunked(char * chunked, unsigned int *inputlen);
#endif

#endif
