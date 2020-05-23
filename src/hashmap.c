#define _GNU_SOURCE // (search.h)
#include <assert.h>
#include <errno.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashmap.h"

int create_hashmap(HashMap* map, size_t value_size, size_t max_size)
{
    assert(map);
    map->value_size = value_size;

    memset(&map->data, 0, sizeof map->data);
    int r = hcreate_r(max_size, &map->data);
    if (!r)
        return -1;

    r = create_vector(&map->keys, sizeof(char*));
    if (r < 0)
        return -1;

    return 0;
}

void destroy_hashmap(HashMap* map)
{
    assert(map);

    char** key;
    while ((key = iter_vector(&map->keys))) {
        // Safe to modify things because hashmap owns things
        free(get_hashmap_entry(map, *key));
        free(*key);
    }
    iter_vector_end(&map->keys);

    destroy_vector(&map->keys);
    hdestroy_r(&map->data);
}

/*
 * Returns the entry as a allocated null terminated char array.
 */
char* _mangle_value(const void* value, size_t value_size)
{
    char* mangled = malloc(value_size + 1); // value + '\0'
    memcpy(mangled, value, value_size);
    mangled[value_size] = '\0';
    return mangled;
}

int add_hashmap_entry(HashMap* map, char* key, void* value)
{
    assert(map);
    ENTRY* entry_ptr = NULL;
    ENTRY entry = { key, NULL };

    // hsearch never replaces entries so we must check if the key is in there
    int r = hsearch_r(entry, FIND, &entry_ptr, &map->data);
    if (!r) {
        if (errno != ESRCH)
            return -1;

        // Not found; insert
        entry.key = strdup(key);
        entry.data = _mangle_value(value, map->value_size);
        if (!entry.key || !entry.data)
            return -1;

        r = hsearch_r(entry, ENTER, &entry_ptr, &map->data);
        if (!r)
            return -1;

        append_vector_item(&map->keys, &entry.key);
    } else {
        // found; replace
        char* mangled = _mangle_value(value, map->value_size);
        if (!mangled)
            return -1;

        free(entry_ptr->data);
        entry_ptr->data = mangled;
    }
    return 0;
}

void* get_hashmap_entry(HashMap* map, char* key)
{
    assert(map);
    ENTRY* entry_ptr = NULL;
    ENTRY entry = { key, NULL };

    hsearch_r(entry, FIND, &entry_ptr, &map->data);
    if (!entry_ptr)
        return NULL;
    return entry_ptr->data;
}

size_t
get_hashmap_count(HashMap* map)
{
    assert(map);
    return get_vector_count(&map->keys);
}

void iter_hashmap(HashMap* map, char** key, void** value)
{
    assert(map);

    char** map_key = iter_vector(&map->keys);
    if (!map_key) {
        if (key)
            *key = NULL;
        if (value)
            *value = NULL;
    } else {
        if (key)
            *key = *map_key;
        if (value)
            *value = get_hashmap_entry(map, *map_key);
    }
}

void* iter_hashmap_values(HashMap* map)
{
    void* value = NULL;
    iter_hashmap(map, NULL, &value);
    return value;
}

void iter_hashmap_end(HashMap* map)
{
    assert(map);
    iter_vector_end(&map->keys);
}
