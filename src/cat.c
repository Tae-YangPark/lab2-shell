#include "commands.h"
#include <stdio.h>
#include <string.h>

/* 열린 입력의 내용을 끝까지 읽어 표준 출력으로 보낸다. */
static int print_file(FILE *file, const char *name)
{
    char buffer[8192];
    size_t n;
    while ((n = fread(buffer, 1, sizeof buffer, file)) > 0) {
        if (fwrite(buffer, 1, n, stdout) != n) {
            perror("cat: stdout");
            return 1;
        }
    }
    if (ferror(file)) {
        perror(name);
        return 1;
    }
    return 0;
}

/* 파일을 순서대로 출력한다. 파일이 없거나 '-'이면 표준 입력을 읽는다. */
int cmd_cat(int argc, char *const argv[])
{
    int i = 1, error = 0;
    if (i < argc && !strcmp(argv[i], "--"))
        i++;
    else if (i < argc && argv[i][0] == '-' && argv[i][1]) {
        fputs("usage: cat [--] [file ...]\n", stderr);
        return 2;
    }
    if (i == argc)
        return print_file(stdin, "stdin");
    for (; i < argc; i++) {
        FILE *file = !strcmp(argv[i], "-") ? stdin : fopen(argv[i], "rb");
        if (!file) {
            perror(argv[i]);
            error = 1;
            continue;
        }
        error |= print_file(file, argv[i]);
        if (file != stdin && fclose(file) == EOF) {
            perror(argv[i]);
            error = 1;
        }
    }
    return error;
}
