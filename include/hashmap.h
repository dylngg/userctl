// SPDX-License-Identifier: GPL-3.0
#ifndef HASHMAP_H
#define HASHMAP_H
#define _GNU_SOURCE

#include "vector.h"
#include <search.h>

typedef struct HashMap {
    struct hsearch_data data;
    // Keep references to the entries
    Vector keys;
    size_t value_size;
} HashMap;

/*
 * Passes back a hashmap and returns a 0 if the creation was successful, or -1
 * is not. If a -1 is returned, the issue should be looked up via errno and
 * the parameters are untouched.
 */
int create_hashmap(HashMap* map, size_t value_size, size_t max_size);

/*
 * Destorys the given hashmap.
 */
void destroy_hashmap(HashMap* map);

/*
 * Adds a given entry to the hashmap, potentially replacing an existing entry.
 * If replaced, the value is freed. The value and key must be zero-terminated
 * strings of characters. Both values are duplcated when added to the hashmap.
 */
int add_hashmap_entry(HashMap* map, char* key, void* value);

/*
 * Gets an entry out of the given hashmap. If the key cannot be found, NULL is
 * returned. The returned entry is owned by the hashmap.
 */
void* get_hashmap_entry(HashMap* map, char* key);

/*
 * Returns the number of entrs in the hashmap.
 */
size_t get_hashmap_count(HashMap* map);

/*
 * Iterates over the hashmap, returning each entry within it. The hashmap owns
 * all the entries returned. The key and value are NULL for the last item.
 */
void iter_hashmap(HashMap* map, char** key, void** value);

/*
 * Iterates over the hashmap, returning each value within it. The hashmap owns
 * all the values returned. The value is NULL for the last item.
 */
void* iter_hashmap_values(HashMap* map);

/*
 * Resets the hashmap iterator.
 */
void iter_hashmap_end(HashMap* map);

#endif // HASHMAP_H
