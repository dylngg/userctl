#ifndef COMMANDS_H
#define COMMANDS_H
#include <stdbool.h>
#include <dirent.h>

/*
 * Lists the possible classes and either exits (and prints out a error) or
 * returns 0 if successful.
 */
int list(int argc, char* argv[]);

/*
 * Prints out the class. If status is true, then the full status of the class
 * is printed out.
 */
void print_class(struct dirent* classfile, bool status);

/*
 * Prints out the help for the list command.
 */
void show_list_help();

#endif // COMMANDS_H
