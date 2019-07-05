#ifndef COMMANDS_H
#define COMMANDS_H
#include <stdbool.h>
#include <dirent.h>

typedef struct {
    const char* cmd;
    int (* const dispatch)(int argc, char* argv[]);
} Command;

/*
 * Dispatches a command to the right function. Reports errors if the command
 * isn't in the given commands or if it doesn't exist and exits. Otherwise,
 * it returns the result of the dispatched function.
 */
int dispatch_cmd(int argc, char* argv[], const Command cmds[]);

/*
 * Lists the possible classes and either exits (and prints out a error) or
 * returns 0 if successful.
 */
int list(int argc, char* argv[]);

/*
 * Prints out the help for the list command.
 */
void show_list_help();

#endif // COMMANDS_H
