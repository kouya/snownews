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

// Netscape cookie file format:
// (http://www.cookiecentral.com/faq/#3.5)
//
// .host.com    host_match[BOOL]        /path   secure[BOOL]    expire[unix time]       NAME    VALUE

#include "config.h"
#include "ui-support.h"

static void CookieCutter (struct feed* cur_ptr, FILE * cookies)
{
    int len = 0;
    int cookienr = 0;

    // Get current time.
    time_t tunix = time (NULL);

    char* url = strdup (cur_ptr->feedurl);
    char* freeme = url;

    strsep (&url, "/");
    strsep (&url, "/");
    char* tmphost = url;
    strsep (&url, "/");
    if (url == NULL) {
	free (freeme);
	return;
    }
    // If tmphost contains an '@' strip authinfo off url.
    if (strchr (tmphost, '@'))
	strsep (&tmphost, "@");

    char* host = strdup (tmphost);	// Current feed hostname.
    --url;
    url[0] = '/';
    if (url[strlen (url) - 1] == '\n')
	url[strlen (url) - 1] = '\0';

    char* path = strdup (url);	// Current feed path.
    free (freeme);
    freeme = NULL;

    while (!feof (cookies)) {
	char buf[BUFSIZ];	// File read buffer.
	if ((fgets (buf, sizeof (buf), cookies)) == NULL)
	    break;

	// Filter \n lines. But ignore them so we can read a NS cookie file.
	if (buf[0] == '\n')
	    continue;

	// Allow adding of comments that start with '#'.
	// Makes it possible to symlink Mozilla's cookies.txt.
	if (buf[0] == '#')
	    continue;

	char* cookie = strdup (buf);
	freeme = cookie;

	// Munch trailing newline.
	if (cookie[strlen (cookie) - 1] == '\n')
	    cookie[strlen (cookie) - 1] = '\0';

	// Decode the cookie string.
	char* cookiehost = NULL;
	char* cookiepath = NULL;
	char* cookiename = NULL;
	char* cookievalue = NULL;
	time_t cookieexpire = 0;
	bool cookiesecure = false;
	for (unsigned i = 0; i < 7; ++i) {
	    const char* tmpstr = strsep (&cookie, "\t");
	    if (!tmpstr)
		break;
	    switch (i) {
		case 0:
		    // Cookie hostname.
		    cookiehost = strdup (tmpstr);
		    break;
		case 1:
		    // Discard host match value.
		    break;
		case 2:
		    // Cookie path.
		    cookiepath = strdup (tmpstr);
		    break;
		case 3:
		    // Secure cookie?
		    if (strcasecmp (tmpstr, "TRUE") == 0)
			cookiesecure = true;
		    break;
		case 4:
		    // Cookie expiration date.
		    cookieexpire = strtoul (tmpstr, NULL, 10);
		    break;
		case 5:
		    // NAME
		    cookiename = strdup (tmpstr);
		    break;
		case 6:
		    // VALUE
		    cookievalue = strdup (tmpstr);
		    break;
	    }
	}

	// See if current cookie matches cur_ptr.
	// Hostname and path must match.
	// Ignore secure cookies.
	// Discard cookie if it has expired.
	if (strstr (host, cookiehost) && strstr (path, cookiepath) && !cookiesecure && cookieexpire > tunix) {
	    ++cookienr;

	    // Construct and append cookiestring.
	    //
	    // Cookie: NAME=VALUE; NAME=VALUE
	    if (cookienr == 1) {
		len = 8 + strlen (cookiename) + 1 + strlen (cookievalue) + 1;
		cur_ptr->cookies = malloc (len);
		strcpy (cur_ptr->cookies, "Cookie: ");
		strcat (cur_ptr->cookies, cookiename);
		strcat (cur_ptr->cookies, "=");
		strcat (cur_ptr->cookies, cookievalue);
	    } else {
		len += strlen (cookiename) + 1 + strlen (cookievalue) + 3;
		cur_ptr->cookies = realloc (cur_ptr->cookies, len);
		strcat (cur_ptr->cookies, "; ");
		strcat (cur_ptr->cookies, cookiename);
		strcat (cur_ptr->cookies, "=");
		strcat (cur_ptr->cookies, cookievalue);
	    }
	} else if ((strstr (host, cookiehost) != NULL) && (strstr (path, cookiepath) != NULL) && (cookieexpire < (int) tunix)) {	// Cast time_t tunix to int.
	    // Print cookie expire warning.
	    char expirebuf[PATH_MAX];
	    snprintf (expirebuf, sizeof (expirebuf), _("Cookie for %s has expired!"), cookiehost);
	    UIStatus (expirebuf, 1, 1);
	}

	free (freeme);
	freeme = NULL;
	free (cookiehost);
	free (cookiepath);
	free (cookiename);
	free (cookievalue);
    }

    free (host);
    free (path);
    free (freeme);

    // Append \r\n to cur_ptr->cookies
    if (cur_ptr->cookies != NULL) {
	cur_ptr->cookies = realloc (cur_ptr->cookies, len + 2);
	strcat (cur_ptr->cookies, "\r\n");
    }
}

void LoadCookies (struct feed* cur_ptr)
{
    char file[PATH_MAX];	// File locations.
    snprintf (file, sizeof (file), SNOWNEWS_CONFIG_DIR "cookies", getenv ("HOME"));
    FILE* cookies = fopen (file, "r");
    if (!cookies)	       // No cookies to load.
	return;
    CookieCutter (cur_ptr, cookies);
    fclose (cookies);
}
