#ifndef UTILS_H
#define UTILS_H

#include <sys/types.h>

/*
 * Quotes last words into stderr and dies (with exit code of 1).
 */
void die(char* quote);

/*
 * Quotes last words and errno error into stderr and dies (with exit code of
 * 1).
 */
void errno_die(char* quote);

/*
 * Converts the username string to a uid, if it wasn't already. If the user
 * doesn't exist or if there is a error, returns -1. If errno is not zero a
 * error occurred.
 */
int to_uid(char* username, uid_t* uid);

/*
 * Converts the groupname string to a gid, if it wasn't already. If the group
 * doesn't exist, returns -1. If errno is not zero a error occurred.
 */
int to_gid(char* groupname, gid_t* gid);

/*
 * Moves the string pointer to after the leading whitespace and adds a NULL
 * terminator at the beginning of the trailing whitespace. Special care should
 * be taken to _not_ free the pointer returned (the pointer may have changed).
 */
void trim_whitespace(char** string);

/*
 * Exits if there is a malloc issue.
 */
void malloc_error_exit();

#endif // UTILS_H
