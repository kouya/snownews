/* Snownews - A lightweight console RSS newsreader
 * $Id: os-support.h 92 2004-10-06 10:29:54Z kiza $
 * 
 * Copyright 2003-2004 Oliver Feiler <kiza@kcore.de>
 * http://kiza.kcore.de/software/snownews/
 *
 * os-support.h
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

#ifndef OS_SUPPORT_H
#define OS_SUPPORT_H

#ifdef SUN
char* strsep(char** str, const char* delims);
#endif

char *s_strcasestr(char *a, char *b);
void * s_malloc (size_t size);

#endif
