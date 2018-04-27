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
