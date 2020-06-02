// SPDX-License-Identifier: GPL-3.0
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <systemd/sd-bus.h>

#include "controller.h"

static void* class_enforcer(void* vargp);

static const sd_bus_vtable userctld_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Evaluate", "u", "s", method_evaluate, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetClass", "s", "sbdauau", method_get_class, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("ListClasses", NULL, "as", method_list_classes, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Reload", "s", NULL, method_reload_class, 0),
    SD_BUS_METHOD("DaemonReload", NULL, NULL, method_daemon_reload, 0),
    SD_BUS_METHOD("SetProperty", "sss", NULL, method_set_property, 0),
    SD_BUS_PROPERTY("DefaultPath", "s", NULL, offsetof(Context, classdir), 0),
    SD_BUS_PROPERTY("DefaultExtension", "s", NULL, offsetof(Context, classext), 0),
    SD_BUS_VTABLE_END
};

static const char* service_path = "/org/dylangardner/userctl";
static const char* service_name = "org.dylangardner.userctl";

void parse_args(int argc, char* argv[])
{
    static int version, help, stop, debug;

    while (true) {
        static struct option long_options[] = {
            { "debug", no_argument, &debug, 'd' },
            { "help", no_argument, &help, 'h' },
            { "version", no_argument, &version, 'v' },
            { 0 }
        };

        int option_index = 0;
        int c = getopt_long(argc, argv, "dhv", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 'd':
            debug = 1;
            break;
        case 'v':
            version = 1;
            break;
        case 'h':
            help = 1;
            break;
        case '?':
            stop = 1;
            break;
        default:
            continue;
        }
    }

    // Abort, missing/wrong args (getopt will print errors out)
    if (stop)
        exit(1);

    if (help) {
        printf("userctld [OPTIONS...]\n\n"
               "Sets configurable and persistent resource controls on users and "
               "groups.\n\n"
               "  -d --debug\t\tDebugging verbosity is turned on and sent to stderr.\n"
               "  -h --help\t\tShow this help.\n"
               "  -v --version\t\tPrint version and exit.\n\n");
        exit(0);
    }
    if (version) {
        printf("userctld v0.0.1\n");
        exit(0);
    }
    if (debug) {
        setlogmask(LOG_UPTO(LOG_DEBUG));
        openlog(NULL, LOG_PERROR | LOG_CONS | LOG_PID | LOG_NDELAY, LOG_DAEMON);
    }
}

int main(int argc, char* argv[])
{
    setlogmask(LOG_UPTO(LOG_NOTICE));
    openlog(NULL, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_DAEMON);

    parse_args(argc, argv);

    Context* context = malloc(sizeof *context);
    if (!context || init_context(context) < 0)
        syslog(LOG_ERR, "Failed to initialize userctld");

    pthread_rwlock_init(&context_lock, NULL);

    pthread_t tid = 0;
    int r = pthread_create(&tid, NULL, class_enforcer, context);
    if (r != 0) {
        syslog(LOG_ERR, "Failed to spawn off class enforcer: %s\n", strerror(r));
        goto cleanup;
    }
    pthread_detach(tid);

    sd_bus* bus = NULL;
    r = sd_bus_open_system(&bus);
    if (r < 0) {
        syslog(LOG_ERR, "Failed to connect to system bus: %s\n", strerror(-r));
        goto cleanup;
    }

    r = sd_bus_add_object_vtable(bus, NULL, service_path, service_name,
        userctld_vtable, context);
    if (r < 0) {
        syslog(LOG_ERR, "Failed to issue method call: %s\n", strerror(-r));
        goto cleanup;
    }

    r = sd_bus_request_name(bus, service_name, 0);
    if (r < 0) {
        syslog(LOG_ERR, "Failed to acquire service name: %s\n", strerror(-r));
        goto cleanup;
    }

    syslog(LOG_NOTICE, "Daemon has started.");
    for (;;) {
        r = sd_bus_process(bus, NULL);
        if (r < 0) {
            syslog(LOG_ERR, "Failed to process bus: %s\n", strerror(-r));
            goto cleanup;
        }
        if (r > 0)
            continue;

        r = sd_bus_wait(bus, (uint64_t)-1);
        if (r < 0) {
            syslog(LOG_ERR, "Failed to wait on bus: %s\n", strerror(-r));
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
static void*
class_enforcer(void* vargp)
{
    assert(vargp);

    sd_bus* bus = NULL;
    sd_event* event = NULL;
    Context* context = vargp;

    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        syslog(LOG_ERR, "Failed to connect to system bus: %s", strerror(-r));
        return NULL;
    }

    r = sd_event_default(&event);
    if (r < 0) {
        syslog(LOG_ERR, "Failed to set default event: %s", strerror(-r));
        goto cleanup;
    }

    r = sd_bus_attach_event(bus, event, SD_EVENT_PRIORITY_NORMAL);
    if (r < 0) {
        syslog(LOG_ERR, "Failed to attach event loop: %s", strerror(-r));
        goto cleanup;
    }

    const char* signal_match = ("type='signal',"
                                "sender='org.freedesktop.login1',"
                                "path='/org/freedesktop/login1',"
                                "interface='org.freedesktop.login1.Manager',"
                                "member='UserNew'");

    // In systemd 237+, sdbus has sd_bus_match_signal, but to remain
    // compatible with older versions we just use sd_bus_match
    r = sd_bus_add_match(bus, NULL, signal_match, match_user_new, context);

    if (r < 0) {
        syslog(LOG_ERR, "Failed to watch for for new users: %s", strerror(-r));
        goto cleanup;
    }

    syslog(LOG_INFO, "Running class enforcer event loop...");
    r = sd_event_loop(event);
    if (r < 0) {
        syslog(LOG_ERR, "Failed to run event loop: %s", strerror(-r));
        goto cleanup;
    }

cleanup:
    sd_bus_flush_close_unref(bus);
    return NULL;
}
