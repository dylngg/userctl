#include "commands.h"

int main(int argc, char* argv[]) {
    // FIXME: Check if root or has correct capabilites
    static const Command cmds[] = {
//        {"add-group", add_group}
//        {"add-user", add_user},
//        {"edit", edit},
//        {"eval-group", eval_group}
//        {"eval-user", eval_user}
        {"list", list},
//        {"set-property", set_property},
//        {"status", status},
//        {"reload", reload},
        {0}
    };
    return dispatch_cmd(argc, argv, cmds);
}

