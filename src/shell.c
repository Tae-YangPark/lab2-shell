#include "commands.h"
#include "parser.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

static const struct command_entry builtins[] = {
    {"ls", cmd_ls},
    {"pwd", cmd_pwd},
    {"cd", cmd_cd},
    {"mkdir", cmd_mkdir},
    {"rmdir", cmd_rmdir},
    {"ln", cmd_ln},
    {"cp", cmd_cp},
};

static int interactive;
static pid_t shell_group;
static volatile sig_atomic_t interrupted;

static void on_interrupt(int signo)
{
    (void)signo;
    interrupted = 1;
}

static void reap_background(int signo)
{
    (void)signo;
    int saved = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
    errno = saved;
}

static int set_handler(int signo, void (*handler)(int), int flags)
{
    struct sigaction sa = {.sa_handler = handler, .sa_flags = flags};
    sigemptyset(&sa.sa_mask);
    return sigaction(signo, &sa, NULL);
}

static command_fn find_builtin(const char *name)
{
    for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++)
        if (!strcmp(name, builtins[i].name))
            return builtins[i].run;
    return NULL;
}

static int redirect(const struct command *cmd)
{
    for (int i = 0; i < cmd->redirection_count; i++) {
        struct redirection r = cmd->redirections[i];
        int input = r.op == '<';
        int fd = open(r.path, input ? O_RDONLY : O_WRONLY | O_CREAT | O_TRUNC,
                      0666);
        if (fd < 0) {
            perror(r.path);
            return -1;
        }
        int rc = dup2(fd, input ? STDIN_FILENO : STDOUT_FILENO);
        close(fd);
        if (rc < 0) {
            perror("dup2");
            return -1;
        }
    }
    return 0;
}

static int parent_cd(const struct command *cmd)
{
    if (!cmd->redirection_count)
        return cmd_cd(cmd->argc, cmd->argv);

    int saved_in = dup(STDIN_FILENO);
    int saved_out = dup(STDOUT_FILENO);
    int result = 1;
    if (saved_in < 0 || saved_out < 0) {
        perror("dup");
    } else {
        if (redirect(cmd) == 0)
            result = cmd_cd(cmd->argc, cmd->argv);
        if (dup2(saved_in, STDIN_FILENO) < 0 ||
            dup2(saved_out, STDOUT_FILENO) < 0) {
            perror("restore stdio");
            exit(1);
        }
    }
    if (saved_in >= 0)
        close(saved_in);
    if (saved_out >= 0)
        close(saved_out);
    return result;
}

static void close_pipes(int pipes[][2], int count)
{
    for (int i = 0; i < count; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}

static void run_child(const struct pipeline *job, int index, pid_t group,
                      int pipes[][2], const int gate[2], const int ready[2],
                      const sigset_t *mask)
{
    close(gate[1]);
    close(ready[0]);
    if (setpgid(0, group) < 0)
        _exit(1);
    if (set_handler(SIGINT, SIG_DFL, 0) < 0 ||
        set_handler(SIGQUIT, SIG_DFL, 0) < 0 ||
        set_handler(SIGTSTP, SIG_DFL, 0) < 0 ||
        set_handler(SIGTTIN, SIG_DFL, 0) < 0 ||
        set_handler(SIGTTOU, SIG_DFL, 0) < 0 ||
        set_handler(SIGPIPE, SIG_DFL, 0) < 0 ||
        set_handler(SIGCHLD, SIG_DFL, 0) < 0)
        _exit(1);

    // Acknowledge that the signal dispositions are ready before terminal handoff.
    ssize_t n;
    do {
        n = write(ready[1], "r", 1);
    } while (n < 0 && errno == EINTR);
    close(ready[1]);
    if (n != 1)
        _exit(1);

    // Wait until the parent has assigned the foreground terminal.
    char unused;
    do {
        n = read(gate[0], &unused, 1);
    } while (n < 0 && errno == EINTR);
    close(gate[0]);
    if (n < 0 || sigprocmask(SIG_SETMASK, mask, NULL) < 0)
        _exit(1);

    if ((index > 0 && dup2(pipes[index - 1][0], STDIN_FILENO) < 0) ||
        (index + 1 < job->count && dup2(pipes[index][1], STDOUT_FILENO) < 0)) {
        perror("dup2");
        _exit(1);
    }
    close_pipes(pipes, job->count - 1);
    if (job->background && index == 0) {
        int fd = open("/dev/null", O_RDONLY);
        if (fd < 0 || dup2(fd, STDIN_FILENO) < 0) {
            perror("/dev/null");
            _exit(1);
        }
        close(fd);
    }

    const struct command *cmd = &job->commands[index];
    if (redirect(cmd) < 0)
        _exit(1);
    command_fn run = find_builtin(cmd->argv[0]);
    if (run) {
        int result = run(cmd->argc, cmd->argv);
        if (fflush(stdout) == EOF && result == 0)
            result = 1;
        _exit(result);
    }

    execvp(cmd->argv[0], cmd->argv);
    int error = errno;
    perror(cmd->argv[0]);
    _exit(error == ENOENT ? 127 : 126);
}

static int wait_job(pid_t group, const pid_t *children, int count)
{
    int result = 1;
    for (int remaining = count; remaining > 0;) {
        int status;
        pid_t child = waitpid(-1, &status, WUNTRACED);
        if (child < 0) {
            if (errno == EINTR)
                continue;
            perror("waitpid");
            return 1;
        }
        int index = 0;
        while (index < count && children[index] != child)
            index++;
        if (index == count)
            continue;  // Reap background exits while waiting for the foreground.
        if (WIFSTOPPED(status)) {
            // This shell has no suspended-job table or fg command.
            fputs("shell: stopped job terminated\n", stderr);
            kill(-group, SIGKILL);
            continue;
        }
        remaining--;
        if (index == count - 1)
            result = WIFEXITED(status) ? WEXITSTATUS(status) :
                     128 + WTERMSIG(status);
    }
    return result;
}

static int wait_until_ready(int fd, int count)
{
    char bytes[MAX_STAGES];
    while (count > 0) {
        ssize_t n = read(fd, bytes, (size_t)count);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            if (n == 0)
                errno = EIO;
            return -1;
        }
        count -= (int)n;
    }
    return 0;
}

static int execute(const struct pipeline *job)
{
    int pipes[MAX_STAGES - 1][2];
    int opened = 0;
    while (opened < job->count - 1) {
        if (pipe(pipes[opened]) < 0) {
            perror("pipe");
            close_pipes(pipes, opened);
            return 1;
        }
        opened++;
    }
    int gate[2];
    if (pipe(gate) < 0) {
        perror("pipe");
        close_pipes(pipes, opened);
        return 1;
    }
    int ready[2];
    if (pipe(ready) < 0) {
        perror("pipe");
        close_pipes(pipes, opened);
        close(gate[0]);
        close(gate[1]);
        return 1;
    }

    sigset_t block, previous;
    sigemptyset(&block);
    sigaddset(&block, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &block, &previous) < 0) {
        perror("sigprocmask");
        close(gate[0]);
        close(gate[1]);
        close(ready[0]);
        close(ready[1]);
        close_pipes(pipes, opened);
        return 1;
    }

    pid_t children[MAX_STAGES];
    pid_t group = 0;
    int count = 0;
    int failed = 0;
    for (int i = 0; i < job->count; i++) {
        pid_t child = fork();
        if (child == 0)
            run_child(job, i, group, pipes, gate, ready, &previous);
        if (child < 0) {
            perror("fork");
            failed = 1;
            break;
        }
        children[count++] = child;
        if (!group)
            group = child;
        if (setpgid(child, group) < 0) {
            perror("setpgid");
            failed = 1;
            break;
        }
    }
    close(gate[0]);
    close_pipes(pipes, opened);
    close(ready[1]);
    if (!failed && wait_until_ready(ready[0], count) < 0) {
        perror("child startup");
        failed = 1;
    }
    close(ready[0]);

    int foreground = interactive && !job->background;
    if (!failed && foreground && tcsetpgrp(STDIN_FILENO, group) < 0) {
        perror("tcsetpgrp");
        failed = 1;
    }
    if (failed)
        for (int i = 0; i < count; i++)
            kill(children[i], SIGKILL);
    close(gate[1]);

    int result = 0;
    if (failed) {
        for (int i = 0; i < count; i++)
            while (waitpid(children[i], NULL, 0) < 0 && errno == EINTR)
                ;
        result = 1;
    } else if (job->background) {
        if (interactive)
            fprintf(stderr, "[%ld]\n", (long)group);
    } else {
        result = wait_job(group, children, count);
    }
    if (foreground && !failed && tcsetpgrp(STDIN_FILENO, shell_group) < 0) {
        perror("tcsetpgrp");
        exit(1);
    }
    if (sigprocmask(SIG_SETMASK, &previous, NULL) < 0) {
        perror("sigprocmask");
        exit(1);
    }
    return result;
}

static int initialize(void)
{
    // Pipe and redirection descriptors must not overlap standard streams.
    for (int fd = STDIN_FILENO; fd <= STDERR_FILENO; fd++)
        if (fcntl(fd, F_GETFD) < 0)
            return -1;
    interactive = isatty(STDIN_FILENO) && tcgetpgrp(STDIN_FILENO) == getpgrp();
    shell_group = getpgrp();
    if (set_handler(SIGINT, on_interrupt, 0) < 0 ||
        set_handler(SIGQUIT, SIG_IGN, 0) < 0 ||
        set_handler(SIGTSTP, SIG_IGN, 0) < 0 ||
        set_handler(SIGTTIN, SIG_IGN, 0) < 0 ||
        set_handler(SIGTTOU, SIG_IGN, 0) < 0 ||
        set_handler(SIGCHLD, reap_background, SA_RESTART | SA_NOCLDSTOP) < 0)
        return -1;
    if (interactive && shell_group != getpid()) {
        if (setpgid(0, 0) < 0)
            return -1;
        shell_group = getpgrp();
        if (tcsetpgrp(STDIN_FILENO, shell_group) < 0)
            return -1;
    }
    return 0;
}

// Read no further than one line, leaving subsequent input for the command.
static int read_line(char line[MAX_LINE + 1], const sigset_t *mask)
{
    int length = 0;
    for (;;) {
        if (interactive) {
            fd_set input;
            FD_ZERO(&input);
            FD_SET(STDIN_FILENO, &input);
            if (pselect(STDIN_FILENO + 1, &input, NULL, NULL, NULL, mask) < 0) {
                if (errno != EINTR || interrupted)
                    return -1;
                continue;
            }
        }
        char byte;
        ssize_t n = read(STDIN_FILENO, &byte, 1);
        if (interrupted)
            return -1;
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
            break;
        if (length <= MAX_LINE)
            length++;
        if (length <= MAX_LINE)
            line[length - 1] = byte;
        if (byte == '\n')
            break;
    }
    if (length <= MAX_LINE)
        line[length] = '\0';
    return length;
}

int main(void)
{
    if (initialize() < 0) {
        perror("shell initialization");
        return 1;
    }

    sigset_t block;
    sigemptyset(&block);
    sigaddset(&block, SIGINT);
    int result = 0;
    for (;;) {
        char line[MAX_LINE + 1];
        sigset_t previous;
        if (interactive) {
            // Unblock SIGINT atomically while waiting for terminal input.
            if (sigprocmask(SIG_BLOCK, &block, &previous) < 0) {
                perror("sigprocmask");
                return 1;
            }
            fputs("lab2-shell$ ", stderr);
            fflush(stderr);
        }
        int length = read_line(line, interactive ? &previous : NULL);
        int read_error = errno;
        if (interactive && sigprocmask(SIG_SETMASK, &previous, NULL) < 0) {
            perror("sigprocmask");
            return 1;
        }
        if (interrupted) {
            interrupted = 0;
            if (interactive)
                fputc('\n', stderr);
            result = 130;
            continue;
        }
        if (length == 0)
            break;
        if (length < 0) {
            errno = read_error;
            perror("read");
            result = 1;
            break;
        }
        if (length > MAX_LINE || memchr(line, '\0', (size_t)length)) {
            fputs("shell: invalid or overlong input\n", stderr);
            result = 2;
            continue;
        }

        char words[MAX_LINE + 1];
        struct pipeline job = {0};
        const char *error = parse_line(line, words, &job);
        if (error) {
            fprintf(stderr, "shell: %s\n", error);
            result = 2;
            continue;
        }
        if (!job.count)
            continue;

        const struct command *first = &job.commands[0];
        int exit_requested = 0;
        for (int i = 0; i < job.count; i++)
            if (!strcmp(job.commands[i].argv[0], "exit"))
                exit_requested = 1;
        if (exit_requested) {
            if (job.count == 1 && !job.background &&
                !first->redirection_count && first->argc == 1)
                break;
            fputs("usage: exit (standalone)\n", stderr);
            result = 2;
        } else if (job.count == 1 && !job.background &&
                   !strcmp(first->argv[0], "cd")) {
            result = parent_cd(first);
        } else {
            result = execute(&job);
        }
    }
    return result;
}
