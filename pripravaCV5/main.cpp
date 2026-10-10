//
// Created by vasek on 10/9/26.
//

/*
 *
 * mypipe[0] → čtecí konec
 *
 * mypipe[1] → zapisovací konec
 *
 * CHILD 1 -> sort command
 * CHILD 2 -> nl command
 * CHILD 3 -> tr command
 *
 * dup2(kam nastavit aby to vedlo, co nastavit aby vedlo);
 *
 *  Hlavní pravidlo, které si z toho zapamatuj: po fork() musíš zajistit, aby se první potomek nedostal k dalšímu fork().
 *  Proto exit() po práci potomka. A rodič musí nejdřív vytvořit všechny požadované potomky a teprve potom dělat wait().
 */

#include <cstdio>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <cstring>
#include <cstdlib>
#include <time.h>
#include <fcntl.h>

void consumer(int readFd) {
    char buffer[1024];
    char remaining[1024] = "";
    int numLines = 1;

    while (1) {

        int ret = read(readFd, buffer, sizeof(buffer) - 1);

        if (ret < 0) {
            perror("Consumer function: Unable to read from pipe");
            exit(EXIT_FAILURE);
        }
        else if (ret == 0) {
            //printf("Consumer function: Pipe closed. Why?\n");
            exit(EXIT_SUCCESS);
        }

        buffer[ret] = '\0';

        strcat(remaining, buffer);

        char *start = remaining;

        while (1) {

            char *end = strchr(start, '\n');

            if (end == nullptr) {
                break;
            }

            *end = '\0';

            fprintf(stdout, "%6d. %s\n", numLines, start);

            numLines++;

            start = end + 1;
        }

        strcpy(remaining, start);

    }
}

int main(int argc, char **argv) {

    if (argc != 2 && argc != 3) {
        fprintf(stderr, "Usage: %s input_file [output_file]\n", argv[0]);
        return EXIT_FAILURE;
    }

    //const char *filename = argv[1];

    int mypipefdA[2];
    int mypipefdB[2];

    if (pipe(mypipefdA) < 0) {
        perror("Unable to create pipe A");
        return 1;
    }

    if (pipe(mypipefdB) < 0) {
        perror("Unable to create pipe B");
        return 1;
    }

    //CREATION OF FIRST CHILD -> sort command
    pid_t child1 = fork();

    if (child1 < 0) {
        perror("Unable to create new child process(child 1)");
        return 1;
    }

    if (child1 != 0) {
        // PARENT

    }
    else {
        // CHILD 1

        close(mypipefdA[0]);
        close(mypipefdB[0]);
        close(mypipefdB[1]);

        int file = open(argv[1], O_RDONLY);

        if (file < 0) {
            perror("Unable to open input file for reading");
            exit(EXIT_FAILURE);
        }

        dup2(file, STDIN_FILENO);
        dup2(mypipefdA[1], STDOUT_FILENO);

        close(mypipefdA[1]);

        close(file);

        char *args[] = {
            (char *)"sort",
            nullptr
        };

        execvp("sort", args);

        perror("execvp sort");
        exit(EXIT_SUCCESS);


    }

    // PARENT ONLY CODE

    //CREATION OF SECOND CHILD -> nl command
    pid_t child2 = fork();

    if (child2 < 0) {
        perror("Unable to create new child process(child 2)");
        return 1;
    }

    if (child2 != 0) {
        // PARENT


    }else {
        // CHILD 2

        close(mypipefdA[1]);
        close(mypipefdB[0]);

        dup2(mypipefdA[0], STDIN_FILENO);
        dup2(mypipefdB[1], STDOUT_FILENO);

        consumer(mypipefdA[0]);

        close(mypipefdA[0]);
        close(mypipefdB[1]);

        exit(EXIT_FAILURE);

        /*
        close(mypipefdA[1]);
        close(mypipefdB[0]);

        dup2(mypipefdA[0], STDIN_FILENO);
        dup2(mypipefdB[1], STDOUT_FILENO);

        close(mypipefdA[0]);
        close(mypipefdB[1]);

        char *args[] = {
            (char *)"nl",
            (char *)"-s",
            (char *)". ",
            nullptr
        };

        execvp("nl", args);

        perror("execvp nl");
        exit(EXIT_FAILURE);
    */
    }

    // PARENT ONLY CODE

    //CREATION OF THIRD CHILD -> tr command
    pid_t child3 = fork();

    if (child3 < 0) {
        perror("Unable to create new child process(child 3)");
        return 1;
    }

    if (child3 != 0) {
        // PARENT


    }else {
        // CHILD 3
        close(mypipefdA[0]);
        close(mypipefdA[1]);
        close(mypipefdB[1]);

        dup2(mypipefdB[0], STDIN_FILENO);

        close(mypipefdB[0]);

        if (argc == 3) {
            int file = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);

            if (file < 0) {
                perror("Unable to open output file for writing");
                exit(EXIT_FAILURE);
            }

            if (dup2(file, STDOUT_FILENO) < 0) {
                perror("dup2 stdout to outputfile");
                close(file);
                exit(EXIT_FAILURE);
            }

            close(file);

            close(file);

        }


        char *args[] = {
            (char *)"tr",
            (char *)"a-z",
            (char *)"A-Z",
            nullptr
        };

        execvp("tr", args);

        perror("execvp tr");
        exit(EXIT_FAILURE);

    }

    //PARENT ONLY CODE

    close(mypipefdA[1]);
    close(mypipefdA[0]);

    close(mypipefdB[1]);
    close(mypipefdB[0]);

    wait(nullptr);
    wait(nullptr);
    wait(nullptr);

    return 0;
}