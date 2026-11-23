#include "commands.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* 파일이나 디렉터리의 이름을 바꾸거나 대상 디렉터리로 옮긴다. */
int cmd_mv(int argc, char *const argv[])
{
    int i = 1, error = 0;
    if (i < argc && !strcmp(argv[i], "--"))
        i++;
    else if (i < argc && argv[i][0] == '-' && argv[i][1]) {
        fputs("usage: mv [--] source ... destination\n", stderr);
        return 2;
    }
    if (argc - i < 2) {
        fputs("usage: mv [--] source ... destination\n", stderr);
        return 2;
    }
    const char *dest = argv[argc - 1];
    struct stat st;
    int is_dir = stat(dest, &st) == 0 && S_ISDIR(st.st_mode);
    if (argc - i > 2 && !is_dir) {
        fprintf(stderr, "mv: target '%s' is not a directory\n", dest);
        return 1;
    }
    for (; i < argc - 1; i++) {
        char path[PATH_MAX];
        const char *target = dest;
        if (is_dir) {
            const char *name = strrchr(argv[i], '/');
            name = name ? name + 1 : argv[i];
            int n = snprintf(path, sizeof path, "%s/%s", dest, name);
            if (!*name || n < 0 || (size_t)n >= sizeof path) {
                fprintf(stderr, "mv: invalid or long path: %s\n", argv[i]);
                error = 1;
                continue;
            }
            target = path;
        }
        if (rename(argv[i], target) < 0) {
            perror(argv[i]);
            error = 1;
        }
    }
    return error;
}
