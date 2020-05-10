// SPDX-License-Identifier: GPL-3.0
#include <stdio.h>

#include "commands.h"

void show_help();

int main(int argc, char* argv[]) {
    static const Command cmds[] = {
//        {"add-group", add_group}
//        {"add-user", add_user},
//        {"edit", edit},
        {"eval", eval},
        {"-h", show_help},
        {"--help", show_help},
        {"list", list},
//        {"set-property", set_property},
        {"status", status},
        {"reload", reload},
        {"daemon-reload", daemon_reload},
        {0}
    };
    dispatch_cmd(argc, argv, cmds);
}

void show_help() {
    printf(
        "userctl {COMMAND} [OPTIONS...]\n\n"
        "Query or send commands to the userctld daemon.\n\n"
        "  -h --help\t\tShow this help.\n\n"
        "Commands:\n"
        "  eval\t\t\tEvaluates a user for what class they are in.\n"
        "  list\t\t\tList the possible classes.\n"
        "  status\t\tPrints the properties of the class.\n"
        "  reload\t\tReload the class.\n"
        "  daemon-reload\t\tReload the daemon.\n"
    );
}
