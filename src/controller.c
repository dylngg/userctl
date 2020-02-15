#define _GNU_SOURCE  // (basename)
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-bus.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "classparser.h"
#include "controller.h"
#include "utils.h"

static int _load_props_list(char* dir, char* ext, ClassProperties** props_list, int* nprops);
static int _index_of_classname(ClassProperties* props_list, int nprops, char* classname);
static int _enforce_controls(uid_t uid, ResourceControl* controls, int ncontrols);

int init_context(Context* context) {
    context->classdir = strdup("/etc/userctl");
    context->classext = strdup(".class");
    if (!context->classdir || !context->classext) return -1;
    // FIXME: What if no /etc/userctl?
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
static int _load_props_list(char* dir, char* ext,  ClassProperties** props_list, int* nprops) {
    assert(dir && ext && props_list && nprops);
    struct dirent** class_files = NULL;
    int num_files = 0, n = 0;
    if (list_class_files(dir, ext, &class_files, &num_files) < 0) return -1;

    assert(class_files); // FIXME: What if no class files!
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

    pthread_rwlock_rdlock(&context_lock);
    Context* context = userdata;
    int nprops = context->nprops;
    char** classnames = malloc(sizeof *classnames * (nprops + 1));
    if (!classnames) {
        pthread_rwlock_unlock(&context_lock);
        r = -ENOMEM;
        goto death;
    }
    classnames[nprops] = NULL;

    for (int n = 0; n < nprops; n++)
        classnames[n] = context->props_list[n].filepath;

    pthread_rwlock_unlock(&context_lock);
    r = sd_bus_message_append_strv(reply, classnames);
    free(classnames);
    if (r < 0) goto death;
    r = sd_bus_send(NULL, reply, NULL);

death:
    free(reply);
    sd_bus_error_set_errno(ret_error, r);
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
    if (r < 0) {
        free(reply);
        return r;
    }
    classname = strdup(classname);
    if (!classname) {
        free(reply);
        return r;
    }

    pthread_rwlock_rdlock(&context_lock);

    // Use classname.class instead of classname if .class extension is not given
    if (!has_ext(classname, (char*) context->classext)) {
        size_t new_size = strlen(classname) + strlen(context->classext) + 1;
        classname = realloc(classname, new_size);
        if (!classname) {
            r = -ENOMEM;
            goto death;
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
    pthread_rwlock_unlock(&context_lock);
    free(classname);
    free(reply);
    return r;
}

/*
 * Returns the index at which the classname was found in the props_list, or -1
 * if no class has that name.
 */
static int _index_of_classname(ClassProperties* props_list, int nprops, char* classname) {
    assert(props_list && classname);

    for (int i = 0; i < nprops; i++) {
        char* curr_name = basename(props_list[i].filepath);
        if (strcmp(curr_name, classname) == 0) return i;
    }
    return -1;
}

int method_evaluate(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    Context* context = userdata;
    uid_t uid;
    sd_bus_message* reply = NULL;
    int r = sd_bus_message_new_method_return(m, &reply);
    if (r < 0) return r;

    r = sd_bus_message_read(m, "u", &uid);
    if (r < 0) goto death;

    int index = -1;
    if (sizeof(uid_t) != sizeof(uint32_t) && uid > UINT32_MAX) {
        r = -EINVAL;
        goto death;
    }

    pthread_rwlock_rdlock(&context_lock);
    r = evaluate((uid_t) uid, context->props_list, context->nprops, &index);
    if (r < 0) goto unlock_death;
    if (index < 0) {
        sd_bus_error_setf(ret_error, "org.dylangardner.NoClassForUser",
                          "No class found for the user.");
        r = -EINVAL;
        goto unlock_death;
    }

    char* classname = context->props_list[index].filepath;
    r = sd_bus_message_append_basic(reply, 's', classname);
    if (r < 0) goto unlock_death;
    r = sd_bus_send(NULL, reply, NULL);

unlock_death:
    pthread_rwlock_unlock(&context_lock);
death:
    free(reply);
    sd_bus_error_set_errno(ret_error, r);
    return r;
}

int match_user_new(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    Context* context = userdata;
    int r, index, ncontrols;
    uid_t uid;

    r = sd_bus_message_read(m, "uo", &uid, NULL);
    if (r < 0) return r;

    printf("Enforcing resource controls on %d\n", uid);
    pthread_rwlock_rdlock(&context_lock);
    r = evaluate(uid, context->props_list, context->nprops, &index);
    if (r < 0) goto death;

    // User has no class; ignore
    if (index < 0) {
        printf("%d belongs to no class. Ignoring.\n", uid);
        r = 0;
        goto death;
    }
    ResourceControl* controls = context->props_list[index].controls;
    ncontrols = context->props_list[index].ncontrols;
    r = _enforce_controls(uid, controls, ncontrols);

death:
    pthread_rwlock_unlock(&context_lock);
    sd_bus_error_set_errno(ret_error, r);
    return r;
}

static int _enforce_controls(uid_t uid, ResourceControl* controls, int ncontrols) {
    int r = 0;
    pid_t pid;
    int status, arglen;
    char *arg;
    int nargs = 3;  // The actual count we've processed
    int argc = 2 + ncontrols;  // set-property + unit_name + controls ...
    char **argv = malloc(sizeof *argv * (argc + 1));;
    if (!argv) return -ENOMEM;
    argv[0] = "systemctl";
    argv[1] = "set-property";
    char unit_name[24];  // 32 bit uid can only be at most 11 chars long
    snprintf(unit_name, 24, "user-%u.slice", (uint32_t) uid);
    argv[2] = unit_name;

    for (int i = 0; i < ncontrols; i++) {
        arglen = (strlen(controls[i].key) + strlen(controls[i].value) + 2);
        arg = malloc(sizeof *arg * arglen);
        if (!arg) {
            r = -ENOMEM;
            goto death;
        }
        snprintf(arg, arglen, "%s=%s", controls[i].key, controls[i].value);
        argv[nargs++] = arg;
    }
    argv[argc] = NULL;

    pid = fork();
    if (pid == -1) {
        perror("Failed to fork and set property");
        return -errno;
    }
    if (pid == 0) {
        if (execv("/bin/systemctl", argv) == -1)
            errno_die("Failed to exec and set property");
    }
    waitpid(pid, &status, 0);
    // FIXME: Properly wait and log things out

death:
    for (int i = 3; i < nargs; i++) free(argv[i]);
    return r;
}
