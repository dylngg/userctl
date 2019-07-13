#ifndef UTILS_H
#define UTILS_H

#include <sys/types.h>

/*
 * Converts the username to a uid. If the user doesn't exist, returns -1.
 */
int to_uid(char* username, uid_t* uid);

/*
 * Converts the groupname to a gid. If the group doesn't exist, returns -1.
 */
int to_gid(char* groupname, gid_t* gid);

/*
 * Given a list of groups, returns a malloced list of corresponding group
 * names. If there is a issue getting a group name, a -1 is returned and that
 * group is skipped and the corresponding ngroups is decreased.
 */
int groupnames(gid_t* groups, char** groupnames, int* ngroups);

/*
 * Exits if there is a malloc issue.
 */
void malloc_error_exit();

#endif // UTILS_H
