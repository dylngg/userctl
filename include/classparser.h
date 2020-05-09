// SPDX-License-Identifier: GPL-3.0
#ifndef CLASSPARSER_H
#define CLASSPARSER_H

#include <dirent.h>
#include <stdbool.h>
#include <sys/types.h>
#include <pwd.h>

#include "vector.h"

/* A resource control (systemd) */
typedef struct ResourceControl {
    char* key;
    char* value;
} ResourceControl;

/*
 * Destroys the ResourceControl struct by deallocating things.
 */
void destroy_control_list(ResourceControl* controls, int ncontrols);

/* The properties of a class */
typedef struct {
    char* filepath;
    bool shared;
    double priority;
    gid_t* groups;
    int ngroups;
    uid_t* users;
    int nusers;
    ResourceControl* controls;
    int ncontrols;
} ClassProperties;

/*
 * Destroys the ClassProperties struct by deallocating things.
 */
void destroy_class(ClassProperties* props);

/*
 * Creates a ClassProperties struct for the given class in the given directory.
 * If there was an issue parsing the class file, returns a -1. In that case,
 * the result of props is undefined. Otherwise, a zero is returned.
 */
int create_class(char *dir, char *filename, ClassProperties *props);

/* The default location of classes */
extern const char* default_loc;
extern const char* default_ext;

/*
 * Parses a class file and passes a ClassProperties struct into props. If
 * there was an issue parsing the class file, returns a -1 and prints the
 * error. In that case, the result of props is undefined. Otherwise, a zero is
 * returned.
 */
int parse_classfile(const char* filename, ClassProperties* props);

/*
 * Writes a class to the file. If there is an issue writing the class file,
 * returns -1 and prints the error. Otherwise, a zero is returned.
 */
int write_classfile(const char* filename, ClassProperties* props);

/*
 * Returns a allocated list of allocated dirent class files in the default
 * class location and passes back the number of files. Returns a -1 if there
 * is an error, otherwise a zero is returned. If a -1 is returned, the issue
 * should be looked up via errno and the parameters are NULL.
 *
 * Note: dirent's d_type may be a DT_UNKNOWN. Do appropriate checks before
 * reading from it.
 */
int list_class_files(char* dir, char* ext, struct dirent*** class_files, int* num_files);

/*
 * Evaluates a user for what class they belong to. If there are multiple
 * classes that the user belongs to, the highest priority class is selected.
 * If there are duplicate highest priorities, the first class found is
 * returned. If there are no classes that the user belongs to, the props is
 * untouched. Returns a -1 if there is an error and the number of classes that
 * the user belongs to. If a -1 is returned, the issue should be looked up via
 * errno.
 */
int evaluate(uid_t uid, Vector *props_list, ClassProperties* props);

#endif // CLASSPARSER_H
