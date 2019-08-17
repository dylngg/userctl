#define _GNU_SOURCE
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <grp.h>
#include <libgen.h>
#include <limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "macros.h"
#include "utils.h"
#include "commands.h"
#include "classparser.h"

#define STATUS_INDENT 10

char* _get_filepath(const char* restrict loc, char* restrict filename);
void _print_class(char* filepath);
void _print_class_status(ClassProperties* props, bool uids, bool gids);
void _print_status_user_line(uid_t* users, int nusers, bool print_uids);
void _print_status_group_line(gid_t* groups, int ngroups, bool print_gids);


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

static int help;

void list(int argc, char* argv[]) {
    assert(argc >= 0);  // No negative args
    assert(argv);  // At least empty

    int c;
    bool stop = false;
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
                stop = true;
                break;
            default:
                // Ignore weird things
                continue;
        }
    }
    // Abort, missing/wrong args
    if (stop) exit(1);

    // Quit after help
    if (help) {
        show_list_help();
        exit(0);
    }

    struct dirent** class_files = NULL;
    int num_files = 0;
    if (list_class_files(&class_files, &num_files) != 0) {
        char error_msg[MSG_BUFSIZE];
        snprintf(error_msg, sizeof error_msg,
                 "Error getting class files (%s/*%s)", default_loc,
                 default_ext);
        errno_die(error_msg);
    }

    for (int i = 0; i < num_files; i++) {
        if (class_files[i]) {
            char* filepath = _get_filepath(default_loc, class_files[i]->d_name);
            _print_class(filepath);
            free(filepath);
            free(class_files[i]);
        }
    }
    free(class_files);
}

/*
 * Returns a malloced filepath for the given filename at the given location.
 */
char* _get_filepath(const char* restrict loc, char* restrict filename) {
    char *filepath = malloc(strlen(loc) + strlen(filename) + 2);
    if (!filepath) malloc_error_exit();
    sprintf(filepath, "%s/%s", loc, filename);
    return filepath;
}

/*
 * Prints out the class.
 */
void _print_class(char* filepath) {
    // Some basename implementations destroy the string in the process
    char* filepath_copy = strdup(filepath);
    if (filepath_copy == NULL) {
        perror("Error printing classes");
        return;
    }
    char* base_name = basename(filepath);

    printf("%s (%s)\n", base_name, filepath_copy);
    free(filepath_copy);
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

    int c;
    uid_t uid;
    char* user;
    optopt = 0;
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
            default:
                // Ignore weird things
                continue;
        }
    }
    if (optopt != 0)
        exit(1);  // getopt already printed the error
    else if (help) {
        show_eval_help();
        exit(0);
    }
    else if (optind < argc) {
        user = argv[optind];
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
        user = pw->pw_name;
    }
    // pw->pw_name may be overwritten by another getpw* call, easier to dup argv too
    user = strdup(user);

    struct dirent** class_files = NULL;
    int num_files = 0;
    if (list_class_files(&class_files, &num_files) != 0) {
        char error_msg[MSG_BUFSIZE];
        snprintf(error_msg, sizeof error_msg,
                 "Error getting class files (%s/*%s)", default_loc,
                 default_ext);
        errno_die(error_msg);
    }

    assert(class_files);
    ClassProperties* props_list = malloc(sizeof *props_list * num_files);
    if (!props_list) malloc_error_exit();
    int nprops = 0;
    for (int i = 0; i < num_files; i++) {
        if (class_files[i]) {
            char* filepath = _get_filepath(default_loc, class_files[i]->d_name);
            if (parse_classfile(filepath, &props_list[nprops]) != -1) nprops++;
            free(filepath);
            free(class_files[i]);
        }
    }
    free(class_files);
    if (nprops < num_files) {
        ClassProperties* new_list = realloc(props_list, sizeof *new_list * nprops);
        props_list = new_list;
    }

    int index = -1;
    if (evaluate(uid, props_list, nprops, &index) == -1) {
        errno_die("Error evaluating user");
    }
    if (index == -1)
        printf("No classes found for %s\n", user);
    else
        _print_class(props_list[index].filepath);
    free(user);
    for (int i = 0; i < nprops; i++) destroy_class(&props_list[i]);
    free(props_list);
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

    static int c, print_gids, print_uids;
    optopt = 0;
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
            default:
                // Ignore weird things
                continue;
        }
    }
    if (optopt != 0)
        exit(1);  // getopt already printed the error
    else if (help) {
        show_status_help();
        exit(0);
    }
    else if (optind >= argc)
        die("No class given\n");

    char* classname = malloc(strlen(argv[optind]) + 1);
    strcpy(classname, argv[optind]);
    if (!valid_filename(classname)) die("Invalid classname given (no !@%^*~|/)\n");

    // Use classname.class instead of classname if .class extension is not given
    if (!has_ext(classname, (char*) default_ext)) {
        size_t new_size = strlen(classname) + strlen(default_ext) + 1;
        classname = realloc(classname, new_size);
        if (!classname) malloc_error_exit();
        strcat(classname, default_ext);
    }
    char* filepath = _get_filepath(default_loc, classname);
    free(classname);

    ClassProperties props_list;
    if (parse_classfile(filepath, &props_list) == -1) exit(1);
    _print_class_status(&props_list, print_uids, print_gids);

    destroy_class(&props_list);
    free(filepath);
}

/*
 * Prints the properties of the class. The users and groups fields contain
 * only those who exist.
 */
void _print_class_status(ClassProperties* props, bool print_uids, bool print_gids) {
    _print_class(props->filepath);
    _print_status_user_line(props->users, props->nusers, print_uids);
    _print_status_group_line(props->groups, props->ngroups, print_gids);

    char* shared_str = "false";
    if (props->shared) shared_str = "true";
    printf("%*s: %s\n", STATUS_INDENT, "Shared", shared_str);
    printf("%*s: %f\n", STATUS_INDENT, "Priority", props->priority);
}

/*
 * Prints the given users onto a line. If print_uids is true, the uids are not
 * converted to usernames. If a user isn't valid, they are ignored.
 */
void _print_status_user_line(uid_t* users, int nusers, bool print_uids) {
    printf("%*s: ", STATUS_INDENT, "Users");
    char* username;
    uid_t uid;
    for (int i = 0; i < nusers; i++) {
        uid = users[i];
        if (print_uids) {
            // Ignore invalid users
            if (!getpwuid(uid)) continue;
            printf("%llu", (unsigned long long) uid);
        }
        else {
            // Ignore invalid users
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
void _print_status_group_line(gid_t* groups, int ngroups, bool print_gids) {
    printf("%*s: ", STATUS_INDENT, "Groups");
    char* groupname;
    gid_t gid;
    for (int i = 0; i < ngroups; i++) {
        gid = groups[i];
        if (print_gids) {
            // Ignore invalid users
            if (!getgrgid(gid)) continue;
            printf("%llu", (unsigned long long) gid);
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
