#define _GNU_SOURCE
#include <assert.h>
#include <dirent.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "utils.h"
#include "commands.h"
#include "classparser.h"

char* _get_filepath(const char* restrict loc, char* restrict filename);
void _print_class(char* filepath);


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
    if (stop)
        exit(1);

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
        perror(error_msg);
        exit(1);
    }

    assert(class_files);
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
    char* user = NULL;
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
    else if (optind < argc)
        user = argv[optind];
    else
        die("No user given\n");

    uid_t uid;
    if (to_uid(user, &uid) == -1) die("No such user\n");

    struct dirent** class_files = NULL;
    int num_files = 0;
    if (list_class_files(&class_files, &num_files) != 0) {
        char error_msg[MSG_BUFSIZE];
        snprintf(error_msg, sizeof error_msg,
                 "Error getting class files (%s/*%s)", default_loc,
                 default_ext);
        perror(error_msg);
        exit(1);
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
        perror("Error evaluating user");
        exit(1);
    }
    if (index == -1)
        printf("No classes found for %s\n", user);
    else
        _print_class(props_list[index].filepath);
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

