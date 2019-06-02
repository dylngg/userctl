#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands.h"

typedef struct {
    const char* cmd;
    int (* const dispatch)(int argc, char* argv[]);
} Command;

int dispatch_cmd(int argc, char* argv[], const Command cmds[]);

/*
 * Runs a userctl command.
 */
int main(int argc, char* argv[]) {
    static const Command cmds[] = {
//        {"add-group", add_group}
//        {"add-user", add_user},
//        {"edit", edit},
//        {"eval-group", eval_group}
//        {"eval-user", eval_user}
        {"list", list},
//        {"set-property", set_property},
//        {"status", status},
        {}
    };
    return dispatch_cmd(argc, argv, cmds);
}

/*
 * Dispatches a command to the right function. Reports errors if the command
 * isn't in the given commands or if it doesn't exist and exits. Otherwise,
 * it returns the result of the dispatched function.
 */
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

