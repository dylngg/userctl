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
#include "commands.h"
#include "classparser.h"


int dispatch_cmd(int argc, char* argv[], const Command cmds[]) {
    assert(cmds);  // Make sure cmds is not null
    assert(cmds[0].dispatch);  // Make sure there are at least 1 cmd given
    assert(argc >= 1);  // Make sure there's enough args
    assert(argv);  // Make sure we can actually look at argv

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
            return cmds[index].dispatch(argc, argv);
        }
        index++;
    }
    printf("%s is not a valid command\n", given_cmd);
    exit(1);
}

static int status_flag = 0;
static int help = 0;
static int stop = 0;

int list(int argc, char* argv[]) {
    assert(argc >= 0);  // No negative args
    assert(argv);  // At least empty

    int c;
    while(true) {
        static struct option long_options[] = {
          {"status", no_argument, &status_flag,1},
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
    // Abort, missing/wrong args
    if (stop)
        exit(1);

    // Quit after help
    if (help) {
        show_list_help();
        return 0;
    }

    struct dirent** class_files = NULL;
    int num_files = 0;
    if (list_class_files(&class_files, &num_files) != 0) {
        char error_msg[MSG_BUFSIZE];
        snprintf(error_msg, sizeof error_msg,
                 "Error getting class files (%s/*%s)", default_loc,
                 default_ext);
        perror(error_msg);
        return 1;
    }

    assert(class_files);
    for (int i = 0; i < num_files; i++) {
        if (class_files[i]) {
            print_class(class_files[i], status_flag);
            free(class_files[i]);
        }
    }
    free(class_files);
    return 0;
}

void print_class(struct dirent* classfile, bool status) {
    char* filename = classfile->d_name;

    // Some basename implementations destroy the string in the process
    char* filename_copy = strdup(filename);
    if (filename_copy == NULL) {
        perror("Error printing classes");
        return;
    }
    char* base_name = basename(filename);

    printf("%s (%s/%s)\n", base_name, default_loc, filename_copy);
    free(filename_copy);
    // TODO: Implement status flag
}

void show_list_help() {
    printf(
        "userctl list [OPTIONS...]\n\n"
        "List the classes available.\n\n"
        "  -h --help              Show this help\n"
        "     --status            Show the status of each class\n"
    );
}

