#include "commands.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

int cmd_ln(int argc, char *const argv[])
{
    int symbolic = 0;
    int i = 1;

    while ( i< argc && argv[i][0] == '-' && argv[i][1]){
        if(!strcmp(argv[i], "--")){
            i++;
            break;
        }
        if (strcmp(argv[i], "-s")) {
            fputs("usage: ln [-s] [--] target link_name\n", stderr);
            return 2;
        }
        symbolic = 1;
        i++;
    }

    if (argc - i != 2) {
        fputs("usage: ln [-s] [--] target link_name\n", stderr);
        return 2;
    }
    const char *target = argv[i];
    const char *link_name = argv[i+1];

    int rc = symbolic ? symlink(target, link_name) : 
    link(target, link_name);
    if(rc < 0) {
        perror(link_name);
        return 1;
    }

    return 0;
    }
