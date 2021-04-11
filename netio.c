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
    fp->problem = true;
    if (fp->lasterror) {
	free (fp->lasterror);
	fp->lasterror = NULL;
    }

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

    // Setup CURL connection
    CURL* curl = curl_easy_init();
    if (!curl)
	return;
    curl_easy_setopt (curl, CURLOPT_URL, url);
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, FeedReceiver);
    curl_easy_setopt (curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt (curl, CURLOPT_USERAGENT, SNOWNEWS_NAME "/" SNOWNEWS_VERSTRING);
    curl_easy_setopt (curl, CURLOPT_BUFFERSIZE, CURL_MAX_READ_SIZE);
    curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt (curl, CURLOPT_MAXREDIRS, 8);
    curl_easy_setopt (curl, CURLOPT_AUTOREFERER, 1);

    // Avoid redownloading if not modified
    curl_easy_setopt (curl, CURLOPT_FILETIME, 1);
    if (fp->lastmodified > 0) {
	curl_easy_setopt (curl, CURLOPT_TIMEVALUE, fp->lastmodified);
	curl_easy_setopt (curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
    }

    // Cookies, if the cookie file is in the config dir
    char cookiefile [PATH_MAX];
    unsigned cfnl = snprintf (cookiefile, sizeof(cookiefile), SNOWNEWS_CONFIG_DIR "cookies", getenv("HOME"));
    if (cfnl < sizeof(cookiefile) && access (cookiefile, R_OK) == 0)
	curl_easy_setopt (curl, CURLOPT_COOKIEFILE, cookiefile);

    // Save old content
    char* oldxmltext = fp->xmltext;
    unsigned oldcontent_length = fp->content_length;
    fp->xmltext = NULL;
    fp->content_length = 0;

    // Download the new feed
    CURLcode rc = curl_easy_perform (curl);
    if (rc == CURLE_OK && fp->content_length)  {
	//
	// Transfer successful
	//
	fp->problem = false;
	if (oldxmltext) {
	    free (oldxmltext);
	    oldxmltext = NULL;
	}
	long filetime = 0;
	if (CURLE_OK == curl_easy_getinfo (curl, CURLINFO_FILETIME, &filetime) && filetime > 0)
	    fp->lastmodified = filetime;
    } else {
	//
	// On error, free downloaded text and restore original
	//
	if (fp->xmltext)
	    free (fp->xmltext);
	fp->xmltext = oldxmltext;
	fp->content_length = oldcontent_length;

	// Check if failed because already up-to-date
	long unmet = 0;
	if (CURLE_OK == curl_easy_getinfo (curl, CURLINFO_CONDITION_UNMET, &unmet) && unmet) {
	    fp->lasterror = strdup (_("Feed already up to date"));
	    UIStatus (fp->lasterror, 0, 0);
	    fp->problem = false;
	} else {
	    // The error is stored in fp->lasterror for display
	    const char* cerrt = curl_easy_strerror (rc);
	    if (cerrt) {
		fp->lasterror = strdup (cerrt);
		UIStatus (cerrt, 2, 1);
		syslog (LOG_ERR, "%s", cerrt);
	    }
	}
    }
    curl_easy_cleanup (curl);
}
