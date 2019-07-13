#ifndef CLASSPARSER_H
#define CLASSPARSER_H

#include <stdbool.h>
#include <sys/types.h>
#include <pwd.h>

#include "controller.h"


/* The properties of a class */
typedef struct {
    bool shared;
    float priority;
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
int list_class_files(struct dirent*** class_files, int* num_files);

/*
 * Evaluates a user for what class they belong to. If there are multiple
 * classes that the user belongs to, the highest priority class is selected.
 * If there are duplicate highest priorities, the first class found is
 * returned. Returns a -1 if there is an error, otherwise a zero is returned.
 * If a -1 is returned, the issue should be looked up via errno. In that case,
 * the result of index is indeterminate.
 */
int evaluate(uid_t uid, ClassProperties* props_list[], int nprops, int* index);

#endif // CLASSPARSER_H
