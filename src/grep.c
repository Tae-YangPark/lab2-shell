#include "commands.h"
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* 입력을 한 줄씩 읽어 정규식과 일치하는 줄을 출력한다. */
static int search(FILE *file, const char *name, regex_t *pattern, int prefix,
                  int *matched)
{
    char *line = NULL;
    size_t size = 0;
    ssize_t n;
    int error = 0;
    while ((n = getline(&line, &size, file)) >= 0) {
        if (regexec(pattern, line, 0, NULL, 0) == 0) {
            *matched = 1;
            if ((prefix && printf("%s:", name) < 0) ||
                fwrite(line, 1, (size_t)n, stdout) != (size_t)n ||
                (n > 0 && line[n - 1] != '\n' && putchar('\n') == EOF)) {
                perror("grep: stdout");
                error = 1;
                break;
            }
        }
    }
    if (ferror(file)) {
        perror(name);
        error = 1;
    }
    free(line);
    return error;
}

/* 기본 정규식으로 파일이나 표준 입력에서 일치하는 줄을 찾는다. */
int cmd_grep(int argc, char *const argv[])
{
    int i = 1, matched = 0, error = 0;
    if (i < argc && !strcmp(argv[i], "--"))
        i++;
    else if (i < argc && argv[i][0] == '-' && argv[i][1]) {
        fputs("usage: grep [--] pattern [file ...]\n", stderr);
        return 2;
    }
    if (i == argc) {
        fputs("usage: grep [--] pattern [file ...]\n", stderr);
        return 2;
    }
    regex_t pattern;
    int rc = regcomp(&pattern, argv[i++], REG_NOSUB);
    if (rc) {
        char message[256];
        regerror(rc, &pattern, message, sizeof message);
        fprintf(stderr, "grep: %s\n", message);
        return 2;
    }
    if (i == argc)
        error = search(stdin, "stdin", &pattern, 0, &matched);
    else {
        int prefix = argc - i > 1;
        for (; i < argc; i++) {
            FILE *file = !strcmp(argv[i], "-") ? stdin : fopen(argv[i], "r");
            if (!file) {
                perror(argv[i]);
                error = 1;
                continue;
            }
            error |= search(file, argv[i], &pattern, prefix, &matched);
            if (file != stdin && fclose(file) == EOF) {
                perror(argv[i]);
                error = 1;
            }
        }
    }
    regfree(&pattern);
    return error ? 2 : matched ? 0 : 1;
}
