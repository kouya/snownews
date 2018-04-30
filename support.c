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
#include <sys/stat.h>
#include <time.h>

bool easterEgg (void) {
	time_t tunix = time(NULL);
	struct tm* t = localtime(&tunix);
	
	char file[256];
	snprintf (file, sizeof(file), "%s/.snownews/santa_hunta", getenv("HOME"));
	struct stat filetest;
	return 0 == stat (file, &filetest) || (t->tm_mon == 11 && t->tm_mday >= 24 && t->tm_mday <= 26);
}
