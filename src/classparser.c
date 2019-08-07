#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <math.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include "utils.h"
#include "classparser.h"
#include "controller.h"
#include "macros.h"

const char* default_loc = "/etc/userctl";
const char* default_ext = ".class";

int _parse_line(char* line, char** restrict key, char** restrict value);
int _insert_class_prop(ClassProperties* prop, char* restrict key, char* restrict value);
void _parse_uids(char* string, ClassProperties* props);
void _parse_gids(char* string, ClassProperties* props);
void _print_line_error(unsigned long long linenum, const char* restrict filepath,
                       const char* restrict desc);
int _is_classfile(const struct dirent* dir);
bool _in_class(uid_t uid, gid_t* groups, int ngroups, ClassProperties* props);


void destroy_class(ClassProperties* props) {
    free(props->filepath);
    free(props->users);
    free(props->groups);
    destroy_control_list(props->controls, props->ncontrols);
}

int parse_classfile(const char* filepath, ClassProperties* props) {
    assert(filepath != NULL);
    memset(props, 0, sizeof *props);
    props->filepath = malloc(strlen(filepath) + 1);
    if (!props->filepath) malloc_error_exit();
    strcpy(props->filepath, filepath);

    FILE* classfile = fopen(props->filepath, "r");
    if (classfile != NULL) {
        unsigned int linenum = 0;
        bool errors = false;
        char buf[LINE_BUFSIZE];
        char* end;
        char* key;
        char* value;

        while((end = fgets(buf, sizeof buf / sizeof *buf, classfile)) != NULL) {
            if (linenum < UINT_MAX) linenum++;

            // Ignore blank lines
            if (!strcmp(end, "\n")) continue;

            // Ignore comments
            if (strchr(end, '#') == end) continue;

            // Ensure equal sign
            if (strchr(end, '=') == NULL) {
                _print_line_error(linenum, filepath, "No key=value found. Ignoring.");
                continue;
            }
            if (_parse_line(end, &key, &value) == -1) {
                _print_line_error(linenum, filepath, "Failed to parse key=value");
                errors = true;
                continue;
            }
            if (_insert_class_prop(props, key, value) == -1) {
                _print_line_error(linenum, filepath, "Unknown key=value pair");
                errors = true;
                continue;
            }
        }

        if (feof(classfile)) {
            // We are at the end of the file
            if (errors) return -1;
            else return 0;
        }
        else if (ferror(classfile)) {
            // There is an error with the stream
            perror(filepath);
        }
    }
    else {
        perror(filepath);
    }
    return -1;
}

/*
 * Parses the given line into a key value pair. If there is a issue with
 * parsing, returns -1, sets key and value to NULL.
 */
int _parse_line(char* line, char** restrict key, char** restrict value) {
    *value = NULL;
    *key = strsep(&line, "=");
    *value = line;
    if (*key == NULL || *key[0] == '\0' || *value == NULL || *value[0] == '\0')
        // Either the line didn't have the delim, or there is no value or key
        return -1;

    trim_whitespace(key);
    trim_whitespace(value);
    return 0;
}

/*
 * Inserts a class property into the ClassProperties struct. If either the key
 * or value doesn't match any of the properties characteristics, a -1 is
 * returned and the properties is indeterminate.
 */
int _insert_class_prop(ClassProperties* props, char* restrict key, char* restrict value) {
    assert(props != NULL && key != NULL && value != NULL);

    if (strcasecmp(key, "shared") == 0) {
        if (strcasecmp(value, "true") == 0 || strcasecmp(value, "yes") == 0)
            props->shared=true;
        else if (strcasecmp(value, "false") == 0 || strcasecmp(value, "no") == 0)
            props->shared=false;
        else
            return -1;
    }
    else if (strcasecmp(key, "priority") == 0) {
        props->priority = strtof(value, NULL);
        if (strcmp(value, "0") != 0 && props->priority == 0) return -1;
    }
    else if (strcasecmp(key, "groups") == 0) {
        _parse_gids(value, props);
    }
    else if (strcasecmp(key, "users") == 0) {
        _parse_uids(value, props);
    }
    else {
        // Assume it's a resource control
        int n = props->ncontrols;
        ResourceControl* controls = realloc(props->controls,
                                            sizeof *controls * (n + 1));
        if (!controls) malloc_error_exit();
        props->controls = controls;

        controls[n].key = malloc(strlen(key) + 1);
        controls[n].value = malloc(strlen(value) + 1);
        if (!controls[n].key || !controls[n].value) malloc_error_exit();
        strcpy(controls[n].key, key);
        strcpy(controls[n].value, value);

        props->ncontrols++;
    }
    return 0;
}

/*
 * Parses uids or usernames out of the string, separated by commas, stripping
 * extra whitespace and transforming usernames to uids. The uids are then put
 * in a malloced list. If the list is not NULL, the contents of the list are
 * copied into a new list. If a username doesn't have a corresponding uid, or
 * if the uid is not valid, it is skipped.
 */
void _parse_uids(char* string, ClassProperties* props) {
    unsigned int comma_count = 0;
    for (int i = 0; string[i] != '\0'; i++) if (string[i] == ',') comma_count++;

    int* nusers = &props->nusers;
    uid_t* users = props->users;
    uid_t* new_list = realloc(users, sizeof(*new_list) * (*nusers + comma_count));
    if (new_list == NULL) malloc_error_exit();
    props->users = users = new_list;
    uid_t* new_uids_list = users + *nusers;

    char* token;
    unsigned int uid_count = 0;
    while ((token = strsep(&string, ",")) != NULL) {
        trim_whitespace(&token);
        if (to_uid(token, new_uids_list) == -1) {
            continue;
        }
        uid_count++;
        new_uids_list++;
    }
    props->nusers = *nusers + uid_count;
    if (uid_count != comma_count) {
        // Resize since not all usernames were valid
        props->users = realloc(new_list, sizeof(*new_list) * props->nusers);
        if (props->users == NULL) malloc_error_exit();
    }
}

/*
 * Parses gids or groupnames out of the string, separated by commas, stripping
 * extra whitespace and transforming groupnames to gids. The gids are then put
 * in a malloced list. If the list is not NULL, the contents of the list are
 * copied into a new list. If a groupname doesn't have a corresponding gid, or
 * if the gid is not valid, it is skipped.
 */
void _parse_gids(char* string, ClassProperties* props) {
    unsigned int comma_count = 0;
    for (int i = 0; string[i] != '\0'; i++) if (string[i] == ',') comma_count++;

    int* ngroups = &props->ngroups;
    gid_t* groups = props->groups;
    gid_t* new_list = realloc(groups, sizeof(*new_list) * (*ngroups + comma_count));
    if (new_list == NULL) malloc_error_exit();
    props->groups = groups = new_list;
    gid_t* new_gids_list = groups + *ngroups;

    char* token;
    unsigned int gid_count = 0;
    while ((token = strsep(&string, ",")) != NULL) {
        trim_whitespace(&token);
        if (to_gid(token, new_gids_list) == -1) continue;
        gid_count++;
        new_gids_list++;
    }
    props->ngroups = *ngroups + gid_count;
    if (gid_count != comma_count) {
        // Resize since not all groupnames were valid
        props->groups = realloc(new_list, sizeof(*new_list) * (props->ngroups));
        if (props->groups == NULL) malloc_error_exit();
    }
}

int write_classfile(const char* filepath, ClassProperties* props) {
    assert(filepath != NULL);
    assert(props != NULL);
    // TODO: Write the function
    return 0;
}

/*
 * Reports on a error on a specific line in the given file.
 */
void _print_line_error(unsigned long long linenum, const char* restrict filepath,
                       const char* restrict desc) {
    fprintf(stderr, "%llu:%s %s\n", linenum, filepath, desc);
}

int list_class_files(struct dirent*** class_files, int* num_files) {
    assert(num_files != NULL);

    int filecount = scandir(default_loc, class_files, _is_classfile, alphasort);
    if (filecount == -1) {
        *num_files = 0;
        class_files = NULL;
        return -1;
    }
    *num_files = filecount;
    return 0;
}

/*
 * Returns 1 if extension matches the default, 0 otherwise.
 */
int _is_classfile(const struct dirent* dir) {
    // Make sure is a regular ol' file
    // Also, allow unknown since not _all_ (but most) file systems support d_type
    if(dir != NULL && (dir->d_type == DT_REG || dir->d_type == DT_UNKNOWN))
        return has_ext((char*) dir->d_name, (char*) default_ext);
    else
        return 0;
}


int evaluate(uid_t uid, ClassProperties* props_list, int nprops, int* index) {
    assert(props_list != NULL);

    errno = 0;
    struct passwd* pw = getpwuid(uid);
    if (!pw) return -1;

    int ngroups = (int) sysconf(_SC_NGROUPS_MAX);
    if (ngroups <= 0) ngroups = 65536;  // Good enough
    gid_t* groups = malloc(sizeof *groups * ngroups);
    if (groups == NULL) malloc_error_exit();
    if (getgrouplist(pw->pw_name, pw->pw_gid, groups, &ngroups) < 0)
        return -1;

    double highest_priority = -INFINITY;
    for (int i = 0; i < nprops; i++) {
        // Select first if same priority
        if ((double) props_list[i].priority > highest_priority &&
                _in_class(uid, groups, ngroups, &props_list[i])) {
            highest_priority = props_list[i].priority;
            *index = i;
        }
    }
    return 0;
}

/*
 * Returns whether the user belongs in the class.
 */
bool _in_class(uid_t uid, gid_t* groups, int ngroups, ClassProperties* props) {
    assert(props != NULL);
    for (int i = 0; i < props->nusers; i++)
        if (props->users[i] == uid) return true;

    for (int j = 0; j < ngroups; j++)
        for (int k = 0; k < props->ngroups; k++)
            if (props->groups[k] == groups[j]) return true;
    return false;
}

