// This file is part of Snownews - A lightweight console RSS newsreader
//
// Copyright (c) 2004 René Puls <http://purl.org/net/kianga/>
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

enum JG_ZLIB_ERROR {
    JG_ZLIB_ERROR_OLDVERSION = -1,
    JG_ZLIB_ERROR_UNCOMPRESS = -2,
    JG_ZLIB_ERROR_NODATA = -3,
    JG_ZLIB_ERROR_BAD_MAGIC = -4,
    JG_ZLIB_ERROR_BAD_METHOD = -5,
    JG_ZLIB_ERROR_BAD_FLAGS = -6
};

int jg_gzip_uncompress (const void* in_buf, unsigned in_size, void** out_buf_ptr, unsigned* out_size);
