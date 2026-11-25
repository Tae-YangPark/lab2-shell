#include "commands.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* 전달받은 파일들을 삭제한다. 디렉터리는 삭제하지 않는다. */
int cmd_rm(int argc, char *const argv[])
{
    int i = 1, error = 0;
    if (i < argc && !strcmp(argv[i], "--"))
        i++;
    else if (i < argc && argv[i][0] == '-' && argv[i][1]) {
        fputs("usage: rm [--] file ...\n", stderr);
        return 2;
    }
    if (i == argc) {
        fputs("usage: rm [--] file ...\n", stderr);
        return 2;
    }
    for (; i < argc; i++) {
        if (unlink(argv[i]) < 0) {
            perror(argv[i]);
            error = 1;
        }
    }
    return error;
}
