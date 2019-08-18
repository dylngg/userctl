#define _GNU_SOURCE  // (basename)
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <systemd/sd-bus.h>

#include "classparser.h"
#include "controller.h"
#include "utils.h"

int _load_props_list(char* dir, char* ext, ClassProperties** props_list, int* nprops);
int _index_of_classname(ClassProperties* props_list, int nprops, char* classname);

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
    if (!classnames) {
        r = -ENOMEM;
        goto death;
    }
    classnames[nprops] = NULL;

    for (int n = 0; n < nprops; n++)
        classnames[n] = context->props_list[n].filepath;

    r = sd_bus_message_append_strv(reply, classnames);
    free(classnames);
    if (r < 0) goto death;
    r = sd_bus_send(NULL, reply, NULL);

death:
    free(reply);
    return r;
}

int method_get_class(sd_bus_message* m, void* userdata, sd_bus_error* ret_error) {
    Context* context = userdata;
    sd_bus_message* reply = NULL;
    char* classname;
    int index, r;
    size_t users_size, groups_size;
    uint32_t* users, *groups;

    r = sd_bus_message_new_method_return(m, &reply);
    if (r < 0) return r;

    r = sd_bus_message_read(m, "s", &classname);
    classname = strdup(classname);
    if (!classname) goto death;
    if (r < 0) goto death;

    // Use classname.class instead of classname if .class extension is not given
    if (!has_ext(classname, (char*) context->classext)) {
        size_t new_size = strlen(classname) + strlen(context->classext) + 1;
        classname = realloc(classname, new_size);
        if (!classname) {
            free(reply);
            return -ENOMEM;
        }
        strcat(classname, context->classext);
    }

    index = _index_of_classname(context->props_list, context->nprops, classname);
    if (index < 0) {
        sd_bus_error_set_const(ret_error, "org.dylangardner.NoSuchClass",
                               "No such class found (may need to reload).");
        r = -EINVAL;
        goto death;
    }
    ClassProperties* props = &context->props_list[index];
    r = sd_bus_message_append(
        reply, "sbd",
        props->filepath,
        props->shared,
        props->priority
    );
    if (r < 0) goto death;

    // Modern Linux uses uint32_t for ids, to be safe we'll convert to uint32_t
    // if needed.
    users_size = sizeof(uid_t) * props->nusers;
    users = props->users;
    groups_size = sizeof(uid_t) * props->ngroups;
    groups = props->groups;
    if (sizeof(id_t) < sizeof(uint32_t)) {
        users_size = sizeof(uint32_t) * props->nusers;
        users = malloc(users_size);
        if (!users) {
            r = -ENOMEM;
            goto death;
        }
        for (int i = 0; i < props->nusers; i++) users[i] = (uint32_t) props->users[i];

        groups_size = sizeof(uint32_t) * props->ngroups;
        groups = malloc(groups_size);
        if (!groups) {
            free(users);
            r = -ENOMEM;
            goto death;
        }
        for (int i = 0; i < props->ngroups; i++) groups[i] = (uint32_t) props->groups[i];
    }

    r = sd_bus_message_append_array(reply, 'u', users, users_size);
    if (r < 0) goto late_death;
    r = sd_bus_message_append_array(reply, 'u', groups, groups_size);
    if (r < 0) goto late_death;
    r = sd_bus_send(NULL, reply, NULL);

late_death:
    free(users);
    free(groups);

death:
    free(classname);
    free(reply);
    return r;
}

/*
 * Returns the index at which the classname was found in the props_list, or -1
 * if no class has that name.
 */
int _index_of_classname(ClassProperties* props_list, int nprops, char* classname) {
    assert(props_list && classname);

    for (int i = 0; i < nprops; i++) {
        char* curr_name = basename(props_list[i].filepath);
        if (strcmp(curr_name, classname) == 0) return i;
    }
    return -1;
}
