// SPDX-License-Identifier: GPL-3.0
#define _GNU_SOURCE // (basename)
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include "classparser.h"
#include "controller.h"
#include "hashmap.h"
#include "utils.h"
#include "vector.h"

static int _load_class_properties(char* dir, char* ext, HashMap* classes);
static int _enforce_controls(uid_t uid, HashMap* controls);
static int _enforce_controls_on_class(const char* classpath, HashMap* classes);
static int _active_uids_and_class(Vector* uids, Vector* classes, HashMap* all_classes);

int init_context(Context* context)
{
    context->classdir = strdup("/etc/userctl");
    context->classext = strdup(".class");
    if (!context->classdir || !context->classext)
        return -1;
    // FIXME: What if no /etc/userctl?
    return _load_class_properties(context->classdir, context->classext,
        &context->classes);
}

void destroy_context(Context* context)
{
    assert(context);

    ClassProperties* props = NULL;
    while ((props = iter_hashmap_values(&context->classes)))
        destroy_class(props);

    destroy_hashmap(&context->classes);
    free(context->classdir);
    free(context->classext);
}

/*
 * Loads and initializes the classes based on the found class files. Only
 * valid class files are returned. If there is a issue with getting the class
 * files, a -1 is returned (and errno should be looked up), otherwise zero is
 * returned.
 */
static int
_load_class_properties(char* dir, char* ext, HashMap* classes)
{
    assert(dir && ext && classes);
    struct dirent** class_files = NULL;
    int num_files = 0;

    // FIXME: Limit classes in list_class_files, rather than here
    if (list_class_files(dir, ext, &class_files, &num_files) < 0)
        return -1;

    assert(class_files);
    assert(*class_files); // FIXME: What if no class files!

    int r = create_hashmap(classes, sizeof(ClassProperties), MAX_CLASSES);
    if (r < 0)
        return -1;

    for (int i = 0; i < num_files; i++) {
        if (i >= MAX_CLASSES) {
            fprintf(stderr, "Skipping %s because the max class count has "
                            "hit (%d)",
                class_files[i]->d_name, MAX_CLASSES);
            free(class_files[i]);
            break;
        }

        ClassProperties props;
        if (create_class(dir, class_files[i]->d_name, &props) >= 0) {
            char* classname = basename(class_files[i]->d_name);
            add_hashmap_entry(classes, classname, &props);
        }
        free(class_files[i]);
    }
    free(class_files);
    return 0;
}

int method_list_classes(sd_bus_message* m, void* userdata, sd_bus_error* ret_error)
{
    Context* context = userdata;
    sd_bus_message* reply = NULL;

    int r = sd_bus_message_new_method_return(m, &reply);
    if (r < 0)
        return r;

    pthread_rwlock_rdlock(&context_lock);

    Vector classnames;
    r = create_vector(&classnames, sizeof(char*));
    if (r < 0) {
        pthread_rwlock_unlock(&context_lock);
        r = -ENOMEM;
        goto cleanup;
    }
    size_t nclasses = get_hashmap_count(&context->classes);
    ensure_vector_capacity(&classnames, nclasses);

    ClassProperties* props;
    while ((props = iter_hashmap_values(&context->classes)))
        append_vector_item(&classnames, &props->filepath);

    iter_hashmap_end(&context->classes);

    char** classnames_strv = pretend_vector_is_array(&classnames);
    r = sd_bus_message_append_strv(reply, classnames_strv);
    if (r < 0)
        goto cleanup_classnames;

    r = sd_bus_send(NULL, reply, NULL);

cleanup_classnames:
    destroy_vector(&classnames);

cleanup:
    pthread_rwlock_unlock(&context_lock);
    sd_bus_error_set_errno(ret_error, r);
    sd_bus_message_unrefp(&reply);
    return r;
}

/*
 * Returns a classname with the given extension if it is not at the end of the
 * classname. If there was an error, -1 is returned. Otherwise, 1 to indicate
 * a new allocation (should be free'd), 0 otherwise.
 */
static int
complete_classname(char* classname, const char* ext, char** completed)
{
    if (!has_ext(classname, ext)) {
        size_t new_size = strlen(classname) + strlen(ext) + 1;
        *completed = malloc(sizeof **completed * new_size);
        if (!*completed)
            return -1;

        strcpy(*completed, classname);
        strcat(*completed, ext);
        return 1;
    }
    *completed = classname;
    return 0;
}

int method_get_class(sd_bus_message* m, void* userdata, sd_bus_error* ret_error)
{
    Context* context = userdata;
    sd_bus_message* reply = NULL;

    int r = sd_bus_message_new_method_return(m, &reply);
    if (r < 0)
        return r;

    char* given_classname = NULL;
    r = sd_bus_message_read(m, "s", &given_classname);
    if (r < 0)
        goto cleanup;

    pthread_rwlock_rdlock(&context_lock);

    // Use classname.class instead of classname if .class extension is not given
    char* classname = NULL;
    r = complete_classname(given_classname, context->classext, &classname);
    if (r < 0) {
        r = -errno;
        goto unlock_cleanup;
    }
    bool is_alloc_classname = (r == 1);

    ClassProperties* props = get_hashmap_entry(&context->classes, classname);
    if (!props) {
        sd_bus_error_set_const(ret_error, "org.dylangardner.NoSuchClass",
            "No such class found (may need to daemon-reload).");
        r = -EINVAL;
        goto unlock_cleanup_classname;
    }

    r = sd_bus_message_append(reply, "sbd", props->filepath,
        props->shared, props->priority);
    if (r < 0)
        goto unlock_cleanup_classname;

    uid_t* users = pretend_vector_is_array(&props->users);
    size_t users_size = get_vector_count(&props->users) * sizeof *users;
    r = sd_bus_message_append_array(reply, 'u', users, users_size);
    if (r < 0)
        goto unlock_cleanup_classname;

    gid_t* groups = pretend_vector_is_array(&props->groups);
    size_t groups_size = get_vector_count(&props->groups) * sizeof *groups;
    r = sd_bus_message_append_array(reply, 'u', groups, groups_size);
    if (r < 0)
        goto unlock_cleanup_classname;

    r = sd_bus_send(NULL, reply, NULL);

unlock_cleanup_classname:
    if (is_alloc_classname)
        free(classname);

unlock_cleanup:
    pthread_rwlock_unlock(&context_lock);

cleanup:
    sd_bus_error_set_errno(ret_error, r);
    sd_bus_message_unrefp(&reply);
    return r;
}

int method_reload_class(sd_bus_message* m, void* userdata, sd_bus_error* ret_error)
{
    Context* context = userdata;
    sd_bus_message* reply = NULL;

    int r = sd_bus_message_new_method_return(m, &reply);
    if (r < 0)
        return r;

    char* given_classname = NULL;
    r = sd_bus_message_read(m, "s", &given_classname);
    if (r < 0)
        goto cleanup;

    pthread_rwlock_wrlock(&context_lock);

    // Use classname.class instead of classname if .class extension is not given
    char* classname = NULL;
    r = complete_classname(given_classname, context->classext, &classname);
    if (r < 0) {
        r = -errno;
        goto unlock_cleanup;
    }
    bool is_alloc_classname = (r == 1);

    ClassProperties* props = get_hashmap_entry(&context->classes, classname);
    if (!props) {
        sd_bus_error_set_const(ret_error, "org.dylangardner.NoSuchClass",
            "No such class found (may need to daemon-reload).");
        r = -EINVAL;
        goto unlock_cleanup_classname;
    }

    // Backup onto the stack just in case of failure
    ClassProperties backup;
    memcpy(&backup, props, sizeof backup);

    // Now try and modify that class
    r = create_class(context->classdir, classname, props);
    if (r < 0) {
        r = -errno;
        memcpy(props, &backup, sizeof backup);
        sd_bus_error_set_const(ret_error, "org.dylangardner.ClassFailure",
            "Class could not be loaded.");
        goto unlock_cleanup_classname;
    } else {
        destroy_class(&backup);
    }
    _enforce_controls_on_class(props->filepath, &context->classes);
    r = sd_bus_send(NULL, reply, NULL);

unlock_cleanup_classname:
    if (is_alloc_classname)
        free(classname);

unlock_cleanup:
    pthread_rwlock_unlock(&context_lock);

cleanup:
    sd_bus_error_set_errno(ret_error, r);
    sd_bus_message_unrefp(&reply);
    return r;
}

int method_daemon_reload(sd_bus_message* m, void* userdata, sd_bus_error* ret_error)
{
    Context* context = userdata;
    sd_bus_message* reply = NULL;

    int r = sd_bus_message_new_method_return(m, &reply);
    if (r < 0)
        return r;

    pthread_rwlock_wrlock(&context_lock);

    Context backup;
    memcpy(&backup, context, sizeof backup);

    if ((init_context(context)) < 0) {
        r = -errno;
        memcpy(context, &backup, sizeof backup);
        sd_bus_error_set_const(ret_error, "org.dylangardner.DaemonFailure",
            "Daemon could not be loaded.");
        goto unlock_cleanup;
    } else {
        destroy_context(&backup);
    }

    _enforce_controls_on_class(NULL, &context->classes);
    r = sd_bus_send(NULL, reply, NULL);

unlock_cleanup:
    pthread_rwlock_unlock(&context_lock);
    sd_bus_error_set_errno(ret_error, r);
    sd_bus_message_unrefp(&reply);
    return r;
}

int method_evaluate(sd_bus_message* m, void* userdata, sd_bus_error* ret_error)
{
    Context* context = userdata;
    sd_bus_message* reply = NULL;

    int r = sd_bus_message_new_method_return(m, &reply);
    if (r < 0)
        return r;

    uid_t uid = 0;
    r = sd_bus_message_read(m, "u", &uid);
    if (r < 0)
        goto cleanup;

    pthread_rwlock_rdlock(&context_lock);
    ClassProperties props = { 0 };
    r = evaluate(uid, &context->classes, &props);
    if (r < 0)
        goto unlock_cleanup;
    if (r == 0) {
        sd_bus_error_setf(ret_error, "org.dylangardner.NoClassForUser",
            "No class found for the user.");
        r = -EINVAL;
        goto unlock_cleanup;
    }

    r = sd_bus_message_append_basic(reply, 's', props.filepath);
    if (r < 0)
        goto unlock_cleanup;
    r = sd_bus_send(NULL, reply, NULL);

unlock_cleanup:
    pthread_rwlock_unlock(&context_lock);

cleanup:
    sd_bus_error_set_errno(ret_error, r);
    sd_bus_message_unrefp(&reply);
    return r;
}

int method_set_property(sd_bus_message* m, void* userdata, sd_bus_error* ret_error)
{
    Context* context = userdata;
    sd_bus_message* reply = NULL;

    int r = sd_bus_message_new_method_return(m, &reply);
    if (r < 0)
        return r;

    char* given_classname = NULL;
    char* key = NULL;
    char* value = NULL;
    r = sd_bus_message_read(m, "sss", &given_classname, &key, &value);
    if (r < 0)
        goto cleanup;

    pthread_rwlock_wrlock(&context_lock);

    // Use classname.class instead of classname if .class extension is not given
    char* classname;
    r = complete_classname(given_classname, context->classext, &classname);
    if (r < 0) {
        r = -errno;
        goto unlock_cleanup;
    }
    bool is_alloc_classname = (r == 1);

    ClassProperties* props = get_hashmap_entry(&context->classes, classname);
    if (!props) {
        sd_bus_error_set_const(ret_error, "org.dylangardner.NoSuchClass",
            "No such class found (may need to daemon-reload).");
        r = -EINVAL;
        goto unlock_cleanup_classname;
    }

    add_hashmap_entry(&props->controls, key, value);

    printf("Enforcing resource controls on all users in %s\n", classname);
    _enforce_controls_on_class(props->filepath, &context->classes);
    r = sd_bus_send(NULL, reply, NULL);

unlock_cleanup_classname:
    if (is_alloc_classname)
        free(classname);

unlock_cleanup:
    pthread_rwlock_unlock(&context_lock);

cleanup:
    sd_bus_error_set_errno(ret_error, r);
    sd_bus_message_unrefp(&reply);
    return r;
}

/*
 * Fills the given vector with uids and a vector of to their corresponding
 * class. If there was an error, -1 is returned (and errno should be looked
 * up). Otherwise, 0 is returned.
 */
static int
_active_uids_and_class(Vector* uids, Vector* classes, HashMap* all_classes)
{
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* msg = NULL;
    sd_bus* bus = NULL;

    /* Connect to the system bus */
    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus to get active uids: %s\n",
            strerror(-r));
        goto cleanup;
    }

    r = sd_bus_call_method(
        bus, "org.freedesktop.login1", "/org/freedesktop/login1",
        "org.freedesktop.login1.Manager", "ListUsers", &error, &msg, NULL);
    if (r < 0) {
        fprintf(stderr, "Failed to get active uids: %s\n", error.message);

        goto cleanup;
    }

    r = sd_bus_message_enter_container(msg, SD_BUS_TYPE_ARRAY, "(uso)");
    if (r < 0) {
        fprintf(stderr, "Failed to get active uids: %s\n", strerror(-r));
        goto cleanup;
    }

    uid_t uid = 0;
    ClassProperties props = { 0 };
    while ((r = sd_bus_message_read(msg, "(uso)", &uid, NULL, NULL)) > 0) {
        if (evaluate(uid, all_classes, &props) < 1)
            continue;

        append_vector_item(uids, &uid);
        append_vector_item(classes, &props);
    }
    if (r < 0) {
        fprintf(stderr, "Failed to parse active uids: %s\n", strerror(-r));
        goto cleanup;
    }

    r = sd_bus_message_exit_container(msg);
    if (r < 0) {
        fprintf(stderr, "Failed to parse active uids: %s\n", strerror(-r));
        goto cleanup;
    }

cleanup:
    sd_bus_error_free(&error);
    sd_bus_unref(bus);
    return r;
}

int match_user_new(sd_bus_message* m, void* userdata, sd_bus_error* ret_error)
{
    Context* context = userdata;

    uid_t uid = 0;
    int r = sd_bus_message_read(m, "uo", &uid, NULL);
    if (r < 0)
        return r;

    pthread_rwlock_rdlock(&context_lock);
    ClassProperties props = { 0 };
    r = evaluate(uid, &context->classes, &props);
    if (r < 0)
        goto cleanup;

    // User has no class; ignore
    if (r == 0) {
        printf("%d belongs to no class. Ignoring.\n", uid);
        r = 0;
        goto cleanup;
    }
    r = _enforce_controls(uid, &props.controls);

cleanup:
    pthread_rwlock_unlock(&context_lock);
    sd_bus_error_set_errno(ret_error, r);
    return r;
}

/*
 * Enforces the given resource controls on the active users of the given
 * class. If the given class is NULL, every user's evaluated resource controls
 * are enforced. If there was an error, -1 is returned (and errno should be
 * looked up). Otherwise, 0 is returned.
 */
static int
_enforce_controls_on_class(const char* filepath, HashMap* classes)
{
    ClassProperties* evaluated_props = NULL;

    Vector active_uids = { 0 };
    Vector corresponding_classes = { 0 };
    create_vector(&active_uids, sizeof(uid_t));
    create_vector(&corresponding_classes, sizeof(ClassProperties));
    int r = _active_uids_and_class(&active_uids, &corresponding_classes, classes);
    if (r < 0)
        return -1;

    size_t nuids = get_vector_count(&active_uids);
    for (size_t n = 0; n < nuids; n++) {
        uid_t uid = *((uid_t*)get_vector_item(&active_uids, n));
        evaluated_props = get_vector_item(&corresponding_classes, n);
        if (filepath && strcmp(filepath, evaluated_props->filepath) != 0)
            continue;

        _enforce_controls(uid, &evaluated_props->controls);
    }
    destroy_vector(&active_uids);
    destroy_vector(&corresponding_classes);
    return 0;
}

/*
 * Enforces the given resource controls on a specific user. If there was an
 * error, -1 is returned (and errno should be looked up). Otherwise, 0 is
 * returned.
 */
static int
_enforce_controls(uid_t uid, HashMap* controls)
{
    int r = 0;

    printf("Enforcing resource controls on %d\n", uid);

    size_t ncontrols = get_hashmap_count(controls);
    if (ncontrols < 1)
        return 0;

    size_t argc_prefix = 3; // systemctl + set-property + unit_name
    size_t argc = argc_prefix + ncontrols; // + controls ...
    char** argv = malloc(sizeof *argv * (argc + 1)); // + NULL
    if (!argv)
        return -1;

    argv[0] = "systemctl";
    argv[1] = "set-property";
    char unit_name[24]; // 32 bit uid can only be at most 11 chars long
    snprintf(unit_name, 24, "user-%u.slice", uid);
    argv[2] = unit_name;

    char* key = NULL;
    char* value = NULL;
    size_t n = 0;
    for (; n < ncontrols; n++) {
        iter_hashmap(controls, &key, (void**)&value);
        int arglen = strlen(key) + strlen(value) + 2;
        char* arg = malloc(sizeof *arg * arglen);
        if (!arg) {
            r = -1;
            iter_hashmap_end(controls);
            goto cleanup;
        }

        snprintf(arg, arglen, "%s=%s", key, value);
        argv[argc_prefix + n] = arg;
    }
    iter_hashmap_end(controls);
    argv[argc] = NULL;

    pid_t pid = fork();
    if (pid == -1) {
        perror("Failed to fork and set property");
        r = -1;
        goto exec_cleanup;
    }
    if (pid == 0) {
        if (execv("/bin/systemctl", argv) == -1)
            errno_die("Failed to exec and set property");
    }

    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) == 0)
            goto exec_cleanup;

        for (int i = 0; argv[i]; i++)
            fprintf(stderr, "%s ", argv[i]);
        fprintf(stderr, "exited with non-zero status code: %d",
            WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        if (WTERMSIG(status) == 0)
            goto exec_cleanup;

        for (int i = 0; argv[i]; i++)
            fprintf(stderr, "%s ", argv[i]);
        fprintf(stderr, "recieved a signal: %s", strsignal(WTERMSIG(status)));
    }

exec_cleanup:
    for (size_t m = 0; m < n; m++)
        free(argv[argc_prefix + m]);

cleanup:
    free(argv);
    return r;
}
