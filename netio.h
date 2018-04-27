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

#ifndef NETIO_H
#define NETIO_H

struct feed;

char * DownloadFeed (char * url, struct feed * cur_ptr, int suppressoutput);

typedef enum {
	NET_ERR_OK,
	/* Init errors */
	NET_ERR_URL_INVALID,
	/* Connect errors */
	NET_ERR_SOCK_ERR,
	NET_ERR_HOST_NOT_FOUND,
	NET_ERR_CONN_REFUSED,
	NET_ERR_CONN_FAILED,
	NET_ERR_TIMEOUT,
	NET_ERR_UNKNOWN,
	/* Transfer errors */
	NET_ERR_REDIRECT_COUNT_ERR,
	NET_ERR_REDIRECT_ERR,
	NET_ERR_HTTP_410,
	NET_ERR_HTTP_404,
	NET_ERR_HTTP_NON_200,
	NET_ERR_HTTP_PROTO_ERR,
	NET_ERR_AUTH_FAILED,
	NET_ERR_AUTH_NO_AUTHINFO,
	NET_ERR_AUTH_GEN_AUTH_ERR,
	NET_ERR_AUTH_UNSUPPORTED,
	NET_ERR_GZIP_ERR,
	NET_ERR_CHUNKED
} netio_error_type;

#endif
