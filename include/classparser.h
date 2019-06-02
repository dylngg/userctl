#ifndef CLASSPARSER_H
#define CLASSPARSER_H

#include <stdbool.h>

#include "controller.h"

// If ERR_ERRNO, check errno for error
enum _class_err {
    ERR_ERRNO = 1,
    ERR_UNKNOWN_KEY,
    ERR_INVALID_VAL,
    ERR_NO_VAL,
    ERR_NO_KEY,
};

/* Allow lookup of errors */
typedef enum _class_err error_t;
/* Get where the error is located on (the line) */
extern int errloc;

typedef struct {
    int code;
    char* desc;
} _class_err_desc;

_class_err_desc class_err_desc[5];

/* The properties of a class */
typedef struct {
    bool shared;
    float priority;
    const char** groups;
    const char** users;
    ResourceControl controls[];
} ClassProperties;

/* The default location of classes */
extern const char* default_loc;
extern const char* default_ext;

/*
 * Parses a class file and passes a ClassProperties struct into props. If
 * there was an issue parsing the class file, returns a error_t and sets props
 * to null. Otherwise, a zero is returned.
 */
error_t parse_classfile(const char* filename, ClassProperties* props);

/*
 * Writes a class to the file. If there is an issue writing the class file,
 * returns a error_t. Otherwise, a zero is returned.
 */
error_t write_classfile(const char* filename, ClassProperties props);

/*
 * Returns a allocated list of allocated dirent class files in the default
 * class location and passes back the number of files. Returns a error_t if
 * there is an error, otherwise a zero is returned. A ERR_ERRNO error is the
 * only error that can be returned.
 *
 * Note: dirent's d_type may be a DT_UNKNOWN. Do appropriate checks before
 * reading from it.
 */
error_t list_class_files(struct dirent*** class_files, int* num_files);

#endif // CLASSPARSER_H
