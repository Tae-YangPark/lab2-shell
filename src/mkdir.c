#include <limits.h>
#include "commands.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static int make_one_allow_exist(const char *path)
{
    if (mkdir(path, 0777) == 0)
        return 0;                      // 새로 만드면 성공
    if (errno == EEXIST) {             
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
            return 0;                  
        errno = EEXIST;                
    }
    perror(path);
    return 1;
}
static int make_parents(const char *path)
{
    char buf[PATH_MAX]; //path를 수정 가능한 버퍼로 복사

    if (strlen(path) >= sizeof(buf)) {
        fprintf(stderr, "%s: path too long\n", path);
        return 1;
    }
    strcpy(buf, path);
    for (char *p = buf + 1; *p; p++) {
        if (*p != '/')
            continue;
        *p = '\0';                
        if (make_one_allow_exist(buf))
            return 1;
        *p = '/';                  
    }
    return make_one_allow_exist(buf); 
}


int cmd_mkdir(int argc, char *const argv[])
{
    int parents = 0;
    int i = 1;

    while (i < argc && argv[i][0] == '-' && argv[i][1]) {
        if (!strcmp(argv[i], "--")) {   
            i++;
            break;
        }
        if (strcmp(argv[i], "-p")) {   
            fputs("usage: mkdir [-p] [--] directory ...\n", stderr);
            return 2;
        }
        parents = 1;
        i++;
    }

    if (i == argc) {                    
        fputs("usage: mkdir [-p] [--] directory ...\n", stderr);
        return 2;
    }

    int error = 0;
    for (; i < argc; i++) {
        if (parents) {
            if (make_parents(argv[i]))
                error = 1;
        } else if (mkdir(argv[i], 0777) < 0) {
            perror(argv[i]);       
            error = 1;
        }
    }
    return error;
}