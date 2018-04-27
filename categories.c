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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "categories.h"

struct categories *first_category = NULL;

// Compare global category list with string categoryname and return 1 if a
// matching category was found.
int CategoryListItemExists (char * categoryname) {
	struct categories *category;
	
	for (category = first_category; category != NULL; category = category->next_ptr) {
		if (strcasecmp (category->name, categoryname) == 0) {
			// Matching category found.
			return 1;
		}
	}

	return 0;
}


// The following functions add items to/delete items from the global category list.
void CategoryListAddItem (char * categoryname) {
	int inserted = 0;
	struct categories *category;
	struct categories *before_ptr = NULL;
	struct categories *new_ptr;
		
	if (CategoryListItemExists (categoryname)) {
		// Only increase refcount
		for (category = first_category; category != NULL; category = category->next_ptr) {
			if (strcasecmp (category->name, categoryname) == 0)
				category->refcount++;
		}
	} else {
		// Otherwise add item to structure
		category = malloc (sizeof(struct categories));
		category->name = strdup (categoryname);
		category->next_ptr = NULL;
		
		if (first_category == NULL) {
			// Contains no elements
			category->next_ptr = first_category;
			first_category = category;
		} else {
			for (new_ptr = first_category; new_ptr != NULL; new_ptr = new_ptr->next_ptr) {
				if (strcasecmp (category->name, new_ptr->name) > 0) {
					// New category still below current one. Move on.
					before_ptr = new_ptr;
				} else {
					if (before_ptr == NULL) {
						// Insert before first_ptr
						category->next_ptr = first_category;
						first_category = category;
					} else {
						// Insert after first_ptr
						before_ptr->next_ptr = category;
						category->next_ptr = new_ptr;
					}
					inserted = 1;
					// Done with this loop.
					break;
				}
			}
			if (!inserted) {
				// No match from above. Insert new item at the end of the list.
				before_ptr->next_ptr = category;
			}
		}
		
		// Set refcount to 1 for newly added category.
		category->refcount = 1;
	}
}

// Delete item from category list. Decrease refcount for each deletion.
// If refcount hits 0, remove from list.
void CategoryListDeleteItem (char * categoryname) {
	struct categories *category;
	struct categories *before_ptr = NULL;	// gcc complains ptr uninit with -O2
	
	for (category = first_category; category != NULL; category = category->next_ptr) {
		if (strcasecmp (category->name, categoryname) == 0) {
			// Check refcount
			if (category->refcount > 1) {
				// Still at least one feed defining this category
				category->refcount--;
				break;
			} else {
				// Last feed using this category removed, kill off the structure
				if (category == first_category) {
					first_category = first_category->next_ptr;
					free (category->name);
					free (category);
					break;
				} else {
					before_ptr->next_ptr = category->next_ptr;
					free (category->name);
					free (category);
					break;
				}
			}
		}
		before_ptr = category;
	}
}

// The following two functions add items to/delete items from a feeds category list.
void FeedCategoryAdd (struct feed * cur_ptr, char * categoryname) {
	int inserted = 0;
	struct feedcategories *category;
	struct feedcategories *before_ptr = NULL;
	struct feedcategories *new_ptr;
	
	category = malloc (sizeof(struct feedcategories));
	category->name = strdup (categoryname);
	category->next_ptr = NULL;
	
	
	if (cur_ptr->feedcategories == NULL) {
		// Contains no elements
		category->next_ptr = cur_ptr->feedcategories;
		cur_ptr->feedcategories = category;
	} else {
		for (new_ptr = cur_ptr->feedcategories; new_ptr != NULL; new_ptr = new_ptr->next_ptr) {
			if (strcasecmp (category->name, new_ptr->name) > 0) {
				// New category still below current one. Move on.
				before_ptr = new_ptr;
			} else {
				if (before_ptr == NULL) {
					category->next_ptr = cur_ptr->feedcategories;
					cur_ptr->feedcategories = category;
				} else {
					// The new category is now > current one. Insert it here.
					before_ptr->next_ptr = category;
					category->next_ptr = new_ptr;
				}
				inserted = 1;
				// Done with this loop.
				break;
			}
		}
		if (!inserted) {
			// No match from above. Insert new item at the end of the list.
			before_ptr->next_ptr = category;
		}
	}
	
	CategoryListAddItem (category->name);
}

void FeedCategoryDelete (struct feed * cur_ptr, char * categoryname) {
	struct feedcategories *category;
	struct feedcategories *before_ptr = NULL;
	char *tmpname;
	
	// categoryname == category->name which will be freed by this function!
	tmpname = strdup (categoryname);
	
	for (category = cur_ptr->feedcategories; category != NULL; category = category->next_ptr) {
		if (strcasecmp (category->name, categoryname) == 0) {
			if (category == cur_ptr->feedcategories) {
				cur_ptr->feedcategories = cur_ptr->feedcategories->next_ptr;
				free (category->name);
				free (category);
				break;
			} else {
				before_ptr->next_ptr = category->next_ptr;
				free (category->name);
				free (category);
				break;
			}
		}
		before_ptr = category;
	}
	
	// Decrease refcount in global category list.
	CategoryListDeleteItem (tmpname);
	
	free (tmpname);
}

// Compare a feeds category list with string categoryname and return 1 if a
// matching category was found.
int FeedCategoryExists (struct feed * cur_ptr, char * categoryname) {
	struct feedcategories *category;
	
	for (category = cur_ptr->feedcategories; category != NULL; category = category->next_ptr) {
		if (strcasecmp (category->name, categoryname) == 0) {
			// Matching category found.
			return 1;
		}
	}

	return 0;
}

// Set color label of category
void CategoryListColors (char * categoryname, char * label) {
	struct categories *category;
	
	for (category = first_category; category != NULL; category = category->next_ptr) {
		if (strcasecmp (category->name, categoryname) == 0) {
			// Matching category found.
			if (strlen(label) > 5) {
				category->label = atoi (label+5);
				if ((category->label == 12) ||
					(category->label == 17))
					category->labelbold = 1;
				else
					category->labelbold = 0;
			}
		}
	}
}

// Return color label number and bold status of given category
int CategoryGetLabel (char * categoryname, int type) {
	struct categories *category;
	
	for (category = first_category; category != NULL; category = category->next_ptr) {
		if (strcasecmp (category->name, categoryname) == 0) {
			// Matching category found.
			if (type == 0)
				return category->label;
			else
				return category->labelbold;
		}
	}
	return -1;
}

// Return a comma-separated list of categories defined for the provided feed,
// NULL if no category is defined for the feed.
//
// The caller must free the list.
char *GetCategoryList (struct feed *feed) {
	struct feedcategories *category;
	char *categories = NULL;
	int len;

	if (feed->feedcategories != NULL) {
		categories = malloc(1);
		memset (categories, 0, 1);
		len = 0;
		for (category = feed->feedcategories; category != NULL; category = category->next_ptr) {
			len += strlen(category->name)+4;
			categories = realloc (categories, len);
			strncat (categories, category->name, len);
			if (category->next_ptr != NULL)
				strcat (categories, ", ");
		}
	}

	return categories;
}

// Free and reset the filters list.
void ResetFilters (char **filters) {
	int i;

	if (!filters)
		return;

	for (i = 0; i <= 7; i++) {
		free (filters[i]);
		filters[i] = NULL;
	}
}

