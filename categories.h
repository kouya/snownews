/* Snownews - A lightweight console RSS newsreader
 * $Id: categories.h 92 2004-10-06 10:29:54Z kiza $
 * 
 * Copyright 2003-2004 Oliver Feiler <kiza@kcore.de>
 * http://kiza.kcore.de/software/snownews/
 *
 * categories.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
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
