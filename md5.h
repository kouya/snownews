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

#pragma once
#include "config.h"

enum {
    HASH_SIZE_MD5 = 16,
    HASH_SIZE_MD5_WORDS = HASH_SIZE_MD5 / sizeof (uint32_t),
    HASH_BLOCK_SIZE_MD5 = 64
};

struct HashMD5 {
    uint32_t hash[HASH_SIZE_MD5_WORDS];
    uint64_t offset;
    union {
	uint8_t bytes[HASH_BLOCK_SIZE_MD5];
	uint32_t words[HASH_BLOCK_SIZE_MD5 / sizeof (uint32_t)];
	uint64_t quads[HASH_BLOCK_SIZE_MD5 / sizeof (uint64_t)];
    };
};

void hash_md5_init (struct HashMD5* h);
void hash_md5_data (struct HashMD5* h, const void* d, size_t n);
void hash_md5_finish (struct HashMD5* h);
void hash_md5_to_text (const struct HashMD5* h, char* text);
