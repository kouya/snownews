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

#include "net-support.h"
#include "conversions.h"
#include "ui-support.h"
#include "digcalc.h"

static char* ConstructBasicAuth (const char* username, const char* password)
{
    // Create base64 authinfo.

    // RFC 2617. Basic HTTP authentication.
    // Authorization: Basic username:password[base64 encoded]

    // Construct the cleartext authstring.
    char authstring[128];
    unsigned len = snprintf (authstring, sizeof (authstring), "%s:%s", username, password);
    char* encoded = base64encode (authstring, len);

    // "Authorization: Basic " + base64str + \r\n\0
    len = 21 + strlen (encoded) + 3;
    char* authinfo = malloc (len);
    snprintf (authinfo, len, "Authorization: Basic %s\r\n", encoded);
    free (encoded);
    return authinfo;
}

static char* GetRandomBytes (void)
{
    char raw[8];
    FILE* devrandom = fopen ("/dev/random", "r");
    if (devrandom) {
	fread (raw, sizeof (raw), 1, devrandom);
	fclose (devrandom);
    } else {
	for (unsigned i = 0; i < sizeof (raw); ++i)
	    raw[i] = rand();  // Use rand() if we don't have access to /dev/random.
    }
    char* randomness = calloc (sizeof (raw) * 2 + 1, 1);
    snprintf (randomness, sizeof (raw) * 2 + 1, "%hhx%hhx%hhx%hhx%hhx%hhx%hhx%hhx", raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6], raw[7]);
    return randomness;
}

static char* ConstructDigestAuth (const char* username, const char* password, const char* url, char* authdata)
{
    // Variables for the overcomplicated and annoying HTTP digest algo.
    char* cnonce = GetRandomBytes();
    char* realm = NULL, *qop = NULL, *nonce = NULL, *opaque = NULL;
    while (1) {
	char* token = strsep (&authdata, ", ");
	if (token == NULL)
	    break;

	if (strncasecmp (token, "realm", 5) == 0) {
	    unsigned len = strlen (token) - 8;
	    memmove (token, token + 7, len);
	    token[len] = '\0';
	    realm = strdup (token);
	} else if (strncasecmp (token, "qop", 3) == 0) {
	    unsigned len = strlen (token) - 6;
	    memmove (token, token + 5, len);
	    token[len] = '\0';
	    qop = strdup (token);
	} else if (strncasecmp (token, "nonce", 5) == 0) {
	    unsigned len = strlen (token) - 8;
	    memmove (token, token + 7, len);
	    token[len] = '\0';
	    nonce = strdup (token);
	} else if (strncasecmp (token, "opaque", 6) == 0) {
	    unsigned len = strlen (token) - 9;
	    memmove (token, token + 8, len);
	    token[len] = '\0';
	    opaque = strdup (token);
	}
    }

    HASHHEX HA1;
    DigestCalcHA1 ("md5", username, realm, password, nonce, cnonce, HA1);
    static const char szNonceCount[9] = "00000001";	// Can be always 1 if we never use the same cnonce twice.
    HASHHEX HA2 = "", Response;
    DigestCalcResponse (HA1, nonce, szNonceCount, cnonce, "auth", "GET", url, HA2, Response);

    // Determine length of Authorize header.
    //
    // Authorization: Digest username="(username)", realm="(realm)",
    // nonce="(nonce)", uri="(url)", algorithm=MD5, response="(Response)",
    // qop=(auth), nc=(szNonceCount), cnonce="deadbeef"
    //
    unsigned len = 32 + strlen (username) + 10 + strlen (realm) + 10 + strlen (nonce) + 8 + strlen (url) + 28 + strlen (Response) + 16 + strlen (szNonceCount) + 10 + strlen (cnonce) + 4;
    if (opaque)
	len += 6 + strlen (opaque) + 4;

    // Authorization header as sent to the server.
    char* authinfo = malloc (len);
    snprintf (authinfo, len, "Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", algorithm=MD5, response=\"%s\", qop=auth, nc=%s, cnonce=\"%s\"\r\n", username, realm, nonce, url, Response, szNonceCount, cnonce);
    free (realm);
    free (qop);
    free (nonce);
    free (cnonce);
    if (opaque)
	sprintf (authinfo + strlen (authinfo) - strlen ("\r\n"), ", opaque=\"%s\"\r\n", opaque);
    free (opaque);

    return authinfo;
}

// Authorization: Digest username="(username)", realm="(realm)",
// nonce="(nonce)", uri="(url)", algorithm=MD5, response="(Response)",
// qop=(auth), nc=(szNonceCount), cnonce="deadbeef"
int NetSupportAuth (struct feed* cur_ptr, const char* authdata, const char* url, const char* netbuf)
{
    // Reset cur_ptr->authinfo.
    free (cur_ptr->authinfo);
    cur_ptr->authinfo = NULL;

    // Catch invalid authdata.
    if (!authdata)
	return 1;
    else if (!strchr (authdata, ':'))	// No authinfo found in URL. This should not happen.
	return 1;

    // Parse username:password
    char* username = strdup (authdata);
    char* pwtok = username;
    strsep (&pwtok, ":");
    char* password = strdup (pwtok);

    // Extract requested auth type from webserver reply.
    char* header = strdup (netbuf);
    char* freeme = header;
    strsep (&header, " ");
    char* authtype = header;

    // Catch invalid server replies. authtype should contain at least _something_.
    if (!authtype) {
	free (freeme);
	free (username);
	free (password);
	return -1;
    }
    strsep (&header, " ");
    // header now contains:
    // Basic auth:  realm
    // Digest auth: realm + a lot of other stuff somehow needed by digest auth.

    // Determine auth type the server requests.
    if (strncasecmp (authtype, "Basic", 5) == 0)	// Basic auth.
	cur_ptr->authinfo = ConstructBasicAuth (username, password);
    else if (strncasecmp (authtype, "Digest", 6) == 0)	// Digest auth.
	cur_ptr->authinfo = ConstructDigestAuth (username, password, url, header);
    else {
	// Unkown auth type.
	free (freeme);
	free (username);
	free (password);
	return -1;
    }
    free (username);
    free (password);
    free (freeme);

    if (!cur_ptr->authinfo)
	return 2;
    return 0;
}

// HTTP token may only contain ASCII characters.
//
// Ensure that we don't hit the terminating \0 in a string
// with the for loop.
// The function also ensures that there is no NULL byte in the string.
// If given binary data return at once if we read beyond
// the boundary of sizeof(header).
//
int checkValidHTTPHeader (const unsigned char* header, unsigned size)
{
    unsigned len = strlen ((const char*) header);
    if (len > size)
	return -1;
    for (unsigned i = 0; i < len && header[i] != ':'; ++i)
	if ((header[i] < ' ' || header[i] > '~') && header[i] != '\r' && header[i] != '\n')
	    return -1;
    return 0;
}

int checkValidHTTPURL (const unsigned char* url)
{
    if (strncasecmp ((const char*) url, "http://", 7) != 0)
	return -1;
    for (unsigned i = 0, len = strlen ((const char*)url); i < len; ++i)
	if (url[i] < ' ' || url[i] > '~')
	    return -1;
    return 0;
}
