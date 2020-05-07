// SPDX-License-Identifier: GPL-3.0
#ifndef COMMANDS_H
#define COMMANDS_H
#include <stdbool.h>
#include <dirent.h>

typedef struct {
    const char* cmd;
    void (* const dispatch)(int argc, char* argv[]);
} Command;

/*
 * Dispatches a command to the right function. Reports errors if the command
 * isn't in the given commands or if it doesn't exist and exits.
 */
int dispatch_cmd(int argc, char* argv[], const Command cmds[]);

/*
 * Lists the possible classes.
 */
void list(int argc, char* argv[]);

/*
 * Prints out the help for the list command.
 */
void show_list_help();

/*
 * Evaluates a user for what class they are in.
 */
void eval(int argc, char* argv[]);

/*
 * Prints out the help for the eval command.
 */
void show_eval_help();

/*
 * Prints the properties of the class. The users and groups fields contain
 * only those who exist.
 */
void status(int argc, char* argv[]);

/*
 * Prints out the help for the status command.
 */
void show_status_help();

/*
 * Reloads the class.
 */
void reload(int argc, char* argv[]);

/*
 * Prints out the help for the reload command.
 */
void show_reload_help();


#endif // COMMANDS_H
