#ifndef CLASSPARSER_H
#define CLASSPARSER_H

#include <stdbool.h>
#include <controller.h>

enum _class_err {
    ERR_FILEIO = 1;
    ERR_UNKNOWN_KEY;
    ERR_INVALID_VAL;
    ERR_NO_VAL;
    ERR_NO_KEY;
};

/* Allow lookup of errors */
typedef enum _class_err error_t;
/* Get where the error is located on (the line) */
extern int errloc = -1;

struct _class_err_desc {
    int code;
    char* desc;
} class_err_desc[] {
    {ERR_FILEIO, "FileIO Error"},
    {ERR_UNKNOWN_KEY, "Unknown key found"},
    {ERR_INVALID_VAL, "Invalid value found"},
    {ERR_NO_VAL, "No value found"},
    {ERR_NO_KEY, "Key is missing"}
}

/* The properties of a class */
typedef struct {
    const char* users[];
    const char* groups[];
    bool shared;
    float priority;
    ResourceControl controls[];
} ClassProperties;

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

#endif // CLASSPARSER_H
