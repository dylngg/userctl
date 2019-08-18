#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <systemd/sd-bus.h>

#include "controller.h"
#include "utils.h"

void show_help();

static const sd_bus_vtable userctld_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("GetClass", "s", "sbdauau", method_get_class, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("ListClasses", NULL, "as", method_list_classes, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_PROPERTY("DefaultPath", "s", NULL, offsetof(Context, classdir), 0),
    SD_BUS_PROPERTY("DefaultExtension", "s", NULL, offsetof(Context, classext), 0),
    SD_BUS_VTABLE_END
};

int main(int argc, char* argv[]) {
    sd_bus *bus = NULL;

    Context* context = malloc(sizeof *context);
    if (!context) malloc_error_exit();
    if (init_context(context) < 0)
        errno_die("Failed to initialize userctld");

    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        goto death;
    }
    const char* service_path = "/org/dylangardner/userctl";
    const char* service_name = "org.dylangardner.userctl";

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
        goto death;
    }

    r = sd_bus_request_name(bus, service_name, 0);
    if (r < 0) {
        fprintf(stderr, "Failed to acquire service name: %s\n", strerror(-r));
        goto death;
    }

    for (;;) {
        r = sd_bus_process(bus, NULL);
        if (r < 0) {
            fprintf(stderr, "Failed to process bus: %s\n", strerror(-r));
            goto death;
        }
        if (r > 0) continue;

        r = sd_bus_wait(bus, (uint64_t) -1);
        if (r < 0) {
            fprintf(stderr, "Failed to wait on bus: %s\n", strerror(-r));
            goto death;
        }
    }

death:
    destroy_context(context);
    free(context);
    sd_bus_unref(bus);
    return r < 0 ? 1 : 0;
}

void show_help() {
    printf(
        "userctld [OPTIONS...]\n\n"
        "Sets configurable and persistent resource controls on users and groups.\n\n"
        "  -h --help\t\tShow this help.\n\n"
    );
}
