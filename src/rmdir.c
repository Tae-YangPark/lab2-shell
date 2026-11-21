#include "commands.h"
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>


int cmd_rmdir(int argc, char *const argv[])
{
    int i = 1;

    while (i < argc && argv[i][0] == '-' && argv[i][1]) {
        if (!strcmp(argv[i], "--")) {   
            i++;
            break;
        }
        fputs("usage: rmdir [--] directory ...\n", stderr);
        return 2;
    }

    if (i == argc) {                    
        fputs("usage: rmdir [--] directory ...\n", stderr);
        return 2;
    }

    int error = 0;
    for (; i < argc; i++) {
        if (rmdir(argv[i]) < 0) {
            perror(argv[i]);       
            error = 1;
        }
    }
    return error;
}