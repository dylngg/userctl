#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vector.h"

int create_vector(Vector *vec, size_t item_size) {
    assert(vec);

    vec->capacity = 16;
    vec->count = 0;
    vec->item_size = item_size;

    vec->data = malloc(item_size * vec->capacity);
    if (!vec->data) return -1;
    return 0;
}

void destroy_vector(Vector *vec) {
    assert(vec);
    free(vec->data);
}

int ensure_vector_capacity(Vector *vec, size_t capacity) {
    assert(vec);

    if (vec->capacity - vec->count > capacity) return 0;

    vec->data = realloc(vec->data, vec->item_size * capacity);
    if (!vec->data) return -1;

    vec->capacity = capacity;
    return 0;
}

int append_vector_item(Vector *vec, const void *item) {
    assert(vec);
    assert(item);

    if (vec->capacity - vec->count < 1) {
        if (ensure_vector_capacity(vec, vec->capacity * 2) < 0) return -1;
    }

    memcpy(vec->data + vec->count * vec->item_size, item, vec->item_size);
    vec->count++;
    return 0;
}

void *get_vector_item(Vector *vec, size_t index) {
    assert(vec);
    assert(index < vec->count);
    return vec->data + index * vec->item_size;
}

size_t get_vector_count(Vector *vec) {
    assert(vec);
    return vec->count;
}

void *find_vector_item(Vector *vec, finder_t finder, ...) {
    assert(vec);
    size_t count = get_vector_count(vec);
    void *item = NULL, *tmp_item = NULL;
    va_list args;

    va_start(args, finder);

    for (size_t i = 0; i < count; i++) {
        tmp_item = get_vector_item(vec, i);
        if (finder(tmp_item, args)) {
            item = tmp_item;
            break;
        }
    }

    va_end(args);
    return item;
}

int convert_vector_to_array(Vector *vec, void **array, size_t *size) {
    assert(vec);
    assert(array);

    size_t tmp_count = get_vector_count(vec);
    size_t item_size = vec->item_size;
    char *tmp_array = malloc(sizeof *tmp_array * item_size * (tmp_count + 1));  // + NULL
    if (!tmp_array) return -1;
    memset(tmp_array + item_size * tmp_count, 0, item_size);

    for (size_t i = 0; i < tmp_count; i++) {
        void *item = get_vector_item(vec, i);
        memcpy(tmp_array + i * item_size, item, item_size);
    }
    *array = (void*) tmp_array;
    *size = tmp_count * item_size;
    return 0;
}
