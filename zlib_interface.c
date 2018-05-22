// This file is part of Snownews - A lightweight console RSS newsreader
//
// Copyright (c) 2004 Rene Puls <rpuls@gmx.net>
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

#include "zlib_interface.h"
#include <zlib.h>

struct gzip_header {
	unsigned char magic[2];
	unsigned char method;
	unsigned char flags;
	unsigned char mtime[4];
	unsigned char xfl;
	unsigned char os;
};

struct gzip_footer {
	unsigned char crc32[4];
	unsigned char size[4];
};

static int jg_zlib_uncompress(void const *in_buf, unsigned in_size, void **out_buf_ptr, unsigned* out_size, bool gzip)
{
	// Prepare the stream structure.
	z_stream stream = {};

	stream.next_in = (void*) in_buf;
	stream.avail_in = in_size;

	unsigned char tmp_buf [BUFSIZ];
	stream.next_out = tmp_buf;
	stream.avail_out = sizeof tmp_buf;

	if (out_size)
		*out_size = 0;

	int result = inflateInit2 (&stream, gzip ? MAX_WBITS + 32 : -MAX_WBITS);
	if (result)
		return JG_ZLIB_ERROR_OLDVERSION;

	char *out_buf = NULL;
	unsigned out_buf_bytes = 0;
	do {
		// Should be Z_FINISH?
		result = inflate(&stream, Z_NO_FLUSH);
		switch (result) {
			case Z_BUF_ERROR:
				if (stream.avail_in == 0)
					goto DONE; // zlib bug
				// fallthrough
			case Z_ERRNO:
			case Z_NEED_DICT:
			case Z_MEM_ERROR:
			case Z_DATA_ERROR:
			case Z_VERSION_ERROR:
				inflateEnd(&stream);
				free(out_buf);
				return JG_ZLIB_ERROR_UNCOMPRESS;
		}
		if (stream.avail_out < sizeof(tmp_buf)) {
			// Add the new uncompressed data to our output buffer.
			unsigned new_bytes = sizeof(tmp_buf) - stream.avail_out;
			out_buf = realloc(out_buf, out_buf_bytes + new_bytes);
			memcpy(out_buf + out_buf_bytes, tmp_buf, new_bytes);
			out_buf_bytes += new_bytes;
			stream.next_out = tmp_buf;
			stream.avail_out = sizeof(tmp_buf);
		} else {
			// For some reason, inflate() didn't write out a single byte.
			inflateEnd(&stream);
			free(out_buf);
			return JG_ZLIB_ERROR_NODATA;
		}
	} while (result != Z_STREAM_END);
DONE:
	inflateEnd (&stream);

	// Null-terminate the output buffer so it can be handled like a string.
	out_buf = realloc (out_buf, out_buf_bytes + 1);
	out_buf[out_buf_bytes] = 0;

	// The returned size does NOT include the additionall null byte!
	if (out_size)
		*out_size = out_buf_bytes;
	*out_buf_ptr = out_buf;
	return 0;
}

// Decompressed gzip,deflate compressed data. This is what the webservers usually send.
int jg_gzip_uncompress (const void* in_buf, unsigned in_size, void** out_buf_ptr, unsigned* out_size)
{
	if (out_size)
		*out_size = 0;

	const struct gzip_header* header = in_buf;
	if ((header->magic[0] != 0x1F) || (header->magic[1] != 0x8B))
		return JG_ZLIB_ERROR_BAD_MAGIC;
	if (header->method != 8)
		return JG_ZLIB_ERROR_BAD_METHOD;
	if (header->flags != 0 && header->flags != 8)
		return JG_ZLIB_ERROR_BAD_FLAGS;

	unsigned offset = sizeof(*header);
	if (header->flags & 8)	// skip the file name
		while (offset < in_size)
			if (((char *)in_buf)[offset++] == 0)
				break;
	return jg_zlib_uncompress ((char*)in_buf + offset, in_size - offset - 8, out_buf_ptr, out_size, 0);
}
