/* Snownews - A lightweight console RSS newsreader
 * $Id: io-internal.h 306 2004-11-01 23:15:01Z kiza $
 * 
 * Copyright 2003-2004 Oliver Feiler <kiza@kcore.de>
 * http://kiza.kcore.de/software/snownews/
 *
 * io-internal.h
 *
 * Please read the file README.patching before changing any code in this file!
 *
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
 
#ifndef IO_INTERNAL_H
#define IO_INTERNAL_H

struct feed * newFeedStruct (void);
int UpdateFeed (struct feed * cur_ptr);
int UpdateAllFeeds (void);
int LoadFeed (struct feed * cur_ptr);
int LoadAllFeeds (int numfeeds);
void WriteCache (void);
void printlog (struct feed * feed, const char * text);
void printlogSimple (const char * text);

#endif
