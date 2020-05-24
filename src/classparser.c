// SPDX-License-Identifier: GPL-3.0
#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <math.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include "classparser.h"
#include "hashmap.h"
#include "macros.h"
#include "utils.h"
#include "vector.h"

int _insert_class_prop(ClassProperties* prop, char* restrict key,
    char* restrict value);
void _parse_uids_or_gids(char* string, ClassProperties* props, bool uid_or_gid);
void _print_line_error(unsigned int linenum, const char* restrict filepath,
    const char* restrict desc);
int _is_classfile(const struct dirent* dir);
bool _in_class(uid_t uid, gid_t* groups, int ngroups, ClassProperties* props);
bool _uid_finder(const void* void_uid, va_list args);
bool _gids_finder(const void* void_gid, va_list args);

void destroy_class(ClassProperties* props)
{
    free((char*)props->filepath);
    destroy_vector(&props->users);
    destroy_vector(&props->groups);
    destroy_hashmap(&props->controls);
}

int create_class(const char* dir, const char* filename, ClassProperties* props)
{
    assert(dir);
    assert(filename);
    assert(props);

    const char* filepath = get_filepath(dir, filename);
    int r = parse_classfile(filepath, props);

    free((char*)filepath);
    return r;
}

int parse_classfile(const char* filepath, ClassProperties* props)
{
    assert(filepath);
    assert(props);
    memset(props, 0, sizeof *props);
    props->filepath = strdup(filepath);
    if (!props->filepath)
        return -1;
    if ((create_vector(&props->users, sizeof(uid_t))) < 0)
        return -1;
    if ((create_vector(&props->groups, sizeof(gid_t))) < 0)
        return -1;
    if ((create_hashmap(&props->controls, sizeof(char*), MAX_CONTROLS)) < 0)
        return -1;

    FILE* classfile = fopen(filepath, "r");
    if (!classfile) {
        syslog(LOG_ERR, "Failed to read class file %s: %s", filepath, strerror(errno));
        return -1;
    }

    unsigned int linenum = 0;
    bool errors = false;
    char buf[LINE_BUFSIZE] = { 0 };
    char* end = NULL;
    char* key = NULL;
    char* value = NULL;

    while ((end = fgets(buf, sizeof buf / sizeof *buf, classfile))) {
        linenum++;

        // Ignore blank lines
        if (!strcmp(end, "\n"))
            continue;

        // Ignore comments
        if (strchr(end, '#') == end)
            continue;

        // Ensure equal sign
        if (strchr(end, '=') == NULL) {
            _print_line_error(linenum, filepath, "No key=value found. Ignoring.");
            continue;
        }
        if (parse_key_value(end, &key, &value) == -1) {
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
        return errors ? -1 : 0;
    } else if (ferror(classfile)) {
        // There is an error with the stream
        syslog(LOG_ERR, "Failed to read class file %s: %s", filepath, strerror(errno));
    }
    return 0;
}

/*
 * Parses the given line into a key value pair. If there is a issue with
 * parsing, returns -1, sets key and value to NULL.
 */
int parse_key_value(char* line, char** restrict key, char** restrict value)
{
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
int _insert_class_prop(ClassProperties* props, char* restrict key,
    char* restrict value)
{
    assert(props && key && value);

    if (strcasecmp(key, "shared") == 0) {
        if (strcasecmp(value, "true") == 0 || strcasecmp(value, "yes") == 0) {
            props->shared = true;
            return 0;
        }
        if (strcasecmp(value, "false") == 0 || strcasecmp(value, "no") == 0) {
            props->shared = false;
            return 0;
        }
        return -1;
    }

    if (strcasecmp(key, "priority") == 0) {
        double priority = strtod(value, NULL);
        if (strcmp(value, "0") != 0 && props->priority == 0)
            return -1;

        props->priority = priority;
        return 0;
    }

    if (strcasecmp(key, "groups") == 0) {
        _parse_uids_or_gids(value, props, false);
        return 0;
    }

    if (strcasecmp(key, "users") == 0) {
        _parse_uids_or_gids(value, props, true);
        return 0;
    }

    return add_hashmap_entry(&props->controls, key, value);
}

/*
 * Parses uids or usernames out of the string if uid_or_gid is true, otherwise
 * parses gids or groups out of the string. The ids should be separated by
 * commas. Extra whitespace will be stripped and usernames or groupnames will
 * be converted to uids or gids. If a username or groupname doesn't have a
 * corresponding uid or gid, or if the uid or gid is not valid, it will be
 * skipped.
 */
void _parse_uids_or_gids(char* string, ClassProperties* props, bool uid_or_gid)
{
    id_t id = 0;
    char* token = "";

    while ((token = strsep(&string, ","))) {
        trim_whitespace(&token);
        if (uid_or_gid) {
            if (to_uid(token, &id) == -1)
                continue;

            append_vector_item(&props->users, (uid_t*)&id);
        } else {
            if (to_gid(token, &id) == -1)
                continue;

            append_vector_item(&props->groups, (gid_t*)&id);
        }
    }
}

/*
 * Reports on a error on a specific line in the given file.
 */
void _print_line_error(unsigned int linenum, const char* restrict filepath,
    const char* restrict desc)
{
    syslog(LOG_ERR, "Syntax error in %s:%u %s", filepath, linenum, desc);
}

static const char* curr_ext = "";

int list_class_files(const char* dir, const char* ext, struct dirent*** class_files,
    int* num_files)
{
    assert(num_files);

    curr_ext = ext;
    int filecount = scandir(dir, class_files, _is_classfile, alphasort);
    if (filecount == -1) {
        syslog(LOG_ERR, "Failed to read dir %s: %s", dir, strerror(errno));
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
int _is_classfile(const struct dirent* dir)
{
    // Allow unknown since not _all_ (but most) file systems support d_type
    return (dir && (dir->d_type == DT_REG || dir->d_type == DT_UNKNOWN)
        && has_ext((char*)dir->d_name, curr_ext));
}

int evaluate(uid_t uid, HashMap* classes, ClassProperties* props)
{
    assert(classes);
    assert(props);

    ClassProperties* choosen_class = NULL;
    gid_t* groups = NULL;
    int ngroups = 0;
    int props_match_count = 0;
    double highest_priority = -INFINITY;

    if (get_groups(uid, &groups, &ngroups) < 0) {
        syslog(LOG_ERR, "Failed to get group list for %d", uid);
        return -1;
    }

    ClassProperties* tmp_props = NULL;
    while ((tmp_props = iter_hashmap_values(classes))) {
        // Select first if same priority
        if (tmp_props->priority > highest_priority && _in_class(uid, groups, ngroups, tmp_props)) {
            highest_priority = tmp_props->priority;
            choosen_class = tmp_props;
            props_match_count++;
        }
    }
    iter_hashmap_end(classes);
    free(groups);

    if (choosen_class)
        *props = *choosen_class;
    return props_match_count;
}

/*
 * Returns whether the user belongs in the class.
 */
bool _in_class(uid_t uid, gid_t* groups, int ngroups, ClassProperties* props)
{
    assert(props);

    if (find_vector_item(&props->users, _uid_finder, uid))
        return true;
    if (find_vector_item(&props->groups, _gids_finder, groups, ngroups))
        return true;
    return false;
}

/*
 * Implements the vector finder interface for finding a uid, given as the
 * second argument, in a vector of uids.
 */
inline bool
_uid_finder(const void* void_uid, va_list args)
{
    assert(void_uid);

    const uid_t* uid = void_uid;
    uid_t our_uid = va_arg(args, uid_t);
    return our_uid == *uid;
}

/*
 * Implements the vector finder interface for finding whether any of a list of
 * gids, given as the second argument and the length as the third argument,
 * are in a vector of gids.
 */
inline bool
_gids_finder(const void* void_gid, va_list args)
{
    assert(void_gid);

    gid_t gid = *((gid_t*)void_gid);
    gid_t* our_gids = va_arg(args, gid_t*);
    int our_gid_count = va_arg(args, int);
    for (int i = 0; i < our_gid_count; i++)
        if (our_gids[i] == gid)
            return true;
    return false;
}
