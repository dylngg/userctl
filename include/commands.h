// SPDX-License-Identifier: GPL-3.0
#ifndef COMMANDS_H
#define COMMANDS_H
#include <dirent.h>
#include <stdbool.h>

typedef struct
{
    const char* cmd;
    void (*const dispatch)(int argc, char* argv[]);
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

/*
 * Reloads the daemon.
 */
void daemon_reload(int argc, char* argv[]);

/*
 * Prints out the help for the daemon-reload command.
 */
void show_daemon_reload_help();

/*
 * Sets a transient resource control on a class.
 */
void set_property(int argc, char* argv[]);

/*
 * Prints out the help for the set-property command.
 */
void show_set_property_help();

/*
 * Prints out the class file.
 */
void cat(int argc, char* argv[]);

/*
 * Prints out the help for the cat command.
 */
void show_cat_help();

/*
 * Opens up an editor for a class and reloads the class upon exit.
 */
void edit(int argc, char* argv[]);

/*
 * Prints out the help for the edit command.
 */
void show_edit_help();

#endif // COMMANDS_H
