#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands.h"

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

