#define _GNU_SOURCE  // (basename)
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include "macros.h"
#include "utils.h"
#include "commands.h"

#define STATUS_INDENT 10

typedef struct Class {
    char* filepath;
    bool shared;
    double priority;
    uint32_t* uids;
    uint32_t* gids;
} Class;

void _print_class(char* filepath);
void _print_class_status(Class* class, int nuids, int ngids, bool print_uids, bool print_gids);
void _print_status_user_line(uint32_t* users, int nusers, bool print_uids);
void _print_status_group_line(uint32_t* groups, int ngroups, bool print_gids);


static const char* service_path = "/org/dylangardner/userctl";
static const char* service_name = "org.dylangardner.userctl";
static int help;
static int stop;

int dispatch_cmd(int argc, char* argv[], const Command cmds[]) {
    assert(cmds);  // Make sure cmds is not null
    assert(cmds[0].dispatch);  // Make sure there are at least 1 cmd given

    if (argc < 2) {
        printf("No commands provided\n");
        exit(1);
    }

    char* given_cmd = argv[1];
    int index = 0;
    while(cmds[index].dispatch) {  // If at end of list
        if (strcmp(given_cmd, cmds[index].cmd) == 0) {
            // Remove "userctl cmd" from argv
            argc -= 1;
            argv += 1;
            cmds[index].dispatch(argc, argv);
            exit(0);
        }
        index++;
    }
    printf("%s is not a valid command\n", given_cmd);
    exit(1);
}

void list(int argc, char* argv[]) {
    assert(argc >= 0);  // No negative args
    assert(argv);  // At least empty

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* msg = NULL;
    sd_bus* bus = NULL;
    char** classes = NULL;
    int c, r;

    while(true) {
        static struct option long_options[] = {
            {"help", no_argument, &help, 1},
            {0}
        };

        int option_index = 0;
        c = getopt_long(argc, argv, "h", long_options, &option_index);
        if (c == -1) break;
        switch(c) {
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
    if (stop) exit(1);

    if (help) {
        show_list_help();
        exit(0);
    }

    /* Connect to the system bus */
    r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        goto death;
    }

    r = sd_bus_call_method(
        bus,
        service_name,
        service_path,
        service_name,
        "ListClasses",
        &error,
        &msg,
        NULL
    );
    if (r < 0) {
        fprintf(stderr, "Internal error: Failed to get classes from userctld %s\n",
                error.message);
        goto death;
    }
    r = sd_bus_message_read_strv(msg, &classes);
    if (r < 0) {
        fprintf(stderr, "Internal error: Failed to parse classes from userctld %s\n",
                strerror(-r));
        goto death;
    }

    for (int i = 0; classes[i] != NULL; i++) {
        _print_class(classes[i]);
        free(classes[i]);
    }
    free(classes);

death:
    sd_bus_error_free(&error);
    sd_bus_unref(bus);
}

/*
 * Prints out the class.
 */
void _print_class(char* filepath) {
    // We're using GNU basename, which doesn't destroy the arg (string.h)
    printf("%s (%s)\n", basename(filepath), filepath);
}

void show_list_help() {
    printf(
        "userctl list [OPTIONS...]\n\n"
        "List the possible classes.\n\n"
        "  -h --help\t\tShow this help.\n\n"
    );
}

void eval(int argc, char* argv[]) {
    assert(argc >= 0);  // No negative args
    assert(argv);  // At least empty

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* msg = NULL;
    sd_bus* bus = NULL;
    char* filepath;
    int c, r;
    uid_t uid;

    while(true) {
        static struct option long_options[] = {
            {"help", no_argument, &help, 1},
            {0}
        };

        int option_index = 0;
        c = getopt_long(argc, argv, "h", long_options, &option_index);
        if (c == -1) break;
        switch(c) {
            case 'h':
                help = 1;
                break;
            case '?':
                stop = 1;
                break;
            default:
                // Ignore weird things
                continue;
        }
    }
    // Abort, missing/wrong args (getopt will print errors out)
    if (stop) exit(1);

    if (help) {
        show_eval_help();
        exit(0);
    }
    else if (optind < argc) {
        char* user = argv[optind];
        if (to_uid(user, &uid) == -1) {
            if (errno != 0) errno_die("");
            else die("No such user\n");
        }
    }
    else {
        uid = geteuid();
        errno = 0;
        struct passwd* pw = getpwuid(uid);
        if (!pw) errno_die("Failed to get passwd record of effective uid");
    }

    r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        goto death;
    }

    r = sd_bus_call_method(
        bus,
        service_name,
        service_path,
        service_name,
        "Evaluate",
        &error,
        &msg,
        "u",
        (uint32_t) uid
    );
    if (r < 0) {
        fprintf(stderr, "%s\n", error.message);
        goto death;
    }

    r = sd_bus_message_read_basic(msg, 's', &filepath);
    if (r < 0) {
        fprintf(stderr, "Failed to parse class from userctld: %s\n", strerror(-r));
        goto death;
    }
    _print_class(filepath);

death:
    sd_bus_error_free(&error);
    sd_bus_unref(bus);
}

void show_eval_help() {
    printf(
        "userctl eval [OPTIONS...] [TARGET]\n\n"
        "Evaluates a user for what class they are in\n\n"
        "  -h --help\t\tShow this help\n"
    );
}

void status(int argc, char* argv[]) {
    assert(argc >= 0);  // No negative args
    assert(argv);  // At least empty

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* msg = NULL;
    sd_bus* bus = NULL;
    char* classname = NULL;
    Class class = {0};
    static int c, r, print_gids, print_uids, nuids, ngids;
    size_t uids_size, gids_size;

    while(true) {
        static struct option long_options[] = {
            {"uids", no_argument, &print_uids, 'u'},
            {"gids", no_argument, &print_gids, 'g'},
            {"help", no_argument, &help, 'h'},
            {0}
        };

        int option_index = 0;
        c = getopt_long(argc, argv, "ghu", long_options, &option_index);
        if (c == -1) break;
        switch(c) {
            case 'u':
                print_uids = 1;
                break;
            case 'g':
                print_gids = 1;
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
    if (stop) exit(1);

    if (help) {
        show_status_help();
        exit(0);
    }
    if (optind >= argc)
        die("No class given\n");
    classname = argv[optind];

    /* Connect to the system bus */
    r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        goto death;
    }

    r = sd_bus_call_method(
        bus,
        service_name,
        service_path,
        service_name,
        "GetClass",
        &error,
        &msg,
        "s",
        classname
    );
    if (r < 0) {
        fprintf(stderr, "%s\n", error.message);
        goto death;
    }
    r = sd_bus_message_read(
        msg, "sbd",
        &class.filepath,
        &class.shared,
        &class.priority
    );
    if (r < 0) {
        fprintf(stderr, "Internal error: Failed to class status from userctl %s\n",
                strerror(-r));
        goto death;
    }

    r = sd_bus_message_read_array(msg, 'u', (const void**) &class.uids, &uids_size);
    if (r < 0) {
        fprintf(stderr, "Internal error: Failed to parse uids in class status from userctl %s\n",
                strerror(-r));
        goto death;
    }
    nuids = uids_size / sizeof(*class.uids);

    r = sd_bus_message_read_array(msg, 'u', (const void**) &class.gids, &gids_size);
    if (r < 0) {
        fprintf(stderr, "Internal error: Failed to parse gids in class status from userctl %s\n",
                strerror(-r));
        goto death;
    }
    ngids = gids_size / sizeof(*class.gids);
    _print_class_status(&class, nuids, ngids, print_uids, print_gids);

death:
    sd_bus_error_free(&error);
    sd_bus_unref(bus);
}

/*
 * Prints the properties of the class. The users and groups fields contain
 * only those who exist.
 */
void _print_class_status(Class* class, int nuids, int ngids, bool print_uids, bool print_gids) {
    _print_class(class->filepath);
    _print_status_user_line(class->uids, nuids, print_uids);
    _print_status_group_line(class->gids, ngids, print_gids);

    char* shared_str = (class->shared) ? "true": "false";
    printf("%*s: %s\n", STATUS_INDENT, "Shared", shared_str);
    printf("%*s: %lf\n", STATUS_INDENT, "Priority", class->priority);
}


/*
 * Prints the given users onto a line. If print_uids is true, the uids are not
 * converted to usernames. If a user isn't valid, they are ignored.
 */
void _print_status_user_line(uint32_t* users, int nusers, bool print_uids) {
    printf("%*s: ", STATUS_INDENT, "Users");
    char* username;
    uid_t uid;

    for (int i = 0; i < nusers; i++) {
        uid = (uid_t) users[i];

        if (print_uids) {
            if (!getpwuid(uid)) continue;
            printf("%lu", (unsigned long) uid);
        }

        else {
            if (to_username(uid, &username) == -1) continue;
            printf("%s", username);
        }

        if (i != nusers - 1) printf(", ");
    }
    puts("");
}


/*
 * Prints the given groups onto a line. If print_gids is true, the gids are
 * not converted to groupnames. If a group isn't valid, they are ignored.
 */
void _print_status_group_line(uint32_t* groups, int ngroups, bool print_gids) {
    printf("%*s: ", STATUS_INDENT, "Groups");
    char* groupname;
    gid_t gid;

    for (int i = 0; i < ngroups; i++) {
        gid = (gid_t) groups[i];

        if (print_gids) {
            // Ignore invalid users
            if (!getgrgid(gid)) continue;
            printf("%lu", (unsigned long) gid);
        }

        else {
            // Ignore invalid users
            if (to_groupname(gid, &groupname) == -1) continue;
            printf("%s", groupname);
        }

        if (i != ngroups - 1) printf(", ");
    }
    puts("");
}

void show_status_help() {
    printf(
        "userctl status [OPTIONS...] [TARGET]\n\n"
        "Prints the properties of the class. The users and groups fields contain only\n"
        "those who exist.\n\n"
        "  -u --uids\t\tShow uids rather than usernames\n"
        "  -g --gids\t\tShow gids rather than groupnames\n"
        "  -h --help\t\tShow this help\n"
    );
}
