#include "commands.h"
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int copy_file(const char *src, const char *dst)
{
    struct stat ss, ds;

    if (stat(src, &ss) < 0) {
        perror(src);
        return 1;
    }
    if (S_ISDIR(ss.st_mode)) {
        fprintf(stderr, "cp: omitting directory '%s'\n", src);
        return 1;
    }

    if (stat(dst, &ds) == 0 && ss.st_dev == ds.st_dev && ss.st_ino == ds.st_ino) {
        fprintf(stderr, "cp: '%s' and '%s' are the same file\n", src, dst);
        return 1;
    }

    int in = open(src, O_RDONLY);
    if (in < 0) {
        perror(src);
        return 1;
    }
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, ss.st_mode & 0777);
    if (out < 0) {
        perror(dst);
        close(in);
        return 1;
    }
    char buf[8192];
    int error = 0;
    ssize_t n;
    while ((n = read(in, buf, sizeof buf)) > 0) {
        ssize_t done = 0;
        while (done < n) {
            ssize_t w = write(out, buf + done, n - done);
            if (w < 0) {
                perror(dst);
                error = 1;
                break;
            }
            done += w;
        }
        if (error)
            break;
    }
    if (n < 0) {
        perror(src);
        error = 1;
    }

    close(in);
    close(out);
    return error;
}

int cmd_cp(int argc, char *const argv[])
{
    int i = 1;

    while (i < argc && argv[i][0] == '-' && argv[i][1]) {
        if (!strcmp(argv[i], "--")) {
            i++;
            break;
        }
        fputs("usage: cp [--] source dest | cp [--] source ... directory\n", stderr);
        return 2;
    }

    if (argc - i < 2) {
        fputs("usage: cp [--] source dest | cp [--] source ... directory\n", stderr);
        return 2;
    }

    const char *dest = argv[argc - 1];
    struct stat st;
    int dest_is_dir = stat(dest, &st) == 0 && S_ISDIR(st.st_mode);

    if (argc - i > 2 && !dest_is_dir) {
        fprintf(stderr, "cp: target '%s' is not a directory\n", dest);
        return 1;
    }

    int error = 0;
    for (; i < argc - 1; i++) {
        char path[PATH_MAX];
        int length;
        if (dest_is_dir) {
            const char *slash = strrchr(argv[i], '/');
            const char *name = slash ? slash + 1 : argv[i];
            length = snprintf(path, sizeof path, "%s/%s", dest, name);
        } else {
            length = snprintf(path, sizeof path, "%s", dest);
        }
        if (length < 0 || (size_t)length >= sizeof path) {
            fprintf(stderr, "cp: destination path is too long: %s\n", dest);
            error = 1;
            continue;
        }
        if (copy_file(argv[i], path))
            error = 1;
    }
    return error;
}
