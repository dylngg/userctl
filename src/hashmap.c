#define _GNU_SOURCE // (search.h)
#include <assert.h>
#include <errno.h>
#include <search.h>
#include <stdlib.h>
#include <string.h>

#include "hashmap.h"

int create_hashmap(HashMap* map, size_t max_size)
{
    assert(map);
    int r = 0;

    memset(&map->data, 0, sizeof map->data);
    r = hcreate_r(max_size, &map->data);
    if (!r)
        return -1;

    r = create_vector(&map->keys, sizeof(char**));
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
        free((char*)get_hashmap_entry(map, *key));
        free(*key);
    }
    iter_vector_end(&map->keys);

    destroy_vector(&map->keys);
    hdestroy_r(&map->data);
}

int add_hashmap_entry(HashMap* map, char* key, char* value)
{
    assert(map);
    int r;
    char* tmp;
    ENTRY* entry_ptr = NULL;
    ENTRY entry = { key, NULL };

    // hsearch never replaces entries so we must check if the key is in there
    r = hsearch_r(entry, FIND, &entry_ptr, &map->data);
    if (!r && errno == ESRCH) {
        // Not found; insert
        entry.key = strdup(key);
        entry.data = strdup(value);
        if (!entry.key || !entry.data)
            return -1;

        r = hsearch_r(entry, ENTER, &entry_ptr, &map->data);
        if (!r)
            return -1;

        append_vector_item(&map->keys, &entry.key);
    } else {
        // found; replace
        tmp = strdup(value);
        if (!entry_ptr->data)
            return -1;

        free(entry_ptr->data);
        entry_ptr->data = tmp;
    }
    return 0;
}

const char*
get_hashmap_entry(HashMap* map, char* key)
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

void iter_hashmap(HashMap* map, const char** key, const char** value)
{
    assert(map);
    char** map_key;

    map_key = iter_vector(&map->keys);
    if (!map_key) {
        *key = NULL;
        *value = NULL;
    }
    *key = *map_key;
    *value = get_hashmap_entry(map, *map_key);
}

void iter_hashmap_end(HashMap* map)
{
    assert(map);
    iter_vector_end(&map->keys);
    ;
}
