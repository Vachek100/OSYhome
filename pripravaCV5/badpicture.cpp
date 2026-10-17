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

void myTee(int fd, int outputFd) {
    char buffer[1024];

    while (1) {
        int ret = read(fd, buffer, sizeof(buffer));

        if (ret < 0) {
            perror("Unable to read from pipe");
            exit(EXIT_FAILURE);
        }

        if (ret == 0) {
            exit(EXIT_SUCCESS);
        }


        int written = 0;

        while (written < ret) {
            int result = write(STDOUT_FILENO,
                               buffer + written,
                               ret - written);

            if (result < 0) {
                perror("Unable to write to stdout");
                exit(EXIT_FAILURE);
            }

            if (result == 0) {
                fprintf(stderr, "Write to stdout returned 0\n");
                exit(EXIT_FAILURE);
            }

            written += result;
        }

        if (outputFd >= 0) {
            written = 0;

            while (written < ret) {
                int result = write(outputFd,
                                   buffer + written,
                                   ret - written);

                if (result < 0) {
                    perror("Unable to write to file");
                    exit(EXIT_FAILURE);
                }

                if (result == 0) {
                    fprintf(stderr, "Write to file returned 0\n");
                    exit(EXIT_FAILURE);
                }

                written += result;
            }
        }
    }
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }

    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }

    return -1;
}

void decodeHex() {
    char buffer[1024];
    int firstValue = -1;

    while (1) {
        int ret = read(STDIN_FILENO, buffer, sizeof(buffer));

        if (ret < 0) {
            perror("read hex input");
            exit(EXIT_FAILURE);
        }

        if (ret == 0) {
            break;
        }

        for (int i = 0; i < ret; i++) {
            char c = buffer[i];

            // Přeskočí mezery a konce řádků.
            if (c == ' ' || c == '\n' ||
                c == '\r' || c == '\t') {
                continue;
                }

            int value = hexValue(c);

            if (value < 0) {
                fprintf(stderr, "Invalid hex character: %c\n", c);
                exit(EXIT_FAILURE);
            }

            if (firstValue == -1) {
                // Zatím máme pouze první hex číslici.
                firstValue = value;
            } else {
                // Máme obě číslice, vytvoříme jeden bajt.
                unsigned char byte = firstValue * 16 + value;
                firstValue = -1;

                int written = 0;

                while (written < 1) {
                    int result = write(
                        STDOUT_FILENO,
                        &byte + written,
                        1 - written
                    );

                    if (result < 0) {
                        perror("write decoded byte");
                        exit(EXIT_FAILURE);
                    }

                    if (result == 0) {
                        fprintf(stderr, "write returned 0\n");
                        exit(EXIT_FAILURE);
                    }

                    written += result;
                }
            }
        }
    }

    if (firstValue != -1) {
        fprintf(stderr, "Incomplete hex pair at end of input\n");
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {

    const char *filename = "bad.jpg";
    const char *outputfilename = "img.jpg";

    int mypipefdA[2];
    int mypipefdB[2];
    int mypipefdC[2];
    int mypipefdD[2];

    if (pipe(mypipefdA) < 0) {
        perror("Unable to create pipe A");
        return 1;
    }

    if (pipe(mypipefdB) < 0) {
        perror("Unable to create pipe B");
        return 1;
    }

    if (pipe(mypipefdC) < 0) {
        perror("Unable to create pipe C");
        return 1;
    }

    if (pipe(mypipefdD) < 0) {
        perror("Unable to create pipe D");
        return 1;
    }

    //CREATION OF FIRST CHILD -> rev command
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
        close(mypipefdC[0]);
        close(mypipefdC[1]);
        close(mypipefdD[0]);
        close(mypipefdD[1]);

        int file = open(filename, O_RDONLY);

        if (file < 0) {
            perror("Unable to open input file for reading");
            exit(EXIT_FAILURE);
        }

        dup2(file, STDIN_FILENO);
        dup2(mypipefdA[1], STDOUT_FILENO);

        close(mypipefdA[1]);

        close(file);

        char *args[] = {
            (char *)"rev",
            nullptr
        };

        execvp("rev", args);

        perror("execvp rev");
        exit(EXIT_FAILURE);


    }

    // PARENT ONLY CODE

    //CREATION OF SECOND CHILD -> sort command
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
        close(mypipefdC[0]);
        close(mypipefdC[1]);
        close(mypipefdD[0]);
        close(mypipefdD[1]);

        dup2(mypipefdA[0], STDIN_FILENO);
        dup2(mypipefdB[1], STDOUT_FILENO);

        close(mypipefdA[0]);
        close(mypipefdB[1]);

        char *args[] = {
            (char *)"sort",
            (char *)"-n",
            nullptr
        };

        execvp("sort", args);

        perror("execvp sort");
        exit(EXIT_FAILURE);


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
        close(mypipefdC[0]);
        close(mypipefdD[0]);
        close(mypipefdD[1]);


        dup2(mypipefdB[0], STDIN_FILENO);
        dup2(mypipefdC[1], STDOUT_FILENO);

        close(mypipefdB[0]);
        close(mypipefdC[1]);


        char *args[] = {
            (char *)"./mycut",
            nullptr
        };

        execvp("./mycut", args);

        perror("execvp mycut");
        exit(EXIT_FAILURE);

    }

    //PARENT ONLY CODE

    //CREATION OF FOURTH CHILD -> grep command
    pid_t child4 = fork();

    if (child4 < 0) {
        perror("Unable to create new child process(child 4)");
        return 1;
    }

    if (child4 != 0) {
        // PARENT


    }else {
        // CHILD 4

        close(mypipefdA[0]);
        close(mypipefdA[1]);
        close(mypipefdB[0]);
        close(mypipefdB[1]);
        close(mypipefdC[1]);
        close(mypipefdD[0]);

        if (dup2(mypipefdC[0], STDIN_FILENO) < 0) {
            perror("dup2 stdin child 4");
            exit(EXIT_FAILURE);
        }

        if (dup2(mypipefdD[1], STDOUT_FILENO) < 0) {
            perror("dup2 stdout child 4");
            exit(EXIT_FAILURE);
        }

        close(mypipefdC[0]);
        close(mypipefdD[1]);

        decodeHex();

        /*
        close(mypipefdA[0]);
        close(mypipefdA[1]);
        close(mypipefdB[0]);
        close(mypipefdB[1]);
        close(mypipefdC[1]);
        close(mypipefdD[0]);
        close(mypipefdD[1]);

        dup2(mypipefdC[0], STDIN_FILENO);

        int file = open(outputfilename, O_WRONLY | O_CREAT | O_TRUNC, 0644);

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

        close(mypipefdC[0]);


        char *args[] = {
            (char *)"basenc",
            (char *)"-i",
            (char *)"-d",
            (char *)"--base16",
            nullptr
        };

        execvp("basenc", args);

        perror("execvp basenc");
        exit(EXIT_FAILURE);*/

    }

    //CREATION OF FIFTH CHILD -> tee command
    pid_t child5 = fork();

    if (child5 < 0) {
        perror("Unable to create new child process(child 5)");
        return 1;
    }

    if (child5 != 0) {
        // PARENT


    }else {
        // CHILD 5

        // CREATION OF FIFTH CHILD -> display -

        pid_t child5 = fork();

        if (child5 < 0) {
            perror("Unable to create child 5");
            return 1;
        }

        if (child5 == 0) {
            // CHILD 5

            close(mypipefdA[0]);
            close(mypipefdA[1]);
            close(mypipefdB[0]);
            close(mypipefdB[1]);
            close(mypipefdC[0]);
            close(mypipefdC[1]);
            close(mypipefdD[1]);

            if (dup2(mypipefdD[0], STDIN_FILENO) < 0) {
                perror("dup2 stdin child 5");
                exit(EXIT_FAILURE);
            }

            close(mypipefdD[0]);

            char *args[] = {
                (char *)"display",
                (char *)"-",
                nullptr
            };

            execvp("display", args);

            perror("execvp display");
            exit(EXIT_FAILURE);
        }

        /*
        close(mypipefdA[0]);
        close(mypipefdA[1]);
        close(mypipefdB[0]);
        close(mypipefdB[1]);
        close(mypipefdC[0]);
        close(mypipefdC[1]);
        close(mypipefdD[1]);

        dup2(mypipefdD[0], STDIN_FILENO);

        close(mypipefdD[0]);

        int outputFd = -1;

        //if (argc == 4 && strcmp(argv[1], "-o") == 0) {
        if (strcmp(argv[1], "-o") == 0){
            outputFd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);

            if (outputFd < 0) {
                perror("Unable to open output file");
                exit(EXIT_FAILURE);
            }
        }

        myTee(STDIN_FILENO, outputFd);


/*
        if (argc == 4 && strcmp(argv[1], "-o") == 0) {

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


            char *args[] = {
                (char *)"tee",
                argv[2],
                nullptr
            };

            execvp("tee", args);
        }
        else {
            char *args[] = {
                (char *)"tee",
                nullptr
            };

            execvp("tee", args);
        }

        perror("execvp tee");
        exit(EXIT_FAILURE);*/
    }

    close(mypipefdA[1]);
    close(mypipefdA[0]);

    close(mypipefdB[1]);
    close(mypipefdB[0]);

    close(mypipefdC[1]);
    close(mypipefdC[0]);

    close(mypipefdD[1]);
    close(mypipefdD[0]);

    wait(nullptr);
    wait(nullptr);
    wait(nullptr);
    wait(nullptr);
    wait(nullptr);

    return 0;
}