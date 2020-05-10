// SPDX-License-Identifier: GPL-3.0
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>

#include "controller.h"
#include "utils.h"

void show_help();
static void* class_enforcer(void* vargp);

static const sd_bus_vtable userctld_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Evaluate", "u", "s", method_evaluate, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetClass", "s", "sbdauau", method_get_class, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("ListClasses", NULL, "as", method_list_classes, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Reload", "s", NULL, method_reload_class, 0),
    SD_BUS_METHOD("DaemonReload", NULL, NULL, method_daemon_reload, 0),
    SD_BUS_PROPERTY("DefaultPath", "s", NULL, offsetof(Context, classdir), 0),
    SD_BUS_PROPERTY("DefaultExtension", "s", NULL, offsetof(Context, classext), 0),
    SD_BUS_VTABLE_END
};

static const char* service_path = "/org/dylangardner/userctl";
static const char* service_name = "org.dylangardner.userctl";

int main(int argc, char* argv[]) {
    pthread_t tid;
    sd_bus *bus = NULL;
    int r;

    Context* context = malloc(sizeof *context);
    if (!context) malloc_error_exit();
    if ((init_context(context)) < 0)
        errno_die("Failed to initialize userctld");

    pthread_rwlock_init(&context_lock, NULL);
    // TODO: When reload() method functionality is implemented, we'll want to
    // lock things on write.

    r = pthread_create(&tid, NULL, class_enforcer, context);
    if (r != 0) {
        fprintf(stderr, "Failed to spawn off class enforcer: %s\n", strerror(r));
        goto cleanup;
    }
    pthread_detach(tid);

    r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        goto cleanup;
    }

    r = sd_bus_add_object_vtable(
        bus,
        NULL,
        service_path,
        service_name,
        userctld_vtable,
        context
    );
    if (r < 0) {
        fprintf(stderr, "Failed to issue method call: %s\n", strerror(-r));
        goto cleanup;
    }

    r = sd_bus_request_name(bus, service_name, 0);
    if (r < 0) {
        fprintf(stderr, "Failed to acquire service name: %s\n", strerror(-r));
        goto cleanup;
    }

    for (;;) {
        r = sd_bus_process(bus, NULL);
        if (r < 0) {
            fprintf(stderr, "Failed to process bus: %s\n", strerror(-r));
            goto cleanup;
        }
        if (r > 0) continue;

        r = sd_bus_wait(bus, (uint64_t) -1);
        if (r < 0) {
            fprintf(stderr, "Failed to wait on bus: %s\n", strerror(-r));
            goto cleanup;
        }
    }

cleanup:
    pthread_rwlock_destroy(&context_lock);
    pthread_kill(tid, SIGKILL);
    destroy_context(context);
    free(context);
    sd_bus_unref(bus);
    return r < 0 ? 1 : 0;
}

/*
 * Listens to signals on dbus for when to apply classes on users.
 */
static void* class_enforcer(void* vargp) {
    assert(vargp);

    sd_bus* bus = NULL;
    sd_event* event = NULL;
    int r;
    Context* context = vargp;

    r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s", strerror(-r));
        return NULL;
    }

    r = sd_event_default(&event);
    if (r < 0) {
        fprintf(stderr, "Failed to set default event: %s", strerror(-r));
        goto cleanup;
    }

    r = sd_bus_attach_event(bus, event, SD_EVENT_PRIORITY_NORMAL);
    if (r < 0) {
        fprintf(stderr, "Failed to attach event loop: %s", strerror(-r));
        goto cleanup;
    }

    const char* signal_match = (
        "type='signal',"
        "sender='org.freedesktop.login1',"
        "path='/org/freedesktop/login1',"
        "interface='org.freedesktop.login1.Manager',"
        "member='UserNew'"
    );

    // In systemd 237+, sdbus has sd_bus_match_signal, but to remain
    // compatible with older versions we just use sd_bus_match
    r = sd_bus_add_match(
        bus,
        NULL,
        signal_match,
        match_user_new,
        context
    );

    if (r < 0) {
        fprintf(stderr, "Failed to watch for for new users: %s", strerror(-r));
        goto cleanup;
    }

    printf("Preparing to run event loop...\n");
    r = sd_event_loop(event);
    if (r < 0) {
        fprintf(stderr, "Failed to run event loop: %s", strerror(-r));
        goto cleanup;
    }

cleanup:
    sd_bus_flush_close_unref(bus);
    return NULL;
}


void show_help() {
    printf(
        "userctld [OPTIONS...]\n\n"
        "Sets configurable and persistent resource controls on users and groups.\n\n"
        "  -h --help\t\tShow this help.\n\n"
    );
}
