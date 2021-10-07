#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"

void find(char *path, char *name) {
    struct stat st;
    struct dirent de;
    int fd;
    char buf[512], *p;
    if((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if(fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    // 只能在 directory里面find
    if (st.type != T_DIR) {
        return;
    }

    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
      printf("find: path too long\n");
      return;
    }
    // set path as prefix
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0)
            continue;

        memmove(p, de.name, strlen(de.name));
        p[strlen(de.name)] = 0;
        if(stat(buf, &st) < 0){
            printf("find: cannot stat %s\n", buf);
            continue;
        }

        switch (st.type)
        {
        case T_DIR:
            if (strcmp(de.name, ".") != 0 && strcmp(de.name, "..") != 0)
                find(buf, name);
            break;
        case T_FILE:
            if (strcmp(name, de.name) == 0) {
                printf("%s\n", buf);
            }
            break;
        }   
    }
    close(fd);
}

int
main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(2, "Usage: find <path> <specific_name>\n");
        exit(0);
    }
    
    find(argv[1], argv[2]);

    exit(0);
}