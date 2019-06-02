#define _GNU_SOURCE
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "classparser.h"
#include "controller.h"

int errloc = -1;
const char* default_loc = "/etc/userctl";
const char* default_ext = ".class";

_class_err_desc class_err_desc[5] = {
    // If ERR_ERRNO, check errno for error
    {ERR_ERRNO, "Error"},
    {ERR_UNKNOWN_KEY, "Unknown key found"},
    {ERR_INVALID_VAL, "Invalid value found"},
    {ERR_NO_VAL, "No value found"},
    {ERR_NO_KEY, "Key is missing"}
};

error_t parse_classfile(const char* filename, ClassProperties* props) {
    // TODO
    return 0;
}

error_t write_classfile(const char* filename, ClassProperties props) {
    // TODO
    return 0;
}

/*
 * Returns 1 if extension matches the default, 0 otherwise.
 */
int class_ext(const struct dirent* dir) {
    // Make sure is a regular ol' file
    // Also, allow unknown since not _all_ (but most) file systems support d_type
    if(dir != NULL && (dir->d_type == DT_REG || dir->d_type == DT_UNKNOWN)) {
        const char *ext = strrchr(dir->d_name, '.');

        // Want name + extension
        if(ext != NULL && ext != dir->d_name && strcmp(ext, default_ext) == 0)
            return 1;
    }
    return 0;
}

error_t list_class_files(struct dirent*** class_files, int* num_files) {
    int filecount = scandir(default_loc, class_files, class_ext, alphasort);
    if (filecount == -1) {
        *num_files = 0;
        class_files = NULL;
        return ERR_ERRNO;
    }
    *num_files = filecount;
    return 0;
}

