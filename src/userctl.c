#include <stdio.h>

#include "commands.h"

void show_help();

int main(int argc, char* argv[]) {
    // FIXME: Check if root or has correct capabilites
    static const Command cmds[] = {
//        {"add-group", add_group}
//        {"add-user", add_user},
//        {"edit", edit},
//        {"eval-group", eval_group}
//        {"eval-user", eval_user}
        {"-h", show_help},
        {"--help", show_help},
        {"list", list},
//        {"set-property", set_property},
//        {"status", status},
//        {"reload", reload},
        {0}
    };
    dispatch_cmd(argc, argv, cmds);
}

void show_help() {
    printf("userctl {COMMAND} [OPTIONS...]\n\n"
"Sets configurable and persistent resource controls on users and groups.\n\n"
"  -h --help\t\tShow this help.\n\n"
"Commands:\n"
"  list\t\t\tList the classes available.\n");
}
