#ifndef LAB2_PARSER_H
#define LAB2_PARSER_H

#define MAX_LINE 8192
#define MAX_ARGS 64
#define MAX_STAGES 16

struct redirection {
    int op;
    const char *path;
};

struct command {
    int argc;
    char *argv[MAX_ARGS + 1];
    int redirection_count;
    struct redirection redirections[2];
};

struct pipeline {
    int count;
    int background;
    struct command commands[MAX_STAGES];
};

/* job must be zero initialized, words must have strlen(line) + 1 bytes. */
const char *parse_line(const char *line, char *words, struct pipeline *job);

#endif
