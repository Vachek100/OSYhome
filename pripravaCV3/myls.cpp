//
// Created by vasek on 9/24/26.
//

#include <cstdio>
#include <ctime>
#include <cstring>
#include <sys/stat.h>

#define MAX_FILES 1000


int main(int argc, char *argv[]){

    char options[3];
    int optionCount = 0;

    char *files[MAX_FILES];
    int fileCount = 0;

    char *notFound[MAX_FILES];
    int notFoundCount = 0;

    for (int i = 1; i < argc; i++) {

        if (strcmp(argv[i], "-s") == 0){

            options[optionCount] = 's';
            optionCount++;

        }else if (strcmp(argv[i], "-t") == 0) {

            options[optionCount] = 't';
            optionCount++;

        }else if (strcmp(argv[i], "-r") == 0) {

            options[optionCount] = 'r';
            optionCount++;

        }else {
            files[fileCount] = argv[i];
            fileCount++;
        }
    }

    for (int i = 0; i < fileCount; i++) {

        struct stat info;

        if (stat(files[i], &info) == 0) {
            for (int j = 0; j < optionCount; j++) {
                if (options[j] == 's'){
                    long size = info.st_size;

                    if (size < 1024)
                    {
                        fprintf(stdout, " %ldB ", size);
                    }
                    else if (size < 1024 * 1024)
                    {
                        fprintf(stdout, " %.1fkB ", size / 1024.0);
                    }
                    else if (size < 1024LL * 1024 * 1024)
                    {
                        fprintf(stdout, " %.1fMB ", size / (1024.0 * 1024.0));
                    }
                    else
                    {
                        fprintf(stdout, " %.1fGB ", size / (1024.0 * 1024.0 * 1024.0));
                    }
                }else if (options[j] == 'r') {
                    fprintf(stdout,".");
                    if (info.st_mode & S_IRUSR) {
                        fprintf(stdout, "r");
                    } else {
                        fprintf(stdout, "-");
                    }

                    if (info.st_mode & S_IWUSR) {
                        fprintf(stdout, "w");
                    } else {
                        fprintf(stdout, "-");
                    }

                    if (info.st_mode & S_IXUSR) {
                        fprintf(stdout, "x");
                    } else {
                        fprintf(stdout, "-");
                    }

                    if (info.st_mode & S_IRGRP) {
                        fprintf(stdout, "r");
                    } else {
                        fprintf(stdout, "-");
                    }

                    if (info.st_mode & S_IWGRP) {
                        fprintf(stdout, "w");
                    } else {
                        fprintf(stdout, "-");
                    }

                    if (info.st_mode & S_IXGRP) {
                        fprintf(stdout, "x");
                    } else {
                        fprintf(stdout, "-");
                    }

                    if (info.st_mode & S_IROTH) {
                        fprintf(stdout, "r");
                    } else {
                        fprintf(stdout, "-");
                    }

                    if (info.st_mode & S_IWOTH) {
                        fprintf(stdout, "w");
                    } else {
                        fprintf(stdout, "-");
                    }

                    if (info.st_mode & S_IXOTH) {
                        fprintf(stdout, "x");
                    } else {
                        fprintf(stdout, "-");
                    }
                }else if (options[j] == 't') {
                    fprintf(stdout, " %.*s ", 24, asctime(localtime(&info.st_mtime)));
                }
            }
            //jmeno souboru
            fprintf(stdout, "%20s\n", files[i]);
        }else {
            notFound[notFoundCount] = files[i];
            notFoundCount++;
        }
    }

    if (notFoundCount != 0) {

        fprintf(stdout, "Tyto soubory neexistuji: \n");

        for (int i = 0; i < notFoundCount; i++) {
            fprintf(stdout, "%s\n", notFound[i]);
        }
    }



    return 0;
}