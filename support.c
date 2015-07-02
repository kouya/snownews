/* Snownews - A lightweight console RSS newsreader
 * $Id: support.c 1146 2005-09-10 10:05:21Z kiza $
 * 
 * Copyright 2003-2004 Oliver Feiler <kiza@kcore.de>
 * http://kiza.kcore.de/software/snownews/
 *
 * support.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <time.h>

int easterEgg (void) {
	struct tm *t;
	time_t tunix;
	char file[256];
	struct stat filetest;
	
	/* Get current time. */
	tunix = time(0);
	t = localtime(&tunix);
	
	snprintf (file, sizeof(file), "%s/.snownews/santa_hunta", getenv("HOME"));
	if ((stat (file, &filetest) == 0) ||
		((t->tm_mon == 11) && (t->tm_mday >= 24) && (t->tm_mday <= 26))) {
		return 1;
	} else {
		return 0;
	}
	
	return 0;
}
