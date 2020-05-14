// SPDX-License-Identifier: GPL-3.0
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <systemd/sd-bus.h>

#include "vector.h"

typedef struct Context {
    Vector props_list;
    char* classdir;
    char* classext;
} Context;

pthread_rwlock_t context_lock;

/*
 * Initializes the context.
 */
int init_context(Context* context);

/*
 * Destroys the Context struct by deallocating things.
 */
void destroy_context(Context* context);

/*
 * Evaluates a uid for what class they are in.
 */
int method_evaluate(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

/*
 * Lists the path of the classes known.
 */
int method_list_classes(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

/*
 * Returns a class struct associated with the given classname.
 */
int method_get_class(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

/*
 * Reloads a class.
 */
int method_reload_class(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

/*
 * Reloads the daemon.
 */
int method_daemon_reload(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

/*
 * Sets a transient resource control on a class.
 */
int method_set_property(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

/*
 * Enforces a class on the new user.
 */
int match_user_new(sd_bus_message *m, void *userdata, sd_bus_error *error);

#endif // CONTROLLER_H
