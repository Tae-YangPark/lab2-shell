#ifndef LAB2_COMMANDS_H
#define LAB2_COMMANDS_H

/* argv[0] is the command name, argv[argc] is NULL.
 * Return 0 for success, 1 for an operational error, 2 for invalid arguments.
 * Use stdin/stdout/stderr and return to the caller; do not call exit(). */
typedef int (*command_fn)(int argc, char *const argv[]);

struct command_entry {
    const char *name;
    command_fn run;
};

int cmd_ls(int argc, char *const argv[]);
int cmd_pwd(int argc, char *const argv[]);
int cmd_cd(int argc, char *const argv[]);

#endif
