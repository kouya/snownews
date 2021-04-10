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

#include "netio.h"
#include "io-internal.h"
#include "net-support.h"
#include "ui-support.h"
#include "zlib_interface.h"
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>

enum {
    MAX_HTTP_REDIRECTS = 10,	// Maximum number of redirects we will follow.
    NET_TIMEOUT = 20	       // Global network timeout in sec
};

enum PollOp {
    NET_READ = 1,
    NET_WRITE
};

// Waits NET_TIMEOUT seconds for the socket to return data.
//
// Returns
//
//      0       Socket is ready
//      -1      Error occurred (netio_error is set)
//
static int NetPoll (struct feed* cur_ptr, const int* my_socket, enum PollOp rw)
{
    fd_set fds;
    FD_ZERO (&fds);
    FD_SET (*my_socket, &fds);
    fd_set* prfds = (rw == NET_READ ? &fds : NULL);
    fd_set* pwfds = (rw == NET_WRITE ? &fds : NULL);
    struct timeval tv = {.tv_sec = NET_TIMEOUT };
    if (select (*my_socket + 1, prfds, pwfds, NULL, &tv) == 0) {
	cur_ptr->netio_error = NET_ERR_TIMEOUT;
	return -1;
    }
    if (FD_ISSET (*my_socket, &fds))
	return 0;
    cur_ptr->netio_error = NET_ERR_UNKNOWN;
    return -1;
}

// Connect network sockets.
//
// Returns
//
//      0       Connected
//      -1      Error occured (netio_error is set)
//
static int NetConnect (int* my_socket, const char* host, struct feed* cur_ptr, bool httpsproto __attribute__((unused)), bool suppressoutput)
{
    char* realhost = strdup (host);
    unsigned short port;
    if (sscanf (host, "%[^:]:%hd", realhost, &port) != 2)
	port = 80;

    if (!suppressoutput) {
	char stmsg[128];
	snprintf (stmsg, sizeof (stmsg), _("Downloading \"%s\""), cur_ptr->title ? cur_ptr->title : cur_ptr->feedurl);
	UIStatus (stmsg, 0, 0);
    }
    // Create a inet stream TCP socket.
    *my_socket = socket (AF_INET, SOCK_STREAM, 0);
    if (*my_socket == -1) {
	cur_ptr->netio_error = NET_ERR_SOCK_ERR;
	return -1;
    }
    // If _settings.proxyport is 0 we didn't execute the if http_proxy statement in main
    // so there is no proxy. On any other value of proxyport do proxyrequests instead.
    if (_settings.proxyport == 0) {
	// Lookup remote IP.
	struct hostent* remotehost = gethostbyname (realhost);
	if (!remotehost) {
	    close (*my_socket);
	    free (realhost);
	    cur_ptr->netio_error = NET_ERR_HOST_NOT_FOUND;
	    return -1;
	}
	// Set the remote address.
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_port = htons (port);
	memcpy (&address.sin_addr.s_addr, remotehost->h_addr_list[0], remotehost->h_length);

	// Connect socket.
	cur_ptr->connectresult = connect (*my_socket, (struct sockaddr*) &address, sizeof (address));

	// Check if we're already connected.
	// BSDs will return 0 on connect even in nonblock if connect was fast enough.
	if (cur_ptr->connectresult != 0) {
	    // If errno is not EINPROGRESS, the connect went wrong.
	    if (errno != EINPROGRESS) {
		close (*my_socket);
		free (realhost);
		cur_ptr->netio_error = NET_ERR_CONN_REFUSED;
		return -1;
	    }

	    if (NetPoll (cur_ptr, my_socket, NET_WRITE) == -1) {
		close (*my_socket);
		free (realhost);
		return -1;
	    }
	    // We get errno of connect back via getsockopt SO_ERROR (into connectresult).
	    socklen_t len = sizeof (cur_ptr->connectresult);
	    getsockopt (*my_socket, SOL_SOCKET, SO_ERROR, &cur_ptr->connectresult, &len);

	    if (cur_ptr->connectresult != 0) {
		close (*my_socket);
		free (realhost);
		cur_ptr->netio_error = NET_ERR_CONN_FAILED;	// ->strerror(cur_ptr->connectresult)
		return -1;
	    }
	}
    } else {
	// Lookup proxyserver IP.
	struct hostent* remotehost = gethostbyname (_settings.proxyname);
	if (!remotehost) {
	    close (*my_socket);
	    free (realhost);
	    cur_ptr->netio_error = NET_ERR_HOST_NOT_FOUND;
	    return -1;
	}
	// Set the remote address.
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_port = htons (_settings.proxyport);
	memcpy (&address.sin_addr.s_addr, remotehost->h_addr_list[0], remotehost->h_length);

	// Connect socket.
	cur_ptr->connectresult = connect (*my_socket, (struct sockaddr*) &address, sizeof (address));

	// Check if we're already connected.
	// BSDs will return 0 on connect even in nonblock if connect was fast enough.
	if (cur_ptr->connectresult != 0) {
	    if (errno != EINPROGRESS) {
		close (*my_socket);
		free (realhost);
		cur_ptr->netio_error = NET_ERR_CONN_REFUSED;
		return -1;
	    }

	    if (NetPoll (cur_ptr, my_socket, NET_WRITE) == -1) {
		close (*my_socket);
		free (realhost);
		return -1;
	    }

	    socklen_t len = sizeof (cur_ptr->connectresult);
	    getsockopt (*my_socket, SOL_SOCKET, SO_ERROR, &cur_ptr->connectresult, &len);

	    if (cur_ptr->connectresult != 0) {
		close (*my_socket);
		free (realhost);
		cur_ptr->netio_error = NET_ERR_CONN_FAILED;	// ->strerror(cur_ptr->connectresult)
		return -1;
	    }
	}
    }
    free (realhost);
    return 0;
}

// Main network function.
// (Now with a useful function description *g*)
//
// This function returns the HTTP request's body (deflating gzip encoded data
// if needed).
// Updates passed feed struct with values gathered from webserver.
// Handles all redirection and HTTP status decoding.
// Returns NULL pointer if no data was received and sets netio_error.
//
static char* NetIO (int* my_socket, char* host, char* url, struct feed* cur_ptr, const char* authdata, bool httpsproto, bool suppressoutput)
{
    if (!suppressoutput) {
	char stmsg[256];
	if (cur_ptr->title == NULL)
	    snprintf (stmsg, sizeof (stmsg), _("Downloading \"http://%s%s\""), host, url);
	else
	    snprintf (stmsg, sizeof (stmsg), _("Downloading \"%s\""), cur_ptr->title);
	UIStatus (stmsg, 0, 0);
    }
    // Goto label to redirect reconnect.
  tryagain:

    // Reconstruct digest authinfo for every request so we don't reuse
    // the same nonce value for more than one request.
    // This happens one superflous time on 303 redirects.
    if (cur_ptr->authinfo && cur_ptr->servauth)
	if (strstr (cur_ptr->authinfo, " Digest "))
	    NetSupportAuth (cur_ptr, authdata, url, cur_ptr->servauth);

    // Open socket.
    FILE* stream = fdopen (*my_socket, "r+");
    if (!stream) {
	// This is a serious non-continueable OS error as it will probably not
	// go away if we retry.

	// BeOS will stupidly return SUCCESS here making this code silently fail on BeOS.
	cur_ptr->netio_error = NET_ERR_SOCK_ERR;
	return NULL;
    }
    // Again is _settings.proxyport == 0, non proxy mode, otherwise make proxy requests.
    if (_settings.proxyport == 0) {
	// Request URL from HTTP server.
	if (cur_ptr->lastmodified != NULL) {
	    fprintf (stream, "GET %s HTTP/1.0\r\nAccept-Encoding: gzip\r\nAccept: application/rdf+xml,application/rss+xml,application/xml,text/xml;q=0.9,*/*;q=0.1\r\nUser-Agent: %s\r\nConnection: close\r\nHost: %s\r\nIf-Modified-Since: %s\r\n%s%s\r\n",
		     url, _settings.useragent, host, cur_ptr->lastmodified, (cur_ptr->authinfo ? cur_ptr->authinfo : ""), (cur_ptr->cookies ? cur_ptr->cookies : ""));
	} else {
	    fprintf (stream, "GET %s HTTP/1.0\r\nAccept-Encoding: gzip\r\nAccept: application/rdf+xml,application/rss+xml,application/xml,text/xml;q=0.9,*/*;q=0.1\r\nUser-Agent: %s\r\nConnection: close\r\nHost: %s\r\n%s%s\r\n", url, _settings.useragent,
		     host, (cur_ptr->authinfo ? cur_ptr->authinfo : ""), (cur_ptr->cookies ? cur_ptr->cookies : ""));
	}
	fflush (stream);       // We love Solaris, don't we?
    } else {
	// Request URL from HTTP server.
	if (cur_ptr->lastmodified != NULL) {
	    fprintf (stream,
		     "GET http://%s%s HTTP/1.0\r\nAccept-Encoding: gzip\r\nAccept: application/rdf+xml,application/rss+xml,application/xml,text/xml;q=0.9,*/*;q=0.1\r\nUser-Agent: %s\r\nConnection: close\r\nHost: %s\r\nIf-Modified-Since: %s\r\n%s%s\r\n",
		     host, url, _settings.useragent, host, cur_ptr->lastmodified, (cur_ptr->authinfo ? cur_ptr->authinfo : ""), (cur_ptr->cookies ? cur_ptr->cookies : ""));
	} else {
	    fprintf (stream, "GET http://%s%s HTTP/1.0\r\nAccept-Encoding: gzip\r\nAccept: application/rdf+xml,application/rss+xml,application/xml,text/xml;q=0.9,*/*;q=0.1\r\nUser-Agent: %s\r\nConnection: close\r\nHost: %s\r\n%s%s\r\n", host, url,
		     _settings.useragent, host, (cur_ptr->authinfo ? cur_ptr->authinfo : ""), (cur_ptr->cookies ? cur_ptr->cookies : ""));
	}
	fflush (stream);       // We love Solaris, don't we?
    }

    if (NetPoll (cur_ptr, my_socket, NET_READ) == -1) {
	fclose (stream);
	return NULL;
    }

    char servreply[128];	// First line of server reply
    if ((fgets (servreply, sizeof (servreply), stream)) == NULL) {
	fclose (stream);
	return NULL;
    }
    if (checkValidHTTPHeader ((unsigned char*) servreply, sizeof (servreply)) != 0) {
	cur_ptr->netio_error = NET_ERR_HTTP_PROTO_ERR;
	fclose (stream);
	return NULL;
    }

    char* tmpstatus = strdup (servreply);
    char* savestart = tmpstatus;

    char httpstatus[4] = { };  // HTTP status sent by server.
    // Set pointer to char after first space.
    // HTTP/1.0 200 OK
    //          ^
    // Copy three bytes into httpstatus.
    strsep (&tmpstatus, " ");
    if (tmpstatus == NULL) {
	cur_ptr->netio_error = NET_ERR_HTTP_PROTO_ERR;
	fclose (stream);
	free (savestart);      // Probably more leaks when doing auth and abort here.
	return NULL;
    }
    strncpy (httpstatus, tmpstatus, 3);
    free (savestart);

    cur_ptr->lasthttpstatus = atoi (httpstatus);

    unsigned redirectcount = 0;	// Number of HTTP redirects followed.

    unsigned tmphttpstatus = cur_ptr->lasthttpstatus;
    bool handled = true;
    // Check HTTP server response and handle redirects.
    do {
	switch (tmphttpstatus) {
	    case 200:	       // OK
		// Received good status from server, clear problem field.
		cur_ptr->netio_error = NET_ERR_OK;
		cur_ptr->problem = false;

		// Avoid looping on 20x status codes.
		handled = true;
		break;
	    case 300:	       // Multiple choice and everything 300 not handled is fatal.
		cur_ptr->netio_error = NET_ERR_HTTP_NON_200;
		fclose (stream);
		return NULL;
	    case 301:
		// Permanent redirect. Change feed->feedurl to new location.
		// Done some way down when we have extracted the new url.
	    case 302:	       // Found
	    case 303:	       // See Other
	    case 307:	       // Temp redirect. This is HTTP/1.1
		// Give up if we reach MAX_HTTP_REDIRECTS to avoid loops.
		if (++redirectcount > MAX_HTTP_REDIRECTS) {
		    cur_ptr->netio_error = NET_ERR_REDIRECT_COUNT_ERR;
		    fclose (stream);
		    return NULL;
		}

		while (!feof (stream)) {
		    char netbuf[BUFSIZ];	// Network read buffer.
		    if ((fgets (netbuf, sizeof (netbuf), stream)) == NULL) {
			// Something bad happened. Server sent stupid stuff.
			cur_ptr->netio_error = NET_ERR_HTTP_PROTO_ERR;
			fclose (stream);
			return NULL;
		    }

		    if (checkValidHTTPHeader ((unsigned char*) netbuf, sizeof (netbuf)) != 0) {
			cur_ptr->netio_error = NET_ERR_HTTP_PROTO_ERR;
			fclose (stream);
			return NULL;
		    }
		    // Split netbuf into hostname and trailing url.
		    // Place hostname in *newhost and tail into *newurl.
		    // Close old connection and reconnect to server.

		    // Do not touch any of the following code! :P
		    if (strncasecmp (netbuf, "Location", 8) == 0) {
			char* redirecttarget = strdup (netbuf);
			char* redirecttargetbase = redirecttarget;

			// Remove trailing \r\n from line.
			redirecttarget[strlen (redirecttarget) - 2] = 0;

			// In theory pointer should now be after the space char
			// after the word "Location:"
			strsep (&redirecttarget, " ");

			if (!redirecttarget) {
			    cur_ptr->problem = true;
			    cur_ptr->netio_error = NET_ERR_REDIRECT_ERR;
			    free (redirecttargetbase);
			    fclose (stream);
			    return NULL;
			}
			// Location must start with "http", otherwise switch on quirksmode.
			bool quirksmode = false;	// IIS operation mode.
			if (strncmp (redirecttarget, "http", 4) != 0)
			    quirksmode = true;

			// If the Location header is invalid we need to construct
			// a correct one here before proceeding with the program.
			// This makes headers like
			// "Location: protocol.rdf" work.
			// In violalation of RFC1945, RFC2616.
			char* newlocation;
			if (quirksmode) {
			    unsigned len = 7 + strlen (host) + strlen (redirecttarget) + 3;
			    newlocation = malloc (len);
			    memset (newlocation, 0, len);
			    strcat (newlocation, "http://");
			    strcat (newlocation, host);
			    if (redirecttarget[0] != '/')
				strcat (newlocation, "/");
			    strcat (newlocation, redirecttarget);
			} else
			    newlocation = strdup (redirecttarget);
			free (redirecttargetbase);

			// Change cur_ptr->feedurl on 301.
			if (cur_ptr->lasthttpstatus == 301) {
			    // Check for valid redirection URL
			    if (checkValidHTTPURL ((unsigned char*) newlocation) != 0) {
				cur_ptr->problem = true;
				cur_ptr->netio_error = NET_ERR_REDIRECT_ERR;
				fclose (stream);
				return NULL;
			    }
			    if (!suppressoutput) {
				UIStatus (_("URL points to permanent redirect, updating with new location..."), 1, 0);
				syslog (LOG_NOTICE, _("URL points to permanent redirect, updating with new location..."));
			    }
			    free (cur_ptr->feedurl);
			    if (authdata == NULL)
				cur_ptr->feedurl = strdup (newlocation);
			    else {
				// Include authdata in newly constructed URL.
				unsigned len = strlen (authdata) + strlen (newlocation) + 2;
				cur_ptr->feedurl = malloc (len);
				char* newurl = strdup (newlocation);
				char* newurlbase = newurl;
				strsep (&newurl, "/");
				strsep (&newurl, "/");
				snprintf (cur_ptr->feedurl, len, "http://%s@%s", authdata, newurl);
				free (newurlbase);
			    }
			}

			char* newlocationbase = newlocation;
			strsep (&newlocation, "/");
			strsep (&newlocation, "/");
			char* tmphost = newlocation;
			// The following line \0-terminates tmphost in overwriting the first
			// / after the hostname.
			strsep (&newlocation, "/");

			// newlocation must now be the absolute path on newhost.
			// If not we've been redirected to somewhere unexpected
			// (oh yeah, no offsite linking, go to our front page).
			// Say goodbye to the webserver in this case. In fact, we don't
			// even say goodbye, but just drop the connection.
			if (newlocation == NULL) {
			    cur_ptr->netio_error = NET_ERR_REDIRECT_ERR;
			    fclose (stream);
			    return NULL;
			}

			char* newhost = strdup (tmphost);
			--newlocation;
			newlocation[0] = '/';
			char* newurl = strdup (newlocation);

			free (newlocationbase);

			// Close connection.
			fclose (stream);

			// Reconnect to server.
			if (NetConnect (my_socket, newhost, cur_ptr, httpsproto, suppressoutput))
			    return NULL;

			host = newhost;
			url = newurl;

			goto tryagain;
		    }
		}
		break;
	    case 304:
		// Not modified received. We can close stream and return from here.
		// Not very friendly though. :)
		fclose (stream);
		// Received good status from server, clear problem field.
		cur_ptr->netio_error = NET_ERR_OK;
		cur_ptr->problem = false;

		// This should be freed everywhere where we return
		// and current feed uses auth.
		if (redirectcount > 0 && authdata) {
		    free (host);
		    free (url);
		}
		return NULL;
	    case 401:
		// Authorization.
		// Parse rest of header and rerequest URL from server using auth mechanism
		// requested in WWW-Authenticate header field. (Basic or Digest)
		break;
	    case 404:
		cur_ptr->netio_error = NET_ERR_HTTP_404;
		fclose (stream);
		return NULL;
	    case 410:	       // The feed is gone. Politely remind the user to unsubscribe.
		cur_ptr->netio_error = NET_ERR_HTTP_410;
		fclose (stream);
		return NULL;
	    case 400:
		cur_ptr->netio_error = NET_ERR_HTTP_NON_200;
		fclose (stream);
		return NULL;
	    default:
		// unknown error codes have to be treated like the base class
		if (handled) {
		    // first pass, modify error code to base class
		    handled = false;
		    tmphttpstatus -= tmphttpstatus % 100;
		} else {
		    // second pass, give up on unknown error base class
		    cur_ptr->netio_error = NET_ERR_HTTP_NON_200;
		    syslog (LOG_ERR, "%s", servreply);
		    fclose (stream);
		    return NULL;
		}
	}
    } while (!handled);

#ifdef USE_UNSUPPORTED_AND_BROKEN_CODE
    bool chunked = false;	// Content-Encoding: chunked received?
#endif
    bool inflate = false;	// Whether feed data needs decompressed with zlib.
    bool authfailed = false;	// Avoid repeating failed auth requests endlessly.

    // Read rest of HTTP header and parse what we need.
    while (!feof (stream)) {
	if (NetPoll (cur_ptr, my_socket, NET_READ) == -1) {
	    fclose (stream);
	    return NULL;
	}

	char netbuf[BUFSIZ];
	if ((fgets (netbuf, sizeof (netbuf), stream)) == NULL)
	    break;

	if (checkValidHTTPHeader ((unsigned char*) netbuf, sizeof (netbuf)) != 0) {
	    cur_ptr->netio_error = NET_ERR_HTTP_PROTO_ERR;
	    fclose (stream);
	    return NULL;
	}

	if (strncasecmp (netbuf, "Transfer-Encoding", strlen ("Transfer-Encoding")) == 0) {
	    // Chunked transfer encoding. HTTP/1.1 extension.
	    // http://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html#sec3.6.1
	    // This is not supported, because the contributed dechunk function
	    // does not work with binary data and fails valgrind tests.
	    // Disabled as of 1.5.7.
#ifdef USE_UNSUPPORTED_AND_BROKEN_CODE
#warning ===The function decodedechunked() is not safe for binary data. Since you specifically requested it to be compiled in you probably know better what you are doing than me. Do not report bugs for this code.===
	    if (strstr (netbuf, "chunked") != NULL)
		chunked = true;
#else
	    cur_ptr->netio_error = NET_ERR_CHUNKED;
	    cur_ptr->problem = true;
	    fclose (stream);
	    return NULL;
#endif
	}
	// Get last modified date. This is only relevant on HTTP 200.
	if ((strncasecmp (netbuf, "Last-Modified", strlen ("Last-Modified")) == 0) && (cur_ptr->lasthttpstatus == 200)) {
	    char* tmpstring = strdup (netbuf);
	    char* freeme = tmpstring;
	    strsep (&tmpstring, " ");
	    if (tmpstring == NULL)
		free (freeme);
	    else {
		free (cur_ptr->lastmodified);
		cur_ptr->lastmodified = strdup (tmpstring);
		if (cur_ptr->lastmodified[strlen (cur_ptr->lastmodified) - 1] == '\n')
		    cur_ptr->lastmodified[strlen (cur_ptr->lastmodified) - 1] = '\0';
		if (cur_ptr->lastmodified[strlen (cur_ptr->lastmodified) - 1] == '\r')
		    cur_ptr->lastmodified[strlen (cur_ptr->lastmodified) - 1] = '\0';
		free (freeme);
	    }
	}
	if (strncasecmp (netbuf, "Content-Encoding", strlen ("Content-Encoding")) == 0) {
	    if (strstr (netbuf, "gzip"))
		inflate = true;
	} else if (strncasecmp (netbuf, "Content-Type", strlen ("Content-Type")) == 0) {
	    char* tmpstring = strdup (netbuf);
	    char* freeme = tmpstring;
	    strsep (&tmpstring, " ");
	    if (tmpstring) {
		char* psemicolon = strstr (tmpstring, ";");
		if (psemicolon)
		    *psemicolon = '\0';
		free (cur_ptr->content_type);
		cur_ptr->content_type = strdup (tmpstring);
		if (cur_ptr->content_type[strlen (cur_ptr->content_type) - 1] == '\n')
		    cur_ptr->content_type[strlen (cur_ptr->content_type) - 1] = '\0';
		if (cur_ptr->content_type[strlen (cur_ptr->content_type) - 1] == '\r')
		    cur_ptr->content_type[strlen (cur_ptr->content_type) - 1] = '\0';
	    }
	    free (freeme);
	}
	// HTTP authentication
	//
	// RFC 2617
	if ((strncasecmp (netbuf, "WWW-Authenticate", strlen ("WWW-Authenticate")) == 0) && (cur_ptr->lasthttpstatus == 401)) {
	    if (authfailed) {
		// Don't repeat authrequest if it already failed before!
		cur_ptr->netio_error = NET_ERR_AUTH_FAILED;
		fclose (stream);
		return NULL;
	    }
	    // Remove trailing \r\n from line.
	    if (netbuf[strlen (netbuf) - 1] == '\n')
		netbuf[strlen (netbuf) - 1] = '\0';
	    if (netbuf[strlen (netbuf) - 1] == '\r')
		netbuf[strlen (netbuf) - 1] = '\0';

	    authfailed = true;

	    // Make a copy of the WWW-Authenticate header. We use it to
	    // reconstruct a new auth reply on every loop.
	    free (cur_ptr->servauth);

	    cur_ptr->servauth = strdup (netbuf);

	    // Load authinfo into cur_ptr->authinfo.
	    switch (NetSupportAuth (cur_ptr, authdata, url, netbuf)) {
		case 1:
		    cur_ptr->netio_error = NET_ERR_AUTH_NO_AUTHINFO;
		    fclose (stream);
		    return NULL;
		    break;
		case 2:
		    cur_ptr->netio_error = NET_ERR_AUTH_GEN_AUTH_ERR;
		    fclose (stream);
		    return NULL;
		    break;
		case -1:
		    cur_ptr->netio_error = NET_ERR_AUTH_UNSUPPORTED;
		    fclose (stream);
		    return NULL;
		    break;
		default:
		    break;
	    }

	    // Close current connection and reconnect to server.
	    fclose (stream);
	    if ((NetConnect (my_socket, host, cur_ptr, httpsproto, suppressoutput)) != 0) {
		return NULL;
	    }
	    // Now that we have an authinfo, repeat the current request.
	    goto tryagain;
	}
	// This seems to be optional and probably not worth the effort since we
	// don't issue a lot of consecutive requests.
	//if ((strncasecmp (netbuf, "Authentication-Info", 19) == 0) || (cur_ptr->lasthttpstatus == 200)) {}

	// HTTP RFC 2616, Section 19.3 Tolerant Applications.
	// Accept CRLF and LF line ends in the header field.
	if ((strcmp (netbuf, "\r\n") == 0) || (strcmp (netbuf, "\n") == 0))
	    break;
    }

    // If the redirectloop was run newhost and newurl were allocated.
    // We need to free them here.
    // But _after_ the authentication code since it needs these values!
    if (redirectcount > 0 && authdata) {
	free (host);
	free (url);
    }
    //---------------------
    // End of HTTP header
    //---------------------

    // Init pointer so strncat works.
    // Workaround class hack.
    char* body = malloc (1);
    body[0] = '\0';
    unsigned length = 0;

    // Read stream until EOF and return it to parent.
    while (!feof (stream)) {
	if (NetPoll (cur_ptr, my_socket, NET_READ) == -1) {
	    fclose (stream);
	    return NULL;
	}
	// Since we handle binary data if we read compressed input we
	// need to use fread instead of fgets after reading the header.
	char netbuf[BUFSIZ];
	size_t retval = fread (netbuf, 1, sizeof (netbuf), stream);
	if (retval == 0)
	    break;
	body = realloc (body, length + retval);
	memcpy (body + length, netbuf, retval);
	length += retval;
	if (retval != sizeof (netbuf))
	    break;
    }
    body = realloc (body, length + 1);
    body[length] = '\0';

    cur_ptr->content_length = length;

    // Close connection.
    fclose (stream);

#ifdef USE_UNSUPPORTED_AND_BROKEN_CODE
    if (chunked) {
	if (decodechunked (body, &length) == NULL) {
	    free (body);
	    cur_ptr->netio_error = NET_ERR_HTTP_PROTO_ERR;
	    return NULL;
	}
    }
#endif

    // If inflate==true we need to decompress the content..
    if (inflate) {
	char* inflatedbody;
	// gzipinflate
	int gzipstatus = jg_gzip_uncompress (body, length, (void **) &inflatedbody, &cur_ptr->content_length);
	if (gzipstatus) {
	    free (body);
	    syslog (LOG_ERR, _("zlib exited with code: %d"), gzipstatus);
	    cur_ptr->netio_error = NET_ERR_GZIP_ERR;
	    return NULL;
	}
	// Copy uncompressed data back to body.
	free (body);
	body = inflatedbody;
    }

    return body;
}

// Returns allocated string with body of webserver reply.
// Various status info put into struct feed * cur_ptr.
// Set suppressoutput=1 to disable ncurses calls.
char* DownloadFeed (char* url, struct feed* cur_ptr, bool suppressoutput)
{
    if (checkValidHTTPURL ((unsigned char*) url) != 0) {
	cur_ptr->problem = true;
	cur_ptr->netio_error = NET_ERR_HTTP_PROTO_ERR;
	return NULL;
    }
    // strstr will match _any_ substring. Not good, use strncasecmp with length 5!
    bool httpsproto = (strncasecmp (url, "https", strlen ("https")) == 0);

    strsep (&url, "/");
    strsep (&url, "/");
    char* tmphost = url;
    strsep (&url, "/");
    bool url_fixup = false;
    if (url == NULL) {
	// Assume "/" is input is exhausted.
	url = strdup ("/");
	url_fixup = true;
    }
    // If tmphost contains an '@', extract username and pwd.
    char* authdata = NULL;
    if (strchr (tmphost, '@') != NULL) {
	char* tmpstr = tmphost;
	strsep (&tmphost, "@");
	authdata = strdup (tmpstr);
    }

    char* host = strdup (tmphost);

    // netio() might change pointer of host to something else if redirect
    // loop is executed. Make a copy so we can correctly free everything.
    char* hostbase = host;
    // Only run if url was != NULL above.
    if (!url_fixup) {
	--url;
	url[0] = '/';
	if (url[strlen (url) - 1] == '\n') {
	    url[strlen (url) - 1] = '\0';
	}
    }

    int my_socket = 0;
    if (NetConnect (&my_socket, host, cur_ptr, httpsproto, suppressoutput)) {
	free (hostbase);
	free (authdata);
	if (url_fixup)
	    free (url);
	cur_ptr->problem = true;
	return NULL;
    }
    char* returndata = NetIO (&my_socket, host, url, cur_ptr, authdata, httpsproto, suppressoutput);
    if (!returndata && cur_ptr->netio_error != NET_ERR_OK)
	cur_ptr->problem = true;

    // url will be freed in the calling function.
    free (hostbase);	       // This is *host.
    free (authdata);
    if (url_fixup)
	free (url);

    return returndata;
}
