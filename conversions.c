/* Snownews - A lightweight console RSS newsreader
 * $Id: conversions.c 1173 2006-10-14 21:41:42Z kiza $
 *
 * Copyright 2003-2009 Oliver Feiler <kiza@kcore.de> and
 *                     Rene Puls <rpuls@gmx.net>
 *
 * conversions.c
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

#define _GNU_SOURCE
#include <sys/types.h>
#include <string.h>
#include <iconv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <libxml/HTMLparser.h>
#include <langinfo.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include "os-support.h"
#include "conversions.h"
#include "config.h"
#include "interface.h"
#include "ui-support.h"
#include "setup.h"
#include "digcalc.h"
#include "io-internal.h"

extern struct entity *first_entity;

extern char *forced_target_charset;

static int calcAgeInDays (const struct tm* t);

#ifdef STATIC_CONST_ICONV
char * iconvert (const char * inbuf) {
#else
char * iconvert (char * inbuf) {
#endif
	iconv_t cd;							/* Iconvs conversion descriptor. */
	char *outbuf, *outbuf_first;		/* We need two pointers so we do not lose
	                                      the string starting position. */
	char target_charset[64];
	size_t inbytesleft, outbytesleft;

	/*(void)strlcpy(target_charset, nl_langinfo(CODESET), sizeof(target_charset));*/
	strncpy(target_charset, nl_langinfo(CODESET), sizeof(target_charset));

	/* Take a shortcut. */
	if (strcasecmp (target_charset, "UTF-8") == 0)
		return strdup(inbuf);

	inbytesleft = strlen(inbuf);
	outbytesleft = strlen(inbuf);

	/*(void)strlcat(target_charset, "//TRANSLIT", sizeof(target_charset));*/
	strncat(target_charset, "//TRANSLIT", sizeof(target_charset));

	/* cd = iconv_open(nl_langinfo(CODESET), "UTF-8"); */
	if (forced_target_charset) {
		cd = iconv_open (forced_target_charset, "UTF-8");
	} else {
		cd = iconv_open (target_charset, "UTF-8");
	}
	if (cd == (iconv_t) -1) {
		return NULL;
	}

	outbuf = malloc (outbytesleft+1);
	outbuf_first = outbuf;

	if (iconv (cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft) == (size_t)-1) {
		free(outbuf_first);
		iconv_close(cd);
		return NULL;
	}

	*outbuf = 0;

	iconv_close (cd);

	return outbuf_first;
}


/* UIDejunk: remove crap (=html tags) from feed description and convert
 * html entities to something useful if we hit them.
 * This function took almost forever to get right, but at least I learned
 * that html entity &hellip; has nothing to do with Lucifer's ISP, but
 * instead means "..." (3 dots, "and so on...").
 */
char * UIDejunk (char * feed_description) {
	char *start;		/* Points to first char everytime. Need to free this. */
	char *text;			/* feed_description copy */
	char *newtext;		/* Detag'ed *text. */
	char *detagged;		/* Returned value from strsep. This is what we want. */
	char *htmltag;
/*	char *attribute;*/
	char *entity;
	struct entity *cur_entity;
	int found = 0;		/* User defined entity matched? */
/*	int len;*/
#ifdef __STDC_ISO_10646__
	int len;
	wchar_t ch;					/* Decoded numeric entity */
	int mblen;					/* Length of multi-byte entity */
#else
	unsigned long ch;			/* Decoded numeric entity */
#endif
	const htmlEntityDesc *ep;	/* For looking up HTML entities */

	/* Gracefully handle passed NULL ptr. */
	if (feed_description == NULL) {
		newtext = strdup("(null)");
		return newtext;
	}

	/* Make a copy and point *start to it so we can free the stuff again! */
	text = strdup (feed_description);
	start = text;


	/* If text begins with a tag, discard all of them. */
	while (1) {
		if (text[0] == '<') {
			strsep (&text, "<");
			strsep (&text, ">");
		} else
			break;
		if (text == NULL) {
			newtext = strdup (_("No description available."));
			free (start);
			return newtext;
		}
	}
	newtext = malloc (1);
	newtext[0] = '\0';

	while (1) {
		/* Strip tags... tagsoup mode. */
		/* strsep puts everything before "<" into detagged. */
		detagged = strsep (&text, "<");
		if (detagged == NULL)
			break;

		/* Replace <p> and <br> (in all incarnations) with newlines, but only
		   if there isn't already a following newline. */
		if (text != NULL) {
			if ((strncasecmp (text, "p", 1) == 0) ||
				(strncasecmp (text, "br", 2) == 0)) {
				if ((strncasecmp (text, "br>\n", 4) != 0) &&
					(strncasecmp (text, "br/>\n", 5) != 0) &&
					(strncasecmp (text, "br />\n", 6) != 0) &&
					(strncasecmp (text, "p>\n", 3) != 0)) {
					newtext = realloc (newtext, strlen(newtext)+2);
					strcat (newtext, "\n");
				}
			}
		}
		newtext = realloc (newtext, strlen(newtext)+strlen(detagged)+1);

		/* Now append detagged to newtext. */
		strcat (newtext, detagged);

		/* Advance *text to next position after the closed tag. */
		htmltag = strsep (&text, ">");
		if (htmltag == NULL)
			break;

		if (s_strcasestr(htmltag, "img src") != NULL) {
/*			attribute = s_strcasestr(htmltag, "alt=");
			if (attribute == NULL)
				continue;
			len = strlen(attribute);
			newtext = realloc(newtext, strlen(newtext)+6+len+2);
			strcat(newtext, "[img: ");
			strncat(newtext, attribute+5, len-6);
			strcat(newtext, "]");*/

			newtext = realloc(newtext, strlen(newtext)+7);
			strcat(newtext, "[img] ");
		}
/*		else if (s_strcasestr(htmltag, "a href") != NULL) {
			newtext = realloc(newtext, strlen(newtext)+8);
			strcat(newtext, "[link: ");
		} else if (strcasecmp(htmltag, "/a") == 0) {
			newtext = realloc(newtext, strlen(newtext)+2);
			strcat(newtext, "]");
		}*/
	}
	free (start);

	CleanupString (newtext, 0);

	/* See if there are any entities in the string at all. */
	if (strchr(newtext, '&') != NULL) {
		text = strdup (newtext);
		start = text;
		free (newtext);

		newtext = malloc (1);
		newtext[0] = '\0';

		while (1) {
			/* Strip HTML entities. */
			detagged = strsep (&text, "&");
			if (detagged == NULL)
				break;

			if (*detagged) {
				newtext = realloc (newtext, strlen(newtext)+strlen(detagged)+1);
				strcat (newtext, detagged);
			}
			/* Expand newtext by one char. */
			newtext = realloc (newtext, strlen(newtext)+2);
			/* This might break if there is an & sign in the text. */
			entity = strsep (&text, ";");
			if (entity != NULL) {
				/* XML defined entities. */
				if (strcmp(entity, "amp") == 0) {
					strcat (newtext, "&");
					continue;
				} else if (strcmp(entity, "lt") == 0) {
					strcat (newtext, "<");
					continue;
				} else if (strcmp(entity, "gt") == 0) {
					strcat (newtext, ">");
					continue;
				} else if (strcmp(entity, "quot") == 0) {
					strcat (newtext, "\"");
					continue;
				} else if (strcmp(entity, "apos") == 0) {
					strcat (newtext, "'");
					continue;
				}

				/* Decode user defined entities. */
				found = 0;
				for (cur_entity = first_entity; cur_entity != NULL; cur_entity = cur_entity->next_ptr) {
					if (strcmp (entity, cur_entity->entity) == 0) {
						/* We have found a matching entity. */

						/* If entity_length is more than 1 char we need to realloc
						   more space in newtext. */
						if (cur_entity->entity_length > 1)
							newtext = realloc (newtext,  strlen(newtext)+cur_entity->entity_length+1);

						/* Append new entity. */
						strcat (newtext, cur_entity->converted_entity);

						/* Set found flag. */
						found = 1;

						/* We can now leave the for loop. */
						break;
					}
				}

				/* Try to parse some standard entities. */
				if (!found) {
					/* See if it was a numeric entity. */
					if (entity[0] == '#') {
						if (entity[1] == 'x')
							ch = strtoul(entity + 2, NULL, 16);
						else
							ch = atol(entity + 1);
					}
					else if ((ep = htmlEntityLookup((xmlChar *)entity)) != NULL)
						ch = ep->value;
					else
						ch = 0;  /* Didn't recognize it. */

					if (ch > 0) {
#ifdef __STDC_ISO_10646__
						/* Convert to locale encoding and append. */
						len = strlen(newtext);
						newtext = realloc(newtext, len + MB_CUR_MAX + 1);
						mblen = wctomb(newtext + len, ch);
						/* Only set found flag if the conversion worked. */
						if (mblen > 0) {
							newtext[len + mblen] = '\0';
							found = 1;
						}
#else
						/* Since we can't use wctomb(), just convert ASCII. */
						if (ch <= 0x7F) {
							sprintf(newtext + strlen(newtext), "%c", (int)ch);
							found = 1;
						}
#endif
					}
				}

				/* If nothing matched so far, put text back in. */
				if (!found) {
					/* Changed into &+entity to avoid stray semicolons
					   at the end of wrapped text if no entity matches. */
					newtext = realloc (newtext, strlen(newtext)+strlen(entity)+2);
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

/* 5th try at a wrap text functions.
 * My first version was broken, my second one sucked, my third try was
 * so overcomplicated I didn't understand it anymore... Kianga tried
 * the 4th version which corrupted some random memory unfortunately...
 * but this one works. Heureka!
 */
char* WrapText (const char* text, unsigned width) {
	char *newtext;
	char *textblob;			/* Working copy of text. */
	char *chapter;
	char *line;				/* One line of text with max width. */
	char *savepos;			/* Saved position pointer so we can go back in the string. */
	char *chunk;
	char *start;
	/*
	char *p;
	int lena, lenb;
	*/

	textblob = strdup (text);
	start = textblob;


	line = malloc (1);
	line[0] = '\0';

	newtext = malloc(1);
	memset (newtext, 0, 1);

	while (1) {
		/* First, cut at \n. */
		chapter = strsep (&textblob, "\n");
		if (chapter == NULL)
			break;
		while (1) {
			savepos = chapter;
			chunk = strsep (&chapter, " ");

			/* Last chunk. */
			if (chunk == NULL) {
				if (line != NULL) {
					newtext = realloc (newtext, strlen(newtext)+strlen(line)+2);
					strcat (newtext, line);
					strcat (newtext, "\n");

					/* Faster replacement with memcpy.
					 * Overkill, overcomplicated and smelling of bugs all over.
					 */
					/*
					lena = strlen(newtext);
					lenb = strlen(line);
					newtext = realloc (newtext, lena+lenb+2);
					p = newtext+lena;
					memcpy (p, line, lenb);
					p += lenb;
					*p = '\n';
					p++;
					*p=0;
					*/

					line[0] = '\0';
				}
				break;
			}

			if (strlen(chunk) > width) {
				/* First copy remaining stuff in line to newtext. */
				newtext = realloc (newtext, strlen(newtext)+strlen(line)+2);
				strcat (newtext, line);
				strcat (newtext, "\n");

				free (line);
				line = malloc (1);
				line[0] = '\0';

				/* Then copy chunk with max length of line to newtext. */
				line = realloc (line, width+1);
				strncat (line, chunk, width-5);
				strcat (line, "...");
				newtext = realloc (newtext, strlen(newtext)+width+2);
				strcat (newtext, line);
				strcat (newtext, "\n");
				free (line);
				line = malloc (1);
				line[0] = '\0';
				continue;
			}

			if (strlen(line)+strlen(chunk) <= width) {
				line = realloc (line, strlen(line)+strlen(chunk)+2);
				strcat (line, chunk);
				strcat (line, " ");
			} else {
				/* Why the fuck can chapter be NULL here anyway? */
				if (chapter != NULL) {
					chapter--;
					chapter[0] = ' ';
				}
				chapter = savepos;
				newtext = realloc (newtext, strlen(newtext)+strlen(line)+2);
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

char *base64encode(char const *inbuf, unsigned int inbuf_size) {
	static unsigned char const alphabet[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	char *outbuf = NULL;
	unsigned int inbuf_pos = 0;
	unsigned int outbuf_pos = 0;
	unsigned int outbuf_size = 0;
	int bits = 0;
	int char_count = 0;

	outbuf = malloc(1);

	while (inbuf_pos < inbuf_size) {

		bits |= *inbuf;
		char_count++;

		if (char_count == 3) {
			outbuf = realloc(outbuf, outbuf_size+4);
			outbuf_size += 4;
			outbuf[outbuf_pos+0] = alphabet[bits >> 18];
			outbuf[outbuf_pos+1] = alphabet[(bits >> 12) & 0x3f];
			outbuf[outbuf_pos+2] = alphabet[(bits >> 6) & 0x3f];
			outbuf[outbuf_pos+3] = alphabet[bits & 0x3f];
			outbuf_pos += 4;
			bits = 0;
			char_count = 0;
		}

		inbuf++;
		inbuf_pos++;
		bits <<= 8;
	}

	if (char_count > 0) {
		bits <<= 16 - (8 * char_count);
		outbuf = realloc(outbuf, outbuf_size+4);
		outbuf_size += 4;
		outbuf[outbuf_pos+0] = alphabet[bits >> 18];
		outbuf[outbuf_pos+1] = alphabet[(bits >> 12) & 0x3f];
		if (char_count == 1) {
			outbuf[outbuf_pos+2] = '=';
			outbuf[outbuf_pos+3] = '=';
		} else {
			outbuf[outbuf_pos+2] = alphabet[(bits >> 6) & 0x3f];
			outbuf[outbuf_pos+3] = '=';
		}
		outbuf_pos += 4;
	}

	outbuf = realloc(outbuf, outbuf_size+1);
	outbuf[outbuf_pos] = 0;

	return outbuf;
}

#ifdef USE_UNSUPPORTED_AND_BROKEN_CODE
/* Returns NULL on invalid input */
char* decodechunked(char * chunked, unsigned int *inputlen) {
#warning ===The function decodedechunked() is not safe for binary data. Since you specifically requested it to be compiled in you probably know better what you are doing than me. Do not report bugs for this code.===
	char *orig = chunked, *dest = chunked;
	unsigned long chunklen;

	/* We can reuse the same buffer to dechunkify it:
	 * the data size will never increase. */
	while((chunklen = strtoul(orig, &orig, 16))) {
		/* process one more chunk: */
		/* skip chunk-extension part */
		while(*orig && (*orig != '\r'))
			orig++;
		/* skip '\r\n' after chunk length */
		orig += 2;
		if(( chunklen > (chunked + *inputlen - orig)))
			/* insane chunk length. Well... */
			return NULL;
		memmove(dest, orig, chunklen);
		dest += chunklen;
		orig += chunklen;
		/* and go to the next chunk */
	}
	*dest = '\0';
	*inputlen = dest - chunked;

	return chunked;
}
#endif

/* Remove leading whitspaces, newlines, tabs.
 * This function should be safe for working on UTF-8 strings.
 * tidyness: 0 = only suck chars from beginning of string
 *           1 = extreme, vacuum everything along the string.
 */
void CleanupString (char * string, int tidyness) {
	int len, i;

	/* If we are passed a NULL pointer, leave it alone and return. */
	if (string == NULL)
		return;

	len = strlen(string);

	while ((string[0] == '\n' || string [0] == ' ' || string [0] == '\t') &&
			(len > 0)) {
		/* len=strlen(string) does not include \0 of string.
		   But since we copy from *string+1 \0 gets included.
		   Delicate code. Think twice before it ends in buffer overflows. */
		memmove (string, string+1, len);
		len--;
	}

	/* Remove trailing spaces. */
	while ((len > 1) && (string[len-1] == ' ')) {
		string[len-1] = 0;
		len--;
	}

	len = strlen(string);
	/* Eat newlines and tabs along the whole string. */
	for (i = 0; i < len; i++) {
		if (string[i] == '\t') {
			string[i] = ' ';
		}
		if (tidyness == 1 && string[i] == '\n') {
			string[i] = ' ';
		}
	}
}

/* http://foo.bar/address.rdf -> http:__foo.bar_address.rdf */
char * Hashify (const char * url) {
	int i, len;
	char *hashed_url;

	hashed_url = strdup(url);

	len = strlen(hashed_url);

	/* Don't allow filenames > 128 chars for teeny weeny
	 * operating systems.
	 */
	if (len > 128) {
		len = 128;
		hashed_url[128] = '\0';
	}

	for (i = 0; i < len; i++) {
		if (((hashed_url[i] < 32) || (hashed_url[i] > 38)) &&
			((hashed_url[i] < 43) || (hashed_url[i] > 46)) &&
			((hashed_url[i] < 48) || (hashed_url[i] > 90)) &&
			((hashed_url[i] < 97) || (hashed_url[i] > 122)) &&
			(hashed_url[i] != 0))
			hashed_url[i] = '_';

		/* Cygwin doesn't seem to like anything besides a-z0-9 in filenames.
		   Zap'em! */
#ifdef __CYGWIN__
		if (((hashed_url[i] < 48) || (hashed_url[i] > 57)) &&
			((hashed_url[i] < 65) || (hashed_url[i] > 90)) &&
			((hashed_url[i] < 97) || (hashed_url[i] > 122)) &&
			(hashed_url[i] != 0))
			hashed_url[i] = '_';
#endif
	}

	return hashed_url;
}

char * genItemHash (const char* const* hashitems, int items) {
	EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
	EVP_DigestInit(mdctx, EVP_md5());

	for (int i = 0; i < items; ++i)
		if (hashitems[i])
			EVP_DigestUpdate (mdctx, hashitems[i], strlen(hashitems[i]));

	unsigned char md_value [EVP_MAX_MD_SIZE];
	unsigned md_len = 0;
	EVP_DigestFinal_ex (mdctx, md_value, &md_len);
	EVP_MD_CTX_free (mdctx);

	char md5_hex [MD5_DIGEST_LENGTH*2+1];
	for (unsigned i = 0; i < md_len; ++i)
		sprintf (&md5_hex[2*i], "%02x", md_value[i]);
	return strdup(md5_hex);
}

/* Date conversion */
/* 2004-11-20T19:45:00+00:00 */
int ISODateToUnix (char const * const ISODate) {
	struct tm *t;
	time_t tt = 0;
	int time_unix = 0;

	/* Do not crash with an empty tag */
	if (ISODate == NULL)
		return 0;

	t = malloc(sizeof(struct tm));
	gmtime_r(&tt, t);

	/* OpenBSD does not know %F == %Y-%m-%d
	 * <insert inflamatory comment here> */
	if (strptime(ISODate, "%Y-%m-%dT%T", t)) {
#ifdef __CYGWIN__
		time_unix = mktime(t);
#else
		time_unix = timegm(t);
#endif
	} else if (strptime(ISODate, "%Y-%m-%d", t)) {
#ifdef __CYGWIN__
		time_unix = mktime(t);
#else
		time_unix = timegm(t);
#endif
	}

	free (t);
	return time_unix;
}
/* Sat, 20 Nov 2004 21:45:40 +0000 */
int pubDateToUnix (char const * const pubDate) {
	char const *start_here;
	struct tm *t;
	time_t tt = 0;
	int time_unix = 0;
	char *oldlocale;

	/* Do not crash with an empty Tag */
	if (pubDate == NULL)
		return 0;

	start_here = pubDate;
	start_here += 5;

	t = malloc(sizeof(struct tm));
	gmtime_r(&tt, t);

#ifdef LOCALEPATH
	/* Cruft!
	 * Save old locale so we can parse the stupid pubDate format.
	 * However strftime is not really more intelligent since there is no
	 * format string for abbr. month name NOT in the current locale. Grr.
	 *
	 * This is also not thread safe!
	 */
	oldlocale = strdup(setlocale(LC_TIME, NULL));
	if (oldlocale) {
		/* Only reset if setlocale returned something !NULL.
		 * Will not parse the date correctly in this case, but will also not
		 * overwrite the user's locale setting.
		 */
		setlocale(LC_TIME, "C");
	}
#endif

	if (strptime(start_here, "%d %b %Y %T", t)) {
#ifdef __CYGWIN__
		time_unix = mktime(t);
#else
		time_unix = timegm(t);
#endif
	}

#ifdef LOCALEPATH
	if (oldlocale) {
		setlocale(LC_TIME, oldlocale);
		free (oldlocale);
	}
#endif

	free (t);
	return time_unix;
}
char * unixToPostDateString (int unixDate) {
	time_t unix_t;
	struct tm t;
	char *time_str;
	int len = 64;
	char *time_strfstr;
	int strfstr_len = 32;
	char tmpstr[32];
	int age;

	time_str = malloc(len);
	time_strfstr = malloc(strfstr_len);

	unix_t = unixDate;
	gmtime_r(&unix_t, &t);

	age = calcAgeInDays(&t);

	strftime(time_strfstr, strfstr_len, _(", %H:%M"), &t);
	strcpy(time_str, _("Posted "));
	len -= strlen(_("Posted "));
	if (len <= 0)
		return NULL;

	if (age == 0) {
		strncat(time_str, _("today"), len-1);
		len -= strlen(_("today"));
		if (len <= 0)
			return NULL;
		if (!(!t.tm_hour && !t.tm_min && !t.tm_sec))
			strncat(time_str, time_strfstr, len-1);
	} else if (age == 1) {
		strncat(time_str, _("yesterday"), len-1);
		len -= strlen(_("yesterday"));
		if (len <= 0)
			return NULL;
		if (!(!t.tm_hour && !t.tm_min && !t.tm_sec))
			strncat(time_str, time_strfstr, len-1);
	} else if ((age > 1) && (age < 7)) {
		snprintf(tmpstr, sizeof(tmpstr), _("%d days ago"), age);
		strncat(time_str, tmpstr, len-1);
		len -= strlen(tmpstr);
		if (len <= 0)
			return NULL;
		if (!(!t.tm_hour && !t.tm_min && !t.tm_sec))
			strncat(time_str, time_strfstr, len-1);
	} else if (age == 7) {
		strncat(time_str, _("a week ago"), len-1);
		len -= strlen(_("a week ago"));
		if (len <= 0)
			return NULL;
		if (!(!t.tm_hour && !t.tm_min && !t.tm_sec))
			strncat(time_str, time_strfstr, len-1);
	} else if (age < 0) {
		strcpy(time_str, _("Not yet posted: "));
		len = 64 - strlen(_("Not yet posted: "));
		if (!t.tm_hour && !t.tm_min && !t.tm_sec)
			strftime(time_strfstr, strfstr_len, _("%x"), &t);
		else
			strftime(time_strfstr, strfstr_len, _("%x, %H:%M"), &t);
		strncat(time_str, time_strfstr, len-1);
	} else {
		if (!t.tm_hour && !t.tm_min && !t.tm_sec)
			strftime(time_strfstr, strfstr_len, _("%x"), &t);
		else
			strftime(time_strfstr, strfstr_len, _("%x, %H:%M"), &t);
		strncat(time_str, time_strfstr, len-1);
	}
	free (time_strfstr);

	return time_str;
}

static int calcAgeInDays (const struct tm* t) {
	time_t unix_t = time(NULL);
	struct tm current_t;
	gmtime_r(&unix_t, &current_t);

	/* (((current year - passed year) * 365) + current year day) - passed year day */
	return (((current_t.tm_year - t->tm_year) * 365) + current_t.tm_yday) - t->tm_yday;
}
