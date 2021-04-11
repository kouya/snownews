// This file is part of Snownews - A lightweight console RSS newsreader
//
// Copyright (c) 2003-2009 Oliver Feiler <kiza@kcore.de>
// Copyright (c) 2003-2009 Rene Puls <rpuls@gmx.net>
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

#include "main.h"
#include "conv.h"
#include "uiutil.h"
#include <ctype.h>
#include <iconv.h>
#include <libxml/HTMLparser.h>
#include <langinfo.h>
#include <openssl/evp.h>
#include <openssl/md5.h>

//----------------------------------------------------------------------

static int calcAgeInDays (const struct tm* t);

//----------------------------------------------------------------------

// This is a replacement for strsep which is not portable (missing on Solaris).
//
// http://www.winehq.com/hypermail/wine-patches/2001/11/0024.html
//
// The following function was written by Francois Gouget.
#ifdef SUN
char* strsep (char** str, const char* delims)
{
    if (!*str)		       // No more tokens
	return NULL;
    char* token = *str;
    while (**str) {
	if (strchr (delims, **str)) {
	    *(*str)++ = '\0';
	    return token;
	}
	++(*str);
    }
    // There is no other token
    *str = NULL;
    return token;
}

// timegm() is not available on Solaris
static time_t timegm (struct tm* t)
{
    time_t tl = mktime (t);
    if (tl == -1) {
	--t->tm_hour;
	tl = mktime (t);
	if (tl == -1)
	    return -1;	       // can't deal with output from strptime
	tl += 3600;
    }
    struct tm* tg = gmtime (&tl);
    tg->tm_isdst = 0;
    time_t tb = mktime (tg);
    if (tb == -1) {
	--tg->tm_hour;
	tb = mktime (tg);
	if (tb == -1)
	    return -1;	       // can't deal with output from gmtime
	tb += 3600;
    }
    return tl - (tb - tl);
}
#endif

// strcasestr stolen from: http://www.unixpapa.com/incnote/string.html
const char* s_strcasestr (const char* a, const char* b)
{
    const size_t lena = strlen (a), lenb = strlen (b);
    char f[3];
    snprintf (f, sizeof (f), "%c%c", tolower (*b), toupper (*b));
    for (size_t l = strcspn (a, f); l != lena; l += strcspn (a + l + 1, f) + 1)
	if (strncasecmp (a + l, b, lenb) == 0)
	    return a + l;
    return NULL;
}

//----------------------------------------------------------------------

char* iconvert (const char* inbuf)
{
    iconv_t cd;			// Iconvs conversion descriptor.
    if (_settings.global_charset)
	cd = iconv_open (_settings.global_charset, "UTF-8");
    else {
	const char* langcset = nl_langinfo (CODESET);
	if (strcasecmp (langcset, "UTF-8") == 0)
	    return strdup (inbuf);	// Already in UTF-8
	char target_charset[64];
	snprintf (target_charset, sizeof (target_charset), "%s//TRANSLIT", langcset);
	cd = iconv_open (target_charset, "UTF-8");
    }
    if (cd == (iconv_t) - 1)
	return NULL;

    size_t inbytesleft = strlen (inbuf), outbytesleft = inbytesleft;
    // We need two pointers so we do not lose the string starting position.
    char* outbuf = malloc (outbytesleft + 1), *outbuf_first = outbuf;

    if (iconv (cd, (char **) &inbuf, &inbytesleft, &outbuf, &outbytesleft) == (size_t) -1) {
	free (outbuf_first);
	iconv_close (cd);
	return NULL;
    }
    *outbuf = 0;
    iconv_close (cd);
    return outbuf_first;
}

// UIDejunk: remove html tags from feed description and convert
// html entities to something useful if we hit them.
// This function took almost forever to get right, but at least I learned
// that html entity &hellip; has nothing to do with Lucifer's ISP, but
// instead means "..." (3 dots, "and so on...").
char* UIDejunk (const char* feed_description)
{
    // Gracefully handle passed NULL ptr.
    if (feed_description == NULL)
	return strdup ("(null)");

    // Make a copy and point *start to it so we can free the stuff again!
    char* text = strdup (feed_description);	// feed_description copy
    char* start = text;		// Points to first char everytime. Need to free this.

    // If text begins with a tag, discard all of them.
    while (1) {
	if (text[0] == '<') {
	    strsep (&text, "<");
	    strsep (&text, ">");
	} else
	    break;
	if (text == NULL) {
	    free (start);
	    return strdup (_("No description available."));
	}
    }
    char* newtext = malloc (1);	// Detag'ed *text.
    newtext[0] = '\0';

    while (1) {
	// Strip tags... tagsoup mode.
	// strsep puts everything before "<" into detagged.
	const char* detagged = strsep (&text, "<");
	if (detagged == NULL)
	    break;

	// Replace <p> and <br> (in all incarnations) with newlines, but only
	// if there isn't already a following newline.
	if (text != NULL) {
	    if ((strncasecmp (text, "p", 1) == 0) || (strncasecmp (text, "br", 2) == 0)) {
		if ((strncasecmp (text, "br>\n", 4) != 0) && (strncasecmp (text, "br/>\n", 5) != 0) && (strncasecmp (text, "br />\n", 6) != 0) && (strncasecmp (text, "p>\n", 3) != 0)) {
		    newtext = realloc (newtext, strlen (newtext) + 2);
		    strcat (newtext, "\n");
		}
	    }
	}
	newtext = realloc (newtext, strlen (newtext) + strlen (detagged) + 1);

	// Now append detagged to newtext.
	strcat (newtext, detagged);

	// Advance *text to next position after the closed tag.
	const char* htmltag = strsep (&text, ">");
	if (htmltag == NULL)
	    break;

	if (s_strcasestr (htmltag, "img src") != NULL) {
#if 0
	    attribute = s_strcasestr (htmltag, "alt=");
	    if (attribute == NULL)
		continue;
	    size_t len = strlen (attribute);
	    newtext = realloc (newtext, strlen (newtext) + 6 + len + 2);
	    strcat (newtext, "[img: ");
	    strncat (newtext, attribute + 5, len - 6);
	    strcat (newtext, "]");
#endif
	    newtext = realloc (newtext, strlen (newtext) + 7);
	    strcat (newtext, "[img] ");
	}
#if 0
	else if (s_strcasestr (htmltag, "a href") != NULL) {
	    newtext = realloc (newtext, strlen (newtext) + 8);
	    strcat (newtext, "[link: ");
	} else if (strcasecmp (htmltag, "/a") == 0) {
	    newtext = realloc (newtext, strlen (newtext) + 2);
	    strcat (newtext, "]");
	}
#endif
    }
    free (start);

    CleanupString (newtext, 0);

    // See if there are any entities in the string at all.
    if (strchr (newtext, '&') != NULL) {
	text = strdup (newtext);
	start = text;
	free (newtext);

	newtext = malloc (1);
	newtext[0] = '\0';

	while (1) {
	    // Strip HTML entities.
	    const char* detagged = strsep (&text, "&");
	    if (detagged == NULL)
		break;

	    if (*detagged) {
		newtext = realloc (newtext, strlen (newtext) + strlen (detagged) + 1);
		strcat (newtext, detagged);
	    }
	    // Expand newtext by one char.
	    newtext = realloc (newtext, strlen (newtext) + 2);
	    // This might break if there is an & sign in the text.
	    const char* entity = strsep (&text, ";");
	    if (entity != NULL) {
		// XML defined entities.
		if (strcmp (entity, "amp") == 0) {
		    strcat (newtext, "&");
		    continue;
		} else if (strcmp (entity, "lt") == 0) {
		    strcat (newtext, "<");
		    continue;
		} else if (strcmp (entity, "gt") == 0) {
		    strcat (newtext, ">");
		    continue;
		} else if (strcmp (entity, "quot") == 0) {
		    strcat (newtext, "\"");
		    continue;
		} else if (strcmp (entity, "apos") == 0) {
		    strcat (newtext, "'");
		    continue;
		}
		// Decode user defined entities.
		bool found = false;
		for (const struct entity * cur_entity = _settings.html_entities; cur_entity; cur_entity = cur_entity->next) {
		    if (strcmp (entity, cur_entity->entity) == 0) {
			// We have found a matching entity.

			// If entity_length is more than 1 char we need to realloc
			// more space in newtext.
			if (cur_entity->entity_length > 1)
			    newtext = realloc (newtext, strlen (newtext) + cur_entity->entity_length + 1);

			// Append new entity.
			strcat (newtext, cur_entity->converted_entity);

			// Set found flag.
			found = true;

			// We can now leave the for loop.
			break;
		    }
		}

		// Try to parse some standard entities.
		if (!found) {
		    wchar_t ch = 0;
		    // See if it was a numeric entity.
		    if (entity[0] == '#') {
			if (entity[1] == 'x')
			    ch = strtoul (entity + 2, NULL, 16);
			else
			    ch = atol (entity + 1);
		    } else {
			const htmlEntityDesc* ep = htmlEntityLookup ((xmlChar*) entity);
			if (ep)
			    ch = ep->value;
		    }

		    if (ch > 0) {
#ifdef __STDC_ISO_10646__
			// Convert to locale encoding and append.
			size_t len = strlen (newtext);
			newtext = realloc (newtext, len + MB_CUR_MAX + 1);
			int mblen = wctomb (newtext + len, ch);
			// Only set found flag if the conversion worked.
			if (mblen > 0) {
			    newtext[len + mblen] = '\0';
			    found = true;
			}
#else
			// Since we can't use wctomb(), just convert ASCII.
			if (ch <= CHAR_MAX) {
			    sprintf (newtext + strlen (newtext), "%c", (char) ch);
			    found = true;
			}
#endif
		    }
		}
		// If nothing matched so far, put text back in.
		if (!found) {
		    // Changed into &+entity to avoid stray semicolons
		    // at the end of wrapped text if no entity matches.
		    newtext = realloc (newtext, strlen (newtext) + strlen (entity) + 2);
		    strcat (newtext, "&");
		    strcat (newtext, entity);
		}
	    } else
		break;
	}
	free (start);
    }
    return newtext;
}

// 5th try at a wrap text functions.
// My first version was broken, my second one sucked, my third try was
// so overcomplicated I didn't understand it anymore... Kianga tried
// the 4th version which corrupted some random memory unfortunately...
// but this one works. Heureka!
char* WrapText (const char* text, unsigned width)
{
    char* textblob = strdup (text);	// Working copy of text.
    char* start = textblob;

    char* line = malloc (1);	// One line of text with max width.
    line[0] = '\0';

    char* newtext = malloc (1);
    memset (newtext, 0, 1);

    while (1) {
	// First, cut at \n.
	char* chapter = strsep (&textblob, "\n");
	if (chapter == NULL)
	    break;
	while (1) {
	    char* savepos = chapter;	// Saved position pointer so we can go back in the string.
	    const char* chunk = strsep (&chapter, " ");

	    // Last chunk.
	    if (chunk == NULL) {
		if (line != NULL) {
		    newtext = realloc (newtext, strlen (newtext) + strlen (line) + 2);
		    strcat (newtext, line);
		    strcat (newtext, "\n");
		    line[0] = '\0';
		}
		break;
	    }

	    if (utf8_length (chunk) > width) {
		// First copy remaining stuff in line to newtext.
		newtext = realloc (newtext, strlen (newtext) + strlen (line) + 2);
		strcat (newtext, line);
		strcat (newtext, "\n");

		free (line);
		line = malloc (1);
		line[0] = '\0';

		// Then copy chunk with max length of line to newtext.
		line = realloc (line, width + 1);
		strncat (line, chunk, width - 5);
		strcat (line, "...");
		newtext = realloc (newtext, strlen (newtext) + width + 2);
		strcat (newtext, line);
		strcat (newtext, "\n");
		free (line);
		line = malloc (1);
		line[0] = '\0';
		continue;
	    }

	    if (strlen (line) + strlen (chunk) <= width) {
		line = realloc (line, strlen (line) + strlen (chunk) + 2);
		strcat (line, chunk);
		strcat (line, " ");
	    } else {
		// Why can chapter be NULL here anyway?
		if (chapter != NULL) {
		    --chapter;
		    chapter[0] = ' ';
		}
		chapter = savepos;
		newtext = realloc (newtext, strlen (newtext) + strlen (line) + 2);
		strcat (newtext, line);
		strcat (newtext, "\n");
		free (line);
		line = malloc (1);
		line[0] = '\0';
	    }
	}
    }
    free (line);
    free (start);
    return newtext;
}

// Remove leading whitspaces, newlines, tabs.
// This function should be safe for working on UTF-8 strings.
// tidyness: 0 = only suck chars from beginning of string
//           1 = extreme, vacuum everything along the string.
void CleanupString (char* string, int tidyness)
{
    // If we are passed a NULL pointer, leave it alone and return.
    if (string == NULL)
	return;

    size_t len = strlen (string);
    while (len && (string[0] == '\n' || string[0] == ' ' || string[0] == '\t')) {
	// len=strlen(string) does not include \0 of string.
	// But since we copy from *string+1 \0 gets included.
	// Delicate code. Think twice before it ends in buffer overflows.
	memmove (string, string + 1, len);
	--len;
    }

    // Remove trailing spaces.
    while (len > 1 && string[len - 1] == ' ') {
	string[len - 1] = 0;
	--len;
    }

    // Eat newlines and tabs along the whole string.
    for (size_t i = 0; i < len; ++i) {
	if (string[i] == '\t')
	    string[i] = ' ';
	if (tidyness == 1 && string[i] == '\n')
	    string[i] = ' ';
    }
}

// http://foo.bar/address.rdf -> http:__foo.bar_address.rdf
char* Hashify (const char* url)
{
    char* hashed_url = strdup (url);
    size_t len = strlen (hashed_url);

    // Don't allow filenames > 128 chars for teeny weeny operating systems.
    if (len > 128) {
	len = 128;
	hashed_url[128] = '\0';
    }

    for (size_t i = 0; i < len; ++i) {
	if (((hashed_url[i] < 32) || (hashed_url[i] > 38)) && ((hashed_url[i] < 43) || (hashed_url[i] > 46)) && ((hashed_url[i] < 48) || (hashed_url[i] > 90)) && ((hashed_url[i] < 97) || (hashed_url[i] > 122)) && (hashed_url[i] != 0))
	    hashed_url[i] = '_';

	// Cygwin doesn't seem to like anything besides a-z0-9 in filenames. Zap'em!
#ifdef __CYGWIN__
	if (((hashed_url[i] < 48) || (hashed_url[i] > 57)) && ((hashed_url[i] < 65) || (hashed_url[i] > 90)) && ((hashed_url[i] < 97) || (hashed_url[i] > 122)) && (hashed_url[i] != 0))
	    hashed_url[i] = '_';
#endif
    }

    return hashed_url;
}

char* genItemHash (const char* const* hashitems, unsigned items)
{
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    EVP_DigestInit (mdctx, EVP_md5());

    for (unsigned i = 0; i < items; ++i)
	if (hashitems[i])
	    EVP_DigestUpdate (mdctx, hashitems[i], strlen (hashitems[i]));

    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned md_len = 0;
    EVP_DigestFinal_ex (mdctx, md_value, &md_len);
    EVP_MD_CTX_free (mdctx);

    char hashtext [MD5_DIGEST_LENGTH*2 + 1];
    for (unsigned i = 0; i < md_len; ++i)
	sprintf (&hashtext[2*i], "%02hhx", md_value[i]);
    return strdup (hashtext);
}

// Date conversion
// 2004-11-20T19:45:00+00:00
time_t ISODateToUnix (const char* ISODate)
{
    // Do not crash with an empty tag
    if (!ISODate)
	return 0;
    struct tm t = { };
    // OpenBSD does not know %F == %Y-%m-%d
    if (!strptime (ISODate, "%Y-%m-%dT%T", &t) && !strptime (ISODate, "%Y-%m-%d", &t))
	return 0;
#ifdef __CYGWIN__
    return mktime (&t);
#else
    return timegm (&t);
#endif
}

// Sat, 20 Nov 2004 21:45:40 +0000
time_t pubDateToUnix (const char* pubDate)
{
    // Do not crash with an empty Tag
    if (!pubDate)
	return 0;
#ifdef LOCALEPATH
    // Cruft!
    // Save old locale so we can parse the stupid pubDate format.
    // However strftime is not really more intelligent since there is no
    // format string for abbr. month name NOT in the current locale. Grr.
    //
    // This is also not thread safe!
    char* oldlocale = setlocale (LC_TIME, NULL);
    if (oldlocale)
	setlocale (LC_TIME, "C");
#endif
    struct tm t = { };
    char* r = strptime (pubDate + strlen ("Sat, "), "%d %b %Y %T", &t);
#ifdef LOCALEPATH
    if (oldlocale)
	setlocale (LC_TIME, oldlocale);
#endif
    if (!r)
	return 0;
#ifdef __CYGWIN__
    return mktime (&t);
#else
    return timegm (&t);
#endif
}

char* unixToPostDateString (time_t unixDate)
{
    int len = 64;
    char* time_str = malloc (len);

    int strfstr_len = 32;
    char* time_strfstr = malloc (strfstr_len);

    struct tm t;
    gmtime_r (&unixDate, &t);

    strftime (time_strfstr, strfstr_len, _(", %H:%M"), &t);
    strcpy (time_str, _("Posted "));
    len -= strlen (_("Posted "));
    if (len <= 0)
	return NULL;

    int age = calcAgeInDays (&t);
    if (age == 0) {
	strncat (time_str, _("today"), len - 1);
	len -= strlen (_("today"));
	if (len <= 0)
	    return NULL;
	if (!(!t.tm_hour && !t.tm_min && !t.tm_sec))
	    strncat (time_str, time_strfstr, len - 1);
    } else if (age == 1) {
	strncat (time_str, _("yesterday"), len - 1);
	len -= strlen (_("yesterday"));
	if (len <= 0)
	    return NULL;
	if (!(!t.tm_hour && !t.tm_min && !t.tm_sec))
	    strncat (time_str, time_strfstr, len - 1);
    } else if ((age > 1) && (age < 7)) {
	char tmpstr[32];
	snprintf (tmpstr, sizeof (tmpstr), _("%d days ago"), age);
	strncat (time_str, tmpstr, len - 1);
	len -= strlen (tmpstr);
	if (len <= 0)
	    return NULL;
	if (!(!t.tm_hour && !t.tm_min && !t.tm_sec))
	    strncat (time_str, time_strfstr, len - 1);
    } else if (age == 7) {
	strncat (time_str, _("a week ago"), len - 1);
	len -= strlen (_("a week ago"));
	if (len <= 0)
	    return NULL;
	if (!(!t.tm_hour && !t.tm_min && !t.tm_sec))
	    strncat (time_str, time_strfstr, len - 1);
    } else if (age < 0) {
	strcpy (time_str, _("Not yet posted: "));
	len = 64 - strlen (_("Not yet posted: "));
	if (!t.tm_hour && !t.tm_min && !t.tm_sec)
	    strftime (time_strfstr, strfstr_len, _("%x"), &t);
	else
	    strftime (time_strfstr, strfstr_len, _("%x, %H:%M"), &t);
	strncat (time_str, time_strfstr, len - 1);
    } else {
	if (!t.tm_hour && !t.tm_min && !t.tm_sec)
	    strftime (time_strfstr, strfstr_len, _("%x"), &t);
	else
	    strftime (time_strfstr, strfstr_len, _("%x, %H:%M"), &t);
	strncat (time_str, time_strfstr, len - 1);
    }
    free (time_strfstr);

    return time_str;
}

static int calcAgeInDays (const struct tm* t)
{
    time_t unix_t = time (NULL);
    struct tm current_t;
    gmtime_r (&unix_t, &current_t);

    // (((current year - passed year) * 365) + current year day) - passed year day
    return (((current_t.tm_year - t->tm_year) * 365) + current_t.tm_yday) - t->tm_yday;
}
