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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "config.h"
#include "version.h"
#include "netio.h"
#include "io-internal.h"


void AutoVersionCheck (void) {
	struct feed *update;
	FILE *lastupdated;
	char file[512];
	char timestring[21];	// Should be enough for 64bit systems.
	char oldtimestring[21];
	int oldtime;
	char *versionstring = NULL;
	char *url;
	
	update = newFeedStruct();
	
	// We check once a week.
	snprintf (timestring, sizeof(timestring), "%d", (int) time(NULL));
	snprintf (file, sizeof(file), "%s/.snownews/updatecheck", getenv("HOME"));
	lastupdated = fopen (file, "r+");
	if (lastupdated == NULL) {
		lastupdated = fopen (file, "w+");
		fputs (timestring, lastupdated);
		fclose (lastupdated);
	} else {
		fgets (oldtimestring, sizeof(oldtimestring), lastupdated);
		oldtime = atoi(oldtimestring);
		
		// If -1 is given in updatecheck or last check is <1 week, skip the check.
		if (((((int) time(NULL))-oldtime) < 604800) ||
			(oldtime == -1)) {
			// Less than one week.
			fclose (lastupdated);
			free (update);
			return;
		} else {
			rewind (lastupdated);
			fputs (timestring, lastupdated);
			fclose (lastupdated);
		}
	}
	
	url = strdup ("http://kiza.kcore.de/software/snownews/version");
	update->feedurl = strdup(url);
	versionstring = DownloadFeed (url, update, 1);
	free (url);
	
	if (versionstring != NULL) {
		if (versionstring[strlen(versionstring)-1] == '\n')
			versionstring[strlen(versionstring)-1] = '\0';
	
		if (strcmp(versionstring, VERSION) != 0) {
			printf (_("A new version %s of Snownews is available.\n"), versionstring);
			printf (_("If you want to download it, go to http://snownews.kcore.de/downloading/\n\n"));
			printf (_("To disable the auto version check see the manpage.\n\n"));
			printf ("http://snownews.kcore.de/download/snownews-%s.tar.gz (.sign)\n", versionstring);
			printf ("http://snownews.kcore.de/download/snownews-%s.i386.tar.bz2 (.sign)\n", versionstring);
		}
	}
	free (versionstring);
	free (update->lastmodified);
	free (update->feedurl);
	free (update->content_type);
	
	free (update);
	
	return;
}
