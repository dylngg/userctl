#ifndef UTILS_H
#define UTILS_H

#include <sys/types.h>

/*
 * Quotes last words into stderr and dies (with exit code of 1).
 */
void die(char* quote);

/*
 * Converts the username to a uid. If the user doesn't exist, returns -1.
 */
int to_uid(char* username, uid_t* uid);

/*
 * Converts the groupname to a gid. If the group doesn't exist, returns -1.
 */
int to_gid(char* groupname, gid_t* gid);

/*
 * Exits if there is a malloc issue.
 */
void malloc_error_exit();

#endif // UTILS_H
