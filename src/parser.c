#include "parser.h"

#include <ctype.h>
#include <string.h>

static int operator_char(int c)
{
    return c == '|' || c == '&' || c == '<' || c == '>';
}

static const char *read_word(const char **cursor, char **dest)
{
    const char *p = *cursor;
    while (*p && !isspace((unsigned char)*p) && !operator_char(*p)) {
        if (*p == '\'' || *p == '"') {
            char quote = *p++;
            while (*p && *p != quote) {
                if (quote == '"' && *p == '\\' &&
                    (p[1] == '"' || p[1] == '\\'))
                    p++;
                *(*dest)++ = *p++;
            }
            if (!*p)
                return "unclosed quote";
            p++;
        } else {
            if (*p == '\\' && !*++p)
                return "trailing escape";
            *(*dest)++ = *p++;
        }
    }
    *(*dest)++ = '\0';
    *cursor = p;
    return NULL;
}

const char *parse_line(const char *line, char *words, struct pipeline *job)
{
    job->count = 0;
    job->background = 0;
    struct command *cmd = &job->commands[0];
    int pending = 0;
    int started = 0;

    while (*line) {
        if (isspace((unsigned char)*line)) {
            line++;
            continue;
        }
        started = 1;
        if (operator_char(*line)) {
            if (pending)
                return "redirection requires a filename";
            int op = *line++;
            if (op == '<' || op == '>') {
                for (int i = 0; i < cmd->redirection_count; i++)
                    if (cmd->redirections[i].op == op)
                        return "duplicate redirection";
                pending = op;
            } else {
                if (!cmd->argc)
                    return "missing command";
                if (op == '&') {
                    while (isspace((unsigned char)*line))
                        line++;
                    if (*line)
                        return "& must be the last token";
                    job->background = 1;
                } else {
                    if (job->count + 1 == MAX_STAGES)
                        return "too many pipeline stages";
                    cmd = &job->commands[++job->count];
                }
            }
            continue;
        }

        char *word = words;
        const char *error = read_word(&line, &words);
        if (error)
            return error;
        if (pending) {
            cmd->redirections[cmd->redirection_count++] =
                (struct redirection){pending, word};
            pending = 0;
        } else {
            if (cmd->argc == MAX_ARGS)
                return "too many arguments";
            cmd->argv[cmd->argc++] = word;
            cmd->argv[cmd->argc] = NULL;
        }
    }
    if (pending)
        return "redirection requires a filename";
    if (started && !cmd->argc)
        return "missing command";
    if (started)
        job->count++;
    return NULL;
}
