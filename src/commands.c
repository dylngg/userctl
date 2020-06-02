// SPDX-License-Identifier: GPL-3.0
#define _GNU_SOURCE // (basename)
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include "classparser.h"
#include "commands.h"
#include "macros.h"
#include "utils.h"

#define STATUS_INDENT 10

typedef struct Class {
    const char* filepath;
    bool shared;
    double priority;
    const uid_t* uids;
    size_t uids_size;
    const gid_t* gids;
    size_t gids_size;
} Class;

void _parse_no_args(int argc, char *argv[]);
void _print_class(const char* filepath);
void _print_class_status(Class* class, bool print_uids, bool print_gids);
void _print_status_user_line(const uid_t* users, int nusers, bool print_uids);
void _print_status_group_line(const gid_t* groups, int ngroups,
    bool print_gids);
int _reload_class(const char* classname);

static const char* service_path = "/org/dylangardner/userctl";
static const char* service_name = "org.dylangardner.userctl";
static int help;
static int stop;

int dispatch_cmd(int argc, char* argv[], const Command cmds[])
{
    assert(cmds); // Make sure cmds is not null
    assert(cmds[0].dispatch); // Make sure there are at least 1 cmd given

    if (argc < 2) {
        printf("No commands provided\n");
        exit(1);
    }

    const char* given_cmd = argv[1];
    int index = 0;
    while (cmds[index].dispatch) { // If at end of list
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

void _parse_no_args(int argc, char *argv[])
{
    while (true) {
        static struct option long_options[] = { { "help", no_argument, &help, 1 },
                                                { 0 } };

        int option_index = 0;
        int c = getopt_long(argc, argv, "h", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
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
}

void list(int argc, char* argv[])
{
    assert(argc >= 0); // No negative args
    assert(argv); // At least empty

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* msg = NULL;
    sd_bus* bus = NULL;

    _parse_no_args(argc, argv);

    // Abort, missing/wrong args (getopt will print errors out)
    if (stop)
        exit(1);

    if (help) {
        show_list_help();
        exit(0);
    }

    /* Connect to the system bus */
    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        goto cleanup;
    }

    r = sd_bus_call_method(bus, service_name, service_path, service_name,
        "ListClasses", &error, &msg, NULL);
    if (r < 0) {
        fprintf(stderr, "Internal error: Failed to get classes from userctld %s\n",
            error.message);
        goto cleanup;
    }
    char** classes = NULL;
    r = sd_bus_message_read_strv(msg, &classes);
    if (r < 0) {
        fprintf(stderr,
            "Internal error: Failed to parse classes from userctld %s\n",
            strerror(-r));
        goto cleanup;
    }

    for (int i = 0; classes[i] != NULL; i++) {
        _print_class(classes[i]);
        free(classes[i]);
    }
    free(classes);

cleanup:
    sd_bus_error_free(&error);
    sd_bus_unref(bus);
}

/*
 * Prints out the class.
 */
void _print_class(const char* filepath)
{
    // We're using GNU basename, which doesn't destroy the arg (string.h)
    printf("%s (%s)\n", basename(filepath), filepath);
}

void show_list_help()
{
    printf("userctl list [OPTIONS...]\n\n"
           "List the possible classes.\n\n"
           "  -h --help\t\tShow this help.\n\n");
}

void eval(int argc, char* argv[])
{
    assert(argc >= 0); // No negative args
    assert(argv); // At least empty

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* msg = NULL;
    sd_bus* bus = NULL;

    _parse_no_args(argc, argv);

    // Abort, missing/wrong args (getopt will print errors out)
    if (stop)
        exit(1);

    if (help) {
        show_eval_help();
        exit(0);
    }

    uid_t uid = 0;
    if (optind < argc) {
        const char* user = argv[optind];
        if (to_uid(user, &uid) == -1) {
            if (errno != 0)
                errno_die("");
            else
                die("No such user\n");
        }
    } else {
        uid = geteuid();
        errno = 0;
        struct passwd* pw = getpwuid(uid);
        if (!pw)
            errno_die("Failed to get passwd record of effective uid\n");
    }

    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        goto cleanup;
    }

    r = sd_bus_call_method(bus, service_name, service_path, service_name,
        "Evaluate", &error, &msg, "u", uid);
    if (r < 0) {
        fprintf(stderr, "%s\n", error.message);
        goto cleanup;
    }

    const char* filepath = NULL;
    r = sd_bus_message_read_basic(msg, 's', &filepath);
    if (r < 0) {
        fprintf(stderr, "Failed to parse class from userctld: %s\n", strerror(-r));
        goto cleanup;
    }
    _print_class(filepath);

cleanup:
    sd_bus_error_free(&error);
    sd_bus_unref(bus);
}

void show_eval_help()
{
    printf("userctl eval [OPTIONS...] [TARGET]\n\n"
           "Evaluates a user for what class they are in\n\n"
           "  -h --help\t\tShow this help\n");
}

void status(int argc, char* argv[])
{
    assert(argc >= 0); // No negative args
    assert(argv); // At least empty

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* msg = NULL;
    sd_bus* bus = NULL;
    static int print_gids, print_uids;

    while (true) {
        static struct option long_options[] = {
            { "uids", no_argument, &print_uids, 'u' },
            { "gids", no_argument, &print_gids, 'g' },
            { "help", no_argument, &help, 'h' },
            { 0 }
        };

        int option_index = 0;
        int c = getopt_long(argc, argv, "ghu", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
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
    if (stop)
        exit(1);

    if (help) {
        show_status_help();
        exit(0);
    }
    if (optind >= argc)
        die("No class given\n");

    const char* classname = argv[optind];
    bool alloc_classname = false;
    if (!has_ext(classname, ".class")) {
        alloc_classname = true;
        classname = add_ext(classname, ".class");
        if (!classname)
            errno_die("Failed to add .class to the end of the given classname");
    }

    /* Connect to the system bus */
    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        goto cleanup;
    }

    r = sd_bus_call_method(bus, service_name, service_path, service_name,
        "GetClass", &error, &msg, "s", classname);
    if (r < 0) {
        fprintf(stderr, "%s\n", error.message);
        goto cleanup;
    }

    Class class = { 0 };
    r = sd_bus_message_read(msg, "sbd", &class.filepath, &class.shared,
        &class.priority);
    if (r < 0) {
        fprintf(stderr, "Internal error: Failed to class status from userctl %s\n",
            strerror(-r));
        goto cleanup;
    }

    r = sd_bus_message_read_array(msg, 'u', (const void**)&class.uids,
        &class.uids_size);
    if (r < 0) {
        fprintf(stderr, "Internal error: Failed to parse uids in class status from "
                        "userctl %s\n",
            strerror(-r));
        goto cleanup;
    }

    r = sd_bus_message_read_array(msg, 'u', (const void**)&class.gids,
        &class.gids_size);
    if (r < 0) {
        fprintf(stderr, "Internal error: Failed to parse gids in class status from "
                        "userctl %s\n",
            strerror(-r));
        goto cleanup;
    }
    _print_class_status(&class, print_uids, print_gids);

cleanup:
    if (alloc_classname)
        free((char*) classname);

    sd_bus_error_free(&error);
    sd_bus_unref(bus);
}

/*
 * Prints the properties of the class. The users and groups fields contain
 * only those who exist.
 */
void _print_class_status(Class* class, bool print_uids, bool print_gids)
{
    _print_class(class->filepath);
    _print_status_user_line(class->uids, class->uids_size / sizeof *class->uids,
        print_uids);
    _print_status_group_line(class->gids, class->gids_size / sizeof *class->gids,
        print_gids);

    const char* shared_str = (class->shared) ? "true" : "false";
    printf("%*s: %s\n", STATUS_INDENT, "Shared", shared_str);
    printf("%*s: %lf\n", STATUS_INDENT, "Priority", class->priority);
}

/*
 * Prints the given users onto a line. If print_uids is true, the uids are not
 * converted to usernames. If a user isn't valid, they are ignored.
 */
void _print_status_user_line(const uid_t* users, int nusers, bool print_uids)
{
    assert(users);

    printf("%*s: ", STATUS_INDENT, "Users");

    const char* username = NULL;
    for (int i = 0; i < nusers; i++) {
        uid_t uid = (uid_t)users[i];

        if (print_uids) {
            if (!getpwuid(uid))
                continue;
            printf("%lu", (unsigned long)uid);
        } else {
            if (to_username(uid, &username) == -1)
                continue;
            printf("%s", username);
        }

        if (i != nusers - 1)
            printf(", ");
    }
    puts("");
}

/*
 * Prints the given groups onto a line. If print_gids is true, the gids are
 * not converted to groupnames. If a group isn't valid, they are ignored.
 */
void _print_status_group_line(const uid_t* groups, int ngroups, bool print_gids)
{
    assert(groups);
    printf("%*s: ", STATUS_INDENT, "Groups");

    const char* groupname = NULL;
    for (int i = 0; i < ngroups; i++) {
        gid_t gid = (gid_t)groups[i];

        if (print_gids) {
            // Ignore invalid users
            if (!getgrgid(gid))
                continue;
            printf("%lu", (unsigned long)gid);
        } else {
            // Ignore invalid users
            if (to_groupname(gid, &groupname) == -1)
                continue;
            printf("%s", groupname);
        }

        if (i != ngroups - 1)
            printf(", ");
    }
    puts("");
}

void show_status_help()
{
    printf("userctl status [OPTIONS...] [TARGET]\n\n"
           "Prints the properties of the class. The users and groups fields "
           "contain only\n"
           "those who exist.\n\n"
           "  -u --uids\t\tShow uids rather than usernames\n"
           "  -g --gids\t\tShow gids rather than groupnames\n"
           "  -h --help\t\tShow this help\n");
}

void reload(int argc, char* argv[])
{
    assert(argc >= 0); // No negative args
    assert(argv); // At least empty

    _parse_no_args(argc, argv);

    // Abort, missing/wrong args (getopt will print errors out)
    if (stop)
        exit(1);

    if (help) {
        show_reload_help();
        exit(0);
    }
    if (optind >= argc)
        die("No class given\n");

    if (_reload_class(argv[optind]) < 0)
        exit(1);
}

/*
 * Reloads either the daemon or a specific class, depending on whether the
 * classname is NULL or not.
 */
int _reload_class(const char* classname)
{
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus* bus = NULL;

    /* Connect to the system bus */
    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        goto cleanup;
    }

    if (classname)
        r = sd_bus_call_method(bus, service_name, service_path, service_name,
            "Reload", &error, NULL, "s", classname);
    else
        r = sd_bus_call_method(bus, service_name, service_path, service_name,
            "DaemonReload", &error, NULL, NULL);

    if (r < 0) {
        fprintf(stderr, "%s\n", error.message);
        goto cleanup;
    }

cleanup:
    sd_bus_error_free(&error);
    sd_bus_unref(bus);
    return r < 0 ? -1 : 0;
}

void show_reload_help()
{
    printf("userctl reload [OPTIONS...] [TARGET]\n\n"
           "Reload the class.\n\n"
           "  -h --help\t\tShow this help\n");
}

void daemon_reload(int argc, char* argv[])
{
    assert(argc >= 0); // No negative args
    assert(argv); // At least empty

    _parse_no_args(argc, argv);

    // Abort, missing/wrong args (getopt will print errors out)
    if (stop)
        exit(1);

    if (help) {
        show_daemon_reload_help();
        exit(0);
    }

    if (_reload_class(NULL) < 0)
        exit(1);
}

void show_daemon_reload_help()
{
    printf("userctl daemon-reload [OPTIONS...] \n\n"
           "Reload the daemon.\n\n"
           "  -h --help\t\tShow this help\n");
}

void set_property(int argc, char* argv[])
{
    assert(argc >= 0); // No negative args
    assert(argv); // At least empty

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* msg = NULL;
    sd_bus* bus = NULL;

    _parse_no_args(argc, argv);

    // Abort, missing/wrong args (getopt will print errors out)
    if (stop)
        exit(1);

    if (help) {
        show_set_property_help();
        exit(0);
    }

    int leftover_argc = argc - optind;
    if (leftover_argc < 1)
        die("No class given\n");
    else if (leftover_argc < 2)
        die("No resource controls given\n");

    const char* classname = argv[optind];
    bool alloc_classname = false;
    if (!has_ext(classname, ".class")) {
        alloc_classname = true;
        classname = add_ext(classname, ".class");
        if (!classname)
            errno_die("Failed to add .class to the end of the given classname");
    }

    char* resource_control = argv[optind + 1];

    // Soft error checking just to be nice

    if (strchr(resource_control, '=') == NULL)
        die("Resource control given does not contain an '='\n");

    char* key = NULL;
    char* value = NULL;
    if (parse_key_value(resource_control, &key, &value) < 0)
        die("Failed to parse key=value pair\n");

    /* Connect to the system bus */
    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        goto cleanup;
    }

    r = sd_bus_call_method(bus, service_name, service_path, service_name,
        "SetProperty", &error, &msg, "sss", classname, key,
        value);
    if (r < 0) {
        fprintf(stderr, "%s\n", error.message);
        goto cleanup;
    }

cleanup:
    if (alloc_classname)
        free((char*) classname);

    sd_bus_error_free(&error);
    sd_bus_unref(bus);
}

void show_set_property_help()
{
    printf("userctl set-property [OPTIONS...] [TARGET] [CONTROLS...]\n\n"
           "Sets a transient resource control on a class. For permanent "
           "controls you edit the class file.\n"
           "  -h --help\t\tShow this help\n");
}

void cat(int argc, char* argv[])
{
    assert(argc >= 0); // No negative args
    assert(argv); // At least empty

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* msg = NULL;
    sd_bus* bus = NULL;

    _parse_no_args(argc, argv);

    // Abort, missing/wrong args (getopt will print errors out)
    if (stop)
        exit(1);

    if (help) {
        show_cat_help();
        exit(0);
    }

    if (optind >= argc)
        die("No class given\n");

    /* Connect to the system bus */
    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        goto cleanup;
    }

    const char* filepath;
    for (int i = optind; i < argc; i++) {
        const char* classname = argv[i];
        bool alloc_classname = false;
        if (!has_ext(classname, ".class")) {
            alloc_classname = true;
            classname = add_ext(classname, ".class");
            if (!classname)
                errno_die("Failed to add .class to the end of the given classname");
        }
        r = sd_bus_call_method(bus, service_name, service_path, service_name,
            "GetClass", &error, &msg, "s", classname);
        if (alloc_classname)
            free((char*) classname);
        if (r < 0) {
            fprintf(stderr, "%s\n", error.message);
            continue;
        }

        r = sd_bus_message_read(msg, "s", &filepath);
        if (r < 0) {
            fprintf(stderr,
                "Internal error: Failed to parse class from userctld %s\n",
                strerror(-r));
            continue;
        }

        int fd = open(filepath, O_RDONLY);
        if (fd < 0) {
            perror("Failed to open class file");
            continue;
        }

        size_t bufsize = 8096;
        char buf[bufsize];
        for (;;) {
            r = read(fd, &buf, bufsize);
            if (r < 0) {
                perror("Failed to open class file");
                break;
            }
            if (r == 0)
                break;

            fputs(buf, stdout);
        }
        close(fd);
    }

cleanup:
    sd_bus_error_free(&error);
    sd_bus_unref(bus);
}

void show_cat_help()
{
    printf("userctl cat [OPTIONS...] [TARGET] \n\n"
           "Prints out the class file.\n"
           "  -h --help\t\tShow this help\n");
}

void edit(int argc, char* argv[])
{
    assert(argc >= 0); // No negative args
    assert(argv); // At least empty

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* msg = NULL;
    sd_bus* bus = NULL;

    _parse_no_args(argc, argv);

    // Abort, missing/wrong args (getopt will print errors out)
    if (stop)
        exit(1);

    if (help) {
        show_edit_help();
        exit(0);
    }

    if (optind >= argc)
        die("No class given\n");
    const char* classname = argv[optind];
    bool alloc_classname = false;
    if (!has_ext(classname, ".class")) {
        alloc_classname = true;
        classname = add_ext(classname, ".class");
        if (!classname)
            errno_die("Failed to add .class to the end of the given classname");
    }

    /* Connect to the system bus */
    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        goto cleanup;
    }

    r = sd_bus_call_method(bus, service_name, service_path, service_name,
        "GetClass", &error, &msg, "s", classname);
    if (r < 0) {
        fprintf(stderr, "%s\n", error.message);
        goto cleanup;
    }

    char* filepath = NULL;
    r = sd_bus_message_read(msg, "s", &filepath);
    if (r < 0) {
        fprintf(stderr, "Internal error: Failed to parse class from userctl %s\n",
            strerror(-r));
        goto cleanup;
    }
    if (access(filepath, W_OK)) {
        fprintf(stderr, "Cannot open %s for writing.\n", filepath);
        goto cleanup;
    }

    char* editor = secure_getenv("VISUAL");
    if (editor)
        goto exec;
    editor = secure_getenv("EDITOR");
    if (editor)
        goto exec;
    editor = "/usr/bin/vi";
    if (!access(editor, X_OK))
        goto exec; // Backwards, but correct
    die("Could not edit the given class. Set EDITOR or VISUAL.");

    struct stat classstat = { 0 };
exec:
    if (stat(filepath, &classstat) < 0) {
        fprintf(stderr, "Cannot stat %s.\n", filepath);
        goto cleanup;
    }
    int modtime = classstat.st_mtime;

    pid_t pid = fork();
    if (pid == -1) {
        perror("Failed to fork and edit class");
        r = -1;
        goto cleanup;
    }
    if (pid == 0) {
        char* editor_argv[3] = { editor, filepath, NULL };
        if (execv(editor, editor_argv) == -1) {
            perror("Failed to exec and edit class");
            goto cleanup;
        }
    }

    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        fprintf(stderr, "%s %s exited with non-zero status code: %d\n", editor,
            filepath, WEXITSTATUS(status));
    } else if (WIFSIGNALED(status) && WTERMSIG(status) != 0) {
        fprintf(stderr, "%s %s recieved a signal: %s\n", editor, filepath,
            strsignal(WTERMSIG(status)));
    }

    if (stat(filepath, &classstat) < 0)
        goto cleanup; // It may have been removed?
    if (classstat.st_mtime > modtime) {
        printf("Reloading %s\n", classname);
        _reload_class(classname);
    }

cleanup:
    if (alloc_classname)
        free((char*) classname);

    sd_bus_error_free(&error);
    sd_bus_unref(bus);
}

void show_edit_help()
{
    printf("userctl edit [OPTIONS...] [TARGET] \n\n"
           "Opens up an editor and reloads the class upon exit.\n"
           "  -h --help\t\tShow this help\n");
}
