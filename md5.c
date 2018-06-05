// This file is part of Snownews - A lightweight console RSS newsreader
//
// Copyright (c) 2018 Mike Sharov <msharov@users.sourceforge.net>
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

#include "md5.h"

static inline uint32_t remove_bits (uint32_t v, uint8_t f, uint8_t n)
    { return ((v>>(f+n))<<f)|(v&((1<<f)-1)); }
static inline uint32_t Rol (uint32_t v, uint32_t n)
    { return (v<<n)|(v>>(32-n)); }

static void hash_md5_block (struct HashMD5* h)
{
    static const uint8_t r[16] = { 7, 12, 17, 22, 5,  9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21 };
    //{{{ K table
    static const uint32_t K[64] = {
	0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
	0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
	0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
	0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
	0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
	0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
	0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
	0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
	0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
	0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
	0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
	0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
	0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
	0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
	0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
	0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
    };
    //}}}
    uint32_t a = h->hash[0], b = h->hash[1], c = h->hash[2], d = h->hash[3], f, g;
    for (uint32_t i = 0; i < 64; ++i) {
	if (i < 16) {
	    f = d ^ (b & (c ^ d));
	    g = i;
	} else if (i < 32) {
	    f = c ^ (d & (b ^ c));
	    g = (5*i + 1) % 16;
	} else if (i < 48) {
	    f = b ^ c ^ d;
	    g = (3*i + 5) % 16;
	} else {
	    f = c ^ (b | ~d);
	    g = (7*i) % 16;
	}
	f = Rol (a + f + K[i] + h->words[g], r[remove_bits(i,2,2)]);
	g = d; d = c; c = b; b += f; a = g;
    }
    h->hash[0] += a; h->hash[1] += b; h->hash[2] += c; h->hash[3] += d;
}

void hash_md5_init (struct HashMD5* h)
{
    memset (h, 0, sizeof(*h));
    static const uint32_t hash_md5_initial_value [HASH_SIZE_MD5_WORDS]
	= { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };
    memcpy (h->hash, hash_md5_initial_value, sizeof(h->hash));
}

void hash_md5_data (struct HashMD5* h, const void* d, size_t n)
{
    while (n) {
	uint32_t blockOffset = h->offset % HASH_BLOCK_SIZE_MD5;
	uint32_t copied = HASH_BLOCK_SIZE_MD5 - blockOffset;
	if (copied > n)
	    copied = n;
	memcpy (h->bytes+blockOffset, d, copied);
	d = (const char*)d + copied;
	n -= copied;
	h->offset += copied;
	blockOffset = (blockOffset + copied) % HASH_BLOCK_SIZE_MD5;
	if (!blockOffset)
	    hash_md5_block (h);
    }
}

void hash_md5_finish (struct HashMD5* h)
{
    uint64_t size = h->offset;
    uint8_t pad = 0x80;	// End message with one bit
    do {
	hash_md5_data (h, &pad, sizeof(pad));
	pad = 0;	// Pad with zeroes until there is enough space for offset at the end
    } while (h->offset % HASH_BLOCK_SIZE_MD5 != HASH_BLOCK_SIZE_MD5-sizeof(h->offset));
    h->quads[7] = size*8;	// Write size in bits at end of block
    hash_md5_block (h);
}

void hash_md5_to_text (const struct HashMD5* h, char* text)
{
    for (unsigned i = 0; i < HASH_SIZE_MD5_WORDS; ++i)
	for (unsigned j = 0; j < 4; ++j)
	    sprintf (&text[i*8+j*2], "%02hhx", (uint8_t)(h->hash[i]>>(j*8)));
}
