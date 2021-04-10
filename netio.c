// This file is part of Snownews - A lightweight console RSS newsreader
//
// Copyright (c) 2003-2004 Oliver Feiler <kiza@kcore.de>
// Copyright (c) 2021 Mike Sharov <msharov@users.sourceforge.net>
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

#include "netio.h"
#include "ui-support.h"
#include <curl/curl.h>

static size_t FeedReceiver (void* buffer, size_t msz, size_t nm, void* vpfeed)
{
    struct feed* fp = vpfeed;
    size_t size = msz * nm;
    char* t = realloc (fp->xmltext, fp->content_length + size + 1);
    if (!t) {
	fprintf (stderr, "Error: out of memory\n");
	exit (EXIT_FAILURE);
    }
    fp->xmltext = t;
    memcpy (&fp->xmltext[fp->content_length], buffer, size);
    fp->content_length += size;
    fp->xmltext [fp->content_length] = 0;
    return size;
}

// Returns allocated string with body of webserver reply.
// Various status info put into struct feed* fp.
void DownloadFeed (const char* url, struct feed* fp)
{
    // Default to error
    if (fp->xmltext) {
	free (fp->xmltext);
	fp->xmltext = NULL;
	fp->content_length = 0;
    }
    fp->problem = true;

    // libcurl global init must be called only once
    // snownews is single threaded, so no fancy locks needed
    static bool s_curl_initialized = false;
    if (!s_curl_initialized) {
	if (0 != curl_global_init (CURL_GLOBAL_DEFAULT)) {
	    UIStatus ("Error: failed to initialize libcurl", 2, 1);
	    syslog (LOG_ERR, "failed to initialize libcurl");
	    return;
	}
	atexit (curl_global_cleanup);
	s_curl_initialized = true;
    }

    CURL* curl = curl_easy_init();
    if (!curl)
	return;

    curl_easy_setopt (curl, CURLOPT_URL, url);
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, FeedReceiver);
    curl_easy_setopt (curl, CURLOPT_WRITEDATA, fp);

    char cookiefile [PATH_MAX];
    unsigned cfnl = snprintf (cookiefile, sizeof(cookiefile), SNOWNEWS_CONFIG_DIR "cookies", getenv("HOME"));
    if (cfnl < sizeof(cookiefile) && access (cookiefile, R_OK) == 0)
	curl_easy_setopt (curl, CURLOPT_COOKIEFILE, cookiefile);

    CURLcode rc = curl_easy_perform (curl);

    curl_easy_cleanup (curl);

    if (rc == CURLE_OK)
	fp->problem = false;
    else if (fp->xmltext) {
	free (fp->xmltext);
	fp->xmltext = NULL;
	fp->content_length = 0;
	const char* cerrt = curl_easy_strerror (rc);
	if (cerrt) {
	    UIStatus (cerrt, 2, 1);
	    syslog (LOG_ERR, "%s", cerrt);
	}
    }
}
