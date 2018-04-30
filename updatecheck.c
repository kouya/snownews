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

#include "updatecheck.h"
#include "netio.h"
#include "io-internal.h"

void AutoVersionCheck (void) {
	// We check once a week.
	char timestring[21];	// Should be enough for 64bit systems.
	snprintf (timestring, sizeof(timestring), "%ld", time(NULL));
	char file [PATH_MAX];
	snprintf (file, sizeof(file), "%s/.snownews/updatecheck", getenv("HOME"));

	FILE* lastupdated = fopen (file, "r+");
	if (!lastupdated) {
		lastupdated = fopen (file, "w+");
		fputs (timestring, lastupdated);
	} else {
		char oldtimestring[21];
		fgets (oldtimestring, sizeof(oldtimestring), lastupdated);
		time_t oldtime = atol(oldtimestring);

		// If -1 is given in updatecheck or last check is <1 week, skip the check.
		if (time(NULL)-oldtime < 7*24*3600 || oldtime <= 0) {
			fclose (lastupdated);
			return;	// Less than one week.
		} else {
			rewind (lastupdated);
			fputs (timestring, lastupdated);
		}
	}
	fclose (lastupdated);

	struct feed* update = newFeedStruct();
	char* url = strdup ("http://kiza.kcore.de/software/snownews/version");
	update->feedurl = strdup(url);
	char* versionstring = DownloadFeed (url, update, 1);
	free (url);

	if (versionstring) {
		if (versionstring[strlen(versionstring)-1] == '\n')
			versionstring[strlen(versionstring)-1] = '\0';
		if (strcmp(versionstring, SNOWNEWS_VERSION) != 0) {
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
