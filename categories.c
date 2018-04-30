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

#include "main.h"
#include "categories.h"

// Compare global category list with string categoryname and return 1 if a
// matching category was found.
static bool CategoryListItemExists (const char* categoryname) {
	for (const struct categories* category = _settings.global_categories; category; category = category->next)
		if (strcasecmp (category->name, categoryname) == 0)
			return true;	// Matching category found.
	return false;
}

// The following functions add items to/delete items from the global category list.
static void CategoryListAddItem (const char* categoryname) {
	if (CategoryListItemExists (categoryname)) {
		// Only increase refcount
		for (struct categories* category = _settings.global_categories; category; category = category->next)
			if (strcasecmp (category->name, categoryname) == 0)
				category->refcount++;
	} else {
		// Otherwise add item to structure
		struct categories* category = malloc (sizeof(struct categories));
		category->name = strdup (categoryname);
		category->next = NULL;

		if (!_settings.global_categories) {	// Contains no elements
			category->next = _settings.global_categories;
			_settings.global_categories = category;
		} else {
			bool inserted = false;
			struct categories *before_ptr = NULL;
			for (struct categories* new_ptr = _settings.global_categories; new_ptr; new_ptr = new_ptr->next) {
				if (strcasecmp (category->name, new_ptr->name) > 0)
					before_ptr = new_ptr;	// New category still below current one. Move on.
				else {
					if (!before_ptr) {	// Insert before _settings.global_categories
						category->next = _settings.global_categories;
						_settings.global_categories = category;
					} else {		// Insert after category
						before_ptr->next = category;
						category->next = new_ptr;
					}
					inserted = true;
					break;	// Done with this loop.
				}
			}
			if (!inserted) // No match from above. Insert new item at the end of the list.
				before_ptr->next = category;
		}
		// Set refcount to 1 for newly added category.
		category->refcount = 1;
	}
}

// Delete item from category list. Decrease refcount for each deletion.
// If refcount hits 0, remove from list.
static void CategoryListDeleteItem (const char* categoryname) {
	for (struct categories *category = _settings.global_categories, *before_ptr = NULL; category; category = category->next) {
		if (strcasecmp (category->name, categoryname) == 0) {
			// Check refcount
			if (category->refcount > 1) {
				// Still at least one feed defining this category
				category->refcount--;
				break;
			} else {
				// Last feed using this category removed, kill off the structure
				if (category == _settings.global_categories) {
					_settings.global_categories = _settings.global_categories->next;
					free (category->name);
					free (category);
					break;
				} else {
					before_ptr->next = category->next;
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
void FeedCategoryAdd (struct feed* cur_ptr, const char* categoryname) {
	struct feedcategories* category = malloc (sizeof(struct feedcategories));
	category->name = strdup (categoryname);
	category->next = NULL;

	if (cur_ptr->feedcategories == NULL) {
		// Contains no elements
		category->next = cur_ptr->feedcategories;
		cur_ptr->feedcategories = category;
	} else {
		bool inserted = false;
		struct feedcategories *before_ptr = NULL;
		for (struct feedcategories* new_ptr = cur_ptr->feedcategories; new_ptr != NULL; new_ptr = new_ptr->next) {
			if (strcasecmp (category->name, new_ptr->name) > 0) {
				// New category still below current one. Move on.
				before_ptr = new_ptr;
			} else {
				if (before_ptr == NULL) {
					category->next = cur_ptr->feedcategories;
					cur_ptr->feedcategories = category;
				} else {
					// The new category is now > current one. Insert it here.
					before_ptr->next = category;
					category->next = new_ptr;
				}
				inserted = true;
				break;	// Done with this loop.
			}
		}
		if (!inserted)	// No match from above. Insert new item at the end of the list.
			before_ptr->next = category;
	}
	CategoryListAddItem (category->name);
}

void FeedCategoryDelete (struct feed* cur_ptr, const char* categoryname) {
	// categoryname == category->name which will be freed by this function!
	char* tmpname = strdup (categoryname);
	for (struct feedcategories *category = cur_ptr->feedcategories, *before_ptr = NULL; category; category = category->next) {
		if (strcasecmp (category->name, categoryname) == 0) {
			if (category == cur_ptr->feedcategories) {
				cur_ptr->feedcategories = cur_ptr->feedcategories->next;
				free (category->name);
				free (category);
				break;
			} else {
				before_ptr->next = category->next;
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

// Compare a feeds category list with string categoryname.
// Return true if a matching category was found.
bool FeedCategoryExists (const struct feed* cur_ptr, const char* categoryname) {
	for (const struct feedcategories* category = cur_ptr->feedcategories; category; category = category->next)
		if (strcasecmp (category->name, categoryname) == 0)
			return true;
	return false;
}

// Return a comma-separated list of categories defined for the provided feed,
// NULL if no category is defined for the feed.
//
// The caller must free the list.
char* GetCategoryList (const struct feed *feed) {
	if (!feed->feedcategories)
		return NULL;
	char* categories = malloc (1);
	categories[0] = 0;
	size_t len = 1;
	for (const struct feedcategories* category = feed->feedcategories; category; category = category->next) {
		len += strlen(", ")+strlen(category->name);
		categories = realloc (categories, len);
		if (categories[0])
			strcat (categories, ", ");
		strcat (categories, category->name);
	}
	return categories;
}

// Free and reset the filters list.
void ResetFilters (char **filters) {
	if (!filters)
		return;
	for (unsigned i = 0; i < 8; ++i) {
		free (filters[i]);
		filters[i] = NULL;
	}
}
