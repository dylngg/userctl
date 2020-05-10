// SPDX-License-Identifier: GPL-3.0
#ifndef UTILS_H
#define UTILS_H

#include <sys/types.h>
#include <stdbool.h>

/*
 * Quotes last words into stderr and dies (with exit code of 1).
 */
void die(const char* quote);

/*
 * Quotes last words and errno error into stderr and dies (with exit code of
 * 1).
 */
void errno_die(const char* quote);

/*
 * Converts the username string to a uid, if it wasn't already. If the user
 * doesn't exist or if there is a error, returns -1. If errno is not zero a
 * error occurred.
 */
int to_uid(const char* username, uid_t* uid);

/*
 * Converts the uid to a username string. If the user doesn't exist or if
 * there is a error, returns -1. If errno is not zero a error occurred.
 *
 * Note: It is the callers responsibility to ensure that future getpw* calls
 * do not overwrite the returned username.
 */
int to_username(uid_t uid, const char** username);

/*
 * Converts the groupname string to a gid, if it wasn't already. If the group
 * doesn't exist, returns -1. If errno is not zero a error occurred.
 */
int to_gid(const char* groupname, gid_t* gid);

/*
 * Converts the gid to a groupname string. If the group doesn't exist or if
 * there is a error, returns -1. If errno is not zero a error occurred.
 *
 * Note: It is the callers responsibility to ensure that future getgr* calls
 * do not overwrite the returned groupname.
 */
int to_groupname(gid_t gid, const char** groupname);

/*
 * Moves the string pointer to after the leading whitespace and adds a NULL
 * terminator at the beginning of the trailing whitespace. Special care should
 * be taken to _not_ free the pointer returned (the pointer may have changed).
 */
void trim_whitespace(char** string);

/*
 * Returns whether the string ends in the given extension.
 */
bool has_ext(const char* restrict string, const char* restrict ext);

/*
 * Returns a malloced filepath for the given filename at the given directory.
 */
const char* get_filepath(const char* restrict dir, const char* restrict filename);

/*
 * Exits if there is a malloc issue.
 */
void malloc_error_exit();

#endif // UTILS_H
