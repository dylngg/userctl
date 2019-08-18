#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "utils.h"

bool _alldigits(char* string);

void die(char* quote) {
    fputs(quote, stderr);
    exit(1);
}

void errno_die(char* quote) {
    perror(quote);
    exit(1);
}

int to_uid(char* username, uid_t* uid) {
    errno = 0;
    struct passwd* pw;
    if (_alldigits(username))
        pw = getpwuid((uid_t) strtoll(username, NULL, 10));
    else
        pw = getpwnam(username);
    if (!pw) return -1;
    *uid = pw->pw_uid;
    return 0;
}

int to_username(uid_t uid, char** username) {
    errno = 0;
    struct passwd* pw;
    pw = getpwuid(uid);
    if (!pw) return -1;
    *username = pw->pw_name;
    return 0;
}

int to_gid(char* groupname, gid_t* gid) {
    errno = 0;
    struct group* grp;
    if (_alldigits(groupname))
        grp = getgrgid((gid_t) strtoll(groupname, NULL, 10));
    else
        grp = getgrnam(groupname);
    if (!grp) return -1;
    *gid = grp->gr_gid;
    return 0;
}

int to_groupname(gid_t gid, char** groupname) {
    errno = 0;
    struct group* grp;
    grp = getgrgid(gid);
    if (!grp) return -1;
    *groupname = grp->gr_name;
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

void trim_whitespace(char** string) {
    // Trim leading whitespace
    while(isspace((unsigned char) **string)) (*string)++;
    if(**string == '\0') return;  // ignore if all spaces

    // Trip trailing whitespace
    char* end = *string + strlen(*string) - 1;
    while(end > *string && isspace((unsigned char) *end)) end--;
    end[1] = '\0';
}

bool has_ext(char* restrict string, char* restrict ext) {
    const char *ending = strrchr(string, '.');
    // Want name + extension
    return (ending && ending != string && strcmp(ending, ext) == 0);
}

bool valid_filename(char* filename) {
    char bad_chars[] = "!@%^*~|/";
    for (unsigned int i = 0; i < strlen(bad_chars); i++)
        if (!strchr(filename, bad_chars[i])) return false;
    return true;
}

char* get_filepath(const char* restrict loc, char* restrict filename) {
    char *filepath = malloc(strlen(loc) + strlen(filename) + 2);
    if (!filepath) malloc_error_exit();
    sprintf(filepath, "%s/%s", loc, filename);
    return filepath;
}

void malloc_error_exit() {
    errno_die("");
}
