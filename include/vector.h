// SPDX-License-Identifier: GPL-3.0
#ifndef VECTOR_H
#define VECTOR_H

#include <stdbool.h>

typedef struct Vector {
    char *data;
    size_t capacity;
    size_t count;
    size_t item_size;
} Vector;

typedef bool (*finder_t)(void *, va_list);

/*
 * Passes back a vector and returns a 0 if the creation was successful, or -1
 * is not. If a -1 is returned, the issue should be looked up via errno and
 * the parameters are untouched.
 */
int create_vector(Vector *vec, size_t item_size);

/*
 * Destorys the given vector.
 */
void destroy_vector(Vector *vec);

/*
 * Ensures that the vector has the given capacity. Returns -1 if there was an
 * error (and errno should be looked up), otherwise 0.
 */
int ensure_vector_capacity(Vector *vec, size_t capacity);

/*
 * Appends a item to the vector. Returns -1 if there was an error (and errno
 * should be looked up), otherwise 0.
 */
int append_vector_item(Vector *vec, void *item);

/*
 * Gets an item out of the given Vector.
 */
void *get_vector_item(Vector *vec, size_t index);

/*
 * Returns the number of items in the vector.
 */
size_t get_vector_count(Vector *vec);

/*
 * Finds a given item based on the finder function. If no such item exists,
 * NULL is returned.
 */
void *find_vector_item(Vector *vec, finder_t finder, ...);

/*
 * Creates a new single allocation array (ending in NULL) from the given
 * vector. Returns -1 if there was an error (and errno should be looked up),
 * otherwise 0.
 */
int convert_vector_to_array(Vector *vec, void **array, size_t *size);

#endif // VECTOR_H
