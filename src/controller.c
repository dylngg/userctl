#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <systemd/sd-bus.h>

#include "classparser.h"
#include "controller.h"
#include "utils.h"

int _load_props_list(char* dir, char* ext, ClassProperties** props_list, int* nprops);

int init_context(Context* context) {
    context->classdir = strdup("/etc/userctl");
    context->classext = strdup(".class");
    if (!context->classdir || !context->classext) return -1;
    return _load_props_list(context->classdir, context->classext,
                            &context->props_list, &context->nprops);
}

void destroy_context(Context* context) {
    for (int i = 0; i < context->nprops; i++)
        destroy_class(&context->props_list[i]);
    free(context->props_list);
    free(context->classdir);
    free(context->classext);
}

int reload_context(Context* context) {
    if (context->props_list) free(context->props_list);
    return _load_props_list(context->classdir, context->classext,
                            &context->props_list, &context->nprops);
}

/*
 * Loads and initializes the props_list based on the found class files. Only
 * valid class files are returned. If there is a issue with getting the class
 * files, a -1 is returned (and errno should be looked up), otherwise zero is
 * returned.
 */
int _load_props_list(char* dir, char* ext,  ClassProperties** props_list, int* nprops) {
    assert(dir && ext && props_list && nprops);
    struct dirent** class_files = NULL;
    int num_files = 0, n = 0;
    if (list_class_files(dir, ext, &class_files, &num_files) < 0) return -1;

    assert(class_files);
    ClassProperties* list = malloc(sizeof *list * num_files);
    if (!list) return -1;

    for (int i = 0; i < num_files; i++) {
        if (class_files[i]) {
            char* filepath = get_filepath(dir, class_files[i]->d_name);
            if (parse_classfile(filepath, &list[n]) != -1) n++;
            free(filepath);
            free(class_files[i]);
        }
    }
    free(class_files);

    if (n > 0 && n < num_files) list = realloc(list, sizeof *list * n);
    *nprops = n;
    *props_list = list;
    return 0;
}

int method_list_classes(sd_bus_message* m, void* userdata, sd_bus_error* ret_error) {
    sd_bus_message* reply = NULL;
    int r = sd_bus_message_new_method_return(m, &reply);
    if (r < 0) return r;

    Context* context = userdata;
    int nprops = context->nprops;
    char** classnames = malloc(sizeof *classnames * (nprops + 1));
    if (!classnames) return -ENOMEM;
    classnames[nprops] = NULL;

    for (int n = 0; n < nprops; n++)
        classnames[n] = context->props_list[n].filepath;

    r = sd_bus_message_append_strv(reply, classnames);
    if (r < 0) return r;
    r = sd_bus_send(NULL, reply, NULL);
    free(reply);
    return r;
}
