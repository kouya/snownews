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

#ifndef CATEGORIES_H
#define CATEGORIES_H

void FeedCategoryAdd (struct feed * cur_ptr, char * categoryname);
void FeedCategoryDelete (struct feed * cur_ptr, char * categoryname);
int FeedCategoryExists (struct feed * cur_ptr, char * categoryname);
char *GetCategoryList (struct feed * feed);
void ResetFilters (char **filters);

/* A feeds categories */
struct feedcategories {
	char * name;						/* Category name */
	struct feedcategories * next_ptr;
};

/* Global list of all defined categories, their refcounts and color labels. */
struct categories {
	char * name;						/* Category name */
	int refcount;						/* Number of feeds using this category. */
	int label;							/* Color label of this category. */
	int labelbold;
	struct categories * next_ptr;
};

#endif
