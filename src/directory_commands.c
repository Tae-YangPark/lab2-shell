#include "commands.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int cmd_pwd(int argc, char *const argv[])
{
    (void)argv;
    if (argc != 1) {
        fputs("usage: pwd\n", stderr);
        return 2;
    }
    size_t size = 256;
    for (;;) {
        char *path = malloc(size);
        if (!path) {
            perror("pwd");
            return 1;
        }
        if (getcwd(path, size)) {
            int rc = puts(path);
            free(path);
            return rc == EOF;
        }
        int error = errno;
        free(path);
        if (error != ERANGE || size > (size_t)-1 / 2) {
            errno = error;
            perror("pwd");
            return 1;
        }
        size *= 2;
    }
}

int cmd_cd(int argc, char *const argv[])
{
    if (argc > 2) {
        fputs("usage: cd [directory]\n", stderr);
        return 2;
    }
    const char *path = argc == 2 ? argv[1] : getenv("HOME");
    if (!path || !*path) {
        fputs("cd: directory is empty or HOME is unset\n", stderr);
        return 1;
    }
    if (chdir(path) < 0) {
        perror(path);
        return 1;
    }
    return 0;
}

static int list_path(const char *path, int all, int heading)
{
    struct stat st;
    if (stat(path, &st) < 0) {
        perror(path);
        return 1;
    }
    if (!S_ISDIR(st.st_mode))
        return puts(path) == EOF;

    struct dirent **entries;
    int count = scandir(path, &entries, NULL, alphasort);
    if (count < 0) {
        perror(path);
        return 1;
    }
    int error = heading && printf("%s:\n", path) < 0;
    for (int i = 0; i < count; i++) {
        const char *name = entries[i]->d_name;
        if ((all || *name != '.') && puts(name) == EOF)
            error = 1;
        free(entries[i]);
    }
    free(entries);
    return error;
}

int cmd_ls(int argc, char *const argv[])
{
    int all = 0;
    int i = 1;
    while (i < argc && argv[i][0] == '-' && argv[i][1]) {
        if (!strcmp(argv[i], "--")) {
            i++;
            break;
        }
        if (strcmp(argv[i++], "-a")) {
            fputs("usage: ls [-a] [--] [path ...]\n", stderr);
            return 2;
        }
        all = 1;
    }
    if (i == argc)
        return list_path(".", all, 0);
    int multi = argc - i > 1;
    int error = 0;
    for (; i < argc; i++) {
        error |= list_path(argv[i], all, multi);
        if (multi && i + 1 < argc && putchar('\n') == EOF)
            error = 1;
    }
    return error;
}
