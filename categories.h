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

#pragma once
#include "main.h"

void FeedCategoryAdd (struct feed* cur_ptr, const char* categoryname);
void FeedCategoryDelete (struct feed* cur_ptr, const char* categoryname);
bool FeedCategoryExists (const struct feed* cur_ptr, const char* categoryname);
char* GetCategoryList (const struct feed* feed);
void ResetFilters (char** filters);
