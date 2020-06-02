#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "vector.h"

int create_vector(Vector* vec, size_t item_size)
{
    assert(vec);

    vec->capacity = 16;
    vec->count = 0;
    vec->iter_count = 0;
    vec->item_size = item_size;

    vec->data = malloc(item_size * vec->capacity);
    if (!vec->data)
        return -1;

    return 0;
}

void destroy_vector(Vector* vec)
{
    assert(vec);
    free(vec->data);
}

int ensure_vector_capacity(Vector* vec, size_t capacity)
{
    assert(vec);

    // Always keep one element at the end for the pretend_vector func
    capacity++;
    if (vec->capacity - vec->count > capacity)
        return 0;

    vec->data = realloc(vec->data, vec->item_size * capacity);
    if (!vec->data)
        return -1;

    vec->capacity = capacity;
    return 0;
}

int append_vector_item(Vector* vec, const void* item)
{
    assert(vec);
    assert(item);

    // Always keep one element at the end for the pretend_vector func
    if (vec->capacity - vec->count < 2)
        if (ensure_vector_capacity(vec, vec->capacity * 2) < 0)
            return -1;

    memcpy(vec->data + vec->count * vec->item_size, item, vec->item_size);
    vec->count++;
    return 0;
}

void* get_vector_item(Vector* vec, size_t index)
{
    assert(vec);
    assert(index < vec->count);
    return vec->data + index * vec->item_size;
}

size_t
get_vector_count(Vector* vec)
{
    assert(vec);
    return vec->count;
}

void* find_vector_item(Vector* vec, finder_t finder, ...)
{
    assert(vec);
    va_list args;
    va_start(args, finder);

    void* item = NULL;
    void* tmp_item = NULL;
    while ((tmp_item = iter_vector(vec))) {
        if (finder(tmp_item, args)) {
            item = tmp_item;
            break;
        }
    }
    iter_vector_end(vec);

    va_end(args);
    return item;
}

void* iter_vector(Vector* vec)
{
    assert(vec);

    if (vec->iter_count >= vec->count) {
        iter_vector_end(vec);
        return NULL;
    }
    void* item = vec->data + vec->iter_count * vec->item_size;
    vec->iter_count++;
    return item;
}

void iter_vector_end(Vector* vec)
{
    assert(vec);
    vec->iter_count = 0;
}

void* pretend_vector_is_array(Vector* vec)
{
    assert(vec);
    // We always ensure we have at least one empty place at the end
    memset(vec->data + vec->count * vec->item_size, 0, vec->item_size);
    return vec->data;
}
