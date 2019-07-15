#include <ctype.h>
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "utils.h"

bool _alldigits(char* string);

void die(char* quote) {
    fputs(quote, stderr);
    exit(1);
}

/*
 * Converts the username string to a uid, if it wasn't already. If the user
 * doesn't exist, returns -1.
 */
int to_uid(char* username, uid_t* uid) {
    struct passwd* pw;
    if (_alldigits(username))
        pw = getpwuid((uid_t) strtoll(username, NULL, 10));
    else
        pw = getpwnam(username);
    if (!pw) return -1;
    *uid = pw->pw_uid;
    return 0;
}

/*
 * Converts the groupname string to a gid, if it wasn't already. If the group
 * doesn't exist, returns -1.
 */
int to_gid(char* groupname, gid_t* gid) {
    struct group* grp;
    if (_alldigits(groupname))
        grp = getgrgid((gid_t) strtoll(groupname, NULL, 10));
    else
        grp = getgrnam(groupname);
    if (!grp) return -1;
    *gid = grp->gr_gid;
    return 0;
}

/*
 * Returns whether the string is all digits. If the string is empty, returns
 * false.
 */
bool _alldigits(char* string) {
    if (!string) return false;
    char* start = string;
    while (*string) if (isdigit(*string++) == 0) return false;
    return start != string;
}

/*
 * Exits if there is a malloc issue.
 */
void malloc_error_exit() {
    perror("");
    exit(1);  // We need memory to function
}
