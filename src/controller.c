// SPDX-License-Identifier: GPL-3.0
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

static int _load_props_list(char* dir, char* ext, Vector* props_list);
static bool _classname_finder(void *void_prop, va_list args);
static int _enforce_controls(uid_t uid, ResourceControl* controls, int ncontrols);

int init_context(Context* context) {
    context->classdir = strdup("/etc/userctl");
    context->classext = strdup(".class");
    if (!context->classdir || !context->classext) return -1;
    // FIXME: What if no /etc/userctl?
    return _load_props_list(context->classdir, context->classext,
                            &context->props_list);
}

void destroy_context(Context* context) {
    assert(context);

    ClassProperties *props;
    size_t nprops = get_vector_count(&context->props_list);

    for (size_t n = 0; n < nprops; n++) {
        props = get_vector_item(&context->props_list, n);
        destroy_class(props);
    }
    destroy_vector(&context->props_list);
    free(context->classdir);
    free(context->classext);
}

/*
 * Loads and initializes the props_list based on the found class files. Only
 * valid class files are returned. If there is a issue with getting the class
 * files, a -1 is returned (and errno should be looked up), otherwise zero is
 * returned.
 */
static int _load_props_list(char* dir, char* ext, Vector* props_list) {
    assert(dir && ext && props_list);
    struct dirent** class_files = NULL;
    int num_files = 0;
    if (list_class_files(dir, ext, &class_files, &num_files) < 0) return -1;

    assert(class_files);
    assert(*class_files); // FIXME: What if no class files!

    create_vector(props_list, sizeof (ClassProperties));
    ensure_vector_capacity(props_list, num_files);

    for (int i = 0; i < num_files; i++) {
        ClassProperties props;
        if (create_class(dir, class_files[i]->d_name, &props) < 1)
            append_vector_item(props_list, &props);

        free(class_files[i]);
    }
    free(class_files);
    return 0;
}

int method_list_classes(sd_bus_message* m, void* userdata, sd_bus_error* ret_error) {
    Context* context = userdata;
    sd_bus_message* reply = NULL;

    int r = sd_bus_message_new_method_return(m, &reply);
    if (r < 0) return r;

    pthread_rwlock_rdlock(&context_lock);
    size_t nprops = get_vector_count(&context->props_list);

    char** classnames = malloc(sizeof *classnames * (nprops + 1));  // + NULL
    if (!classnames) {
        pthread_rwlock_unlock(&context_lock);
        r = -ENOMEM;
        goto death;
    }
    classnames[nprops] = NULL;

    for (size_t n = 0; n < nprops; n++) {
        ClassProperties *props = get_vector_item(&context->props_list, n);
        classnames[n] = props->filepath;
    }

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
    int r;
    size_t users_size, groups_size;
    uid_t* users, *groups;

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

    char *classpath = get_filepath(context->classdir, classname);
    ClassProperties *props = find_vector_item(&context->props_list, _classname_finder, classpath);
    if (!props) {
        sd_bus_error_set_const(ret_error, "org.dylangardner.NoSuchClass",
                               "No such class found (may need to reload).");
        r = -EINVAL;
        goto late_death;
    }

    r = sd_bus_message_append(
        reply, "sbd",
        props->filepath,
        props->shared,
        props->priority
    );
    if (r < 0) goto late_death;

    users_size = sizeof *props->users * props->nusers;
    users = props->users;
    groups_size = sizeof *props->groups * props->ngroups;
    groups = props->groups;

    r = sd_bus_message_append_array(reply, 'u', users, users_size);
    if (r < 0) goto late_death;
    r = sd_bus_message_append_array(reply, 'u', groups, groups_size);
    if (r < 0) goto late_death;
    r = sd_bus_send(NULL, reply, NULL);

late_death:
    free(classpath);

death:
    pthread_rwlock_unlock(&context_lock);
    free(classname);
    free(reply);
    return r;
}

int method_reload_class(sd_bus_message* m, void* userdata, sd_bus_error* ret_error) {
    Context* context = userdata;
    sd_bus_message* reply = NULL;
    char* classname;
    int r;

    r = sd_bus_message_new_method_return(m, &reply);
    if (r < 0) return r;

    r = sd_bus_message_read(m, "s", &classname);
    if (r < 0) goto death;

    classname = strdup(classname);
    if (!classname) {
        r = -ENOMEM;
        goto death;
    }

    pthread_rwlock_wrlock(&context_lock);

    // Use classname.class instead of classname if .class extension is not given
    if (!has_ext(classname, (char*) context->classext)) {
        size_t new_size = strlen(classname) + strlen(context->classext) + 1;
        classname = realloc(classname, new_size);
        if (!classname) {
            pthread_rwlock_unlock(&context_lock);
            r = -ENOMEM;
            goto death;
        }
        strcat(classname, context->classext);
    }

    char *classpath = get_filepath(context->classdir, classname);
    ClassProperties *props = find_vector_item(&context->props_list, _classname_finder, classpath);
    if (!props) {
        sd_bus_error_set_const(ret_error, "org.dylangardner.NoSuchClass",
                               "No such class found (may need to reload).");
        r = -EINVAL;
        goto late_death;
    }

    // Backup onto the stack just in case of failure
    ClassProperties backup;
    memcpy(&backup, props, sizeof backup);

    // Now try and modify that class
    r = create_class(context->classdir, classname, props);
    if (r < 0) {
        r = -errno;
        memcpy(props, &backup, sizeof backup);

        // TODO: Return the error to the client
        sd_bus_error_set_const(ret_error, "org.dylangardner.ClassFailure",
                               "Class could not be loaded.");
        r = -EINVAL;
        goto late_death;
    }
    else {
        destroy_class(&backup);
    }
    r = sd_bus_send(NULL, reply, NULL);

late_death:
    pthread_rwlock_unlock(&context_lock);
    free(classname);

death:
    free(reply);
    return r;
}

/*
 * Implements the vector finder interface for finding a classname, given as
 * the second argument, in a vector of ClassProperties.
 */
static bool _classname_finder(void *void_prop, va_list args) {
    assert(void_prop);

    ClassProperties *props = void_prop;
    char *classpath = va_arg(args, char *);
    return strcmp(props->filepath, classpath) == 0;
}

int method_evaluate(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    Context* context = userdata;
    ClassProperties props;
    uid_t uid;
    int r;
    sd_bus_message* reply = NULL;

    r = sd_bus_message_new_method_return(m, &reply);
    if (r < 0) return r;

    r = sd_bus_message_read(m, "u", &uid);
    if (r < 0) goto death;

    pthread_rwlock_rdlock(&context_lock);
    r = evaluate(uid, &context->props_list, &props);
    if (r < 0) goto unlock_death;
    if (r == 0) {
        sd_bus_error_setf(ret_error, "org.dylangardner.NoClassForUser",
                          "No class found for the user.");
        r = -EINVAL;
        goto unlock_death;
    }

    r = sd_bus_message_append_basic(reply, 's', props.filepath);
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
    ClassProperties props;
    int r;
    uid_t uid;

    r = sd_bus_message_read(m, "uo", &uid, NULL);
    if (r < 0) return r;

    printf("Enforcing resource controls on %d\n", uid);
    pthread_rwlock_rdlock(&context_lock);
    r = evaluate(uid, &context->props_list, &props);
    if (r < 0) goto death;

    // User has no class; ignore
    if (r == 0) {
        printf("%d belongs to no class. Ignoring.\n", uid);
        r = 0;
        goto death;
    }
    r = _enforce_controls(uid, props.controls, props.ncontrols);

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

    if (ncontrols < 1) return 0;

    int argc_prefix = 3;                              // systemctl + set-property + unit_name
    int argc = argc_prefix + ncontrols;               // + controls ...
    char **argv = malloc(sizeof *argv * (argc + 1));  // + NULL
    if (!argv) return -ENOMEM;

    argv[0] = "systemctl";
    argv[1] = "set-property";
    char unit_name[24];  // 32 bit uid can only be at most 11 chars long
    snprintf(unit_name, 24, "user-%u.slice", uid);
    argv[2] = unit_name;

    for (int i = 0; i < ncontrols; i++) {
        arglen = (strlen(controls[i].key) + strlen(controls[i].value) + 2);
        arg = malloc(sizeof *arg * arglen);
        if (!arg) {
            r = -ENOMEM;
            goto death;
        }
        snprintf(arg, arglen, "%s=%s", controls[i].key, controls[i].value);
        argv[argc_prefix + i] = arg;
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
    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) == 0) goto death;

        for (int i = 0; argv[i]; i++) fprintf(stderr, "%s ", argv[i]);
        fprintf(stderr, "exited with non-zero status code: %d", WEXITSTATUS(status));
    }
    else if (WIFSIGNALED(status)) {
        if (WTERMSIG(status) == 0) goto death;

        for (int i = 0; argv[i]; i++) fprintf(stderr, "%s ", argv[i]);
        fprintf(stderr, "recieved a signal: %s", strsignal(WTERMSIG(status)));
    }

death:
    for (int i = 0; i < ncontrols; i++) free(argv[argc_prefix + i]);
    return r;
}
