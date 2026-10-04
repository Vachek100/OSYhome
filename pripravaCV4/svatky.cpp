//
// Created by vasek on 10/2/26.
//

/*
 * g_svatky[0][0] -> [prvni svatek(prvek v poli) v seznamu svatku] [prvni prvek v poli] -> "1.1."
 *
 * g_svatky[0][1] -> [prvni svatek(prvek v poli) v seznamu svatku] [druhy prvek v poli] -> "Novy rok"
 *
 * mypipe[0] → čtecí konec
 *
 * mypipe[1] → zapisovací konec
 *
 */

#include <cstdio>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <cstring>
#include <cstdlib>
#include <time.h>

#include "svatky.hpp"


void generateRandomDate(char *buffer) {

    int daysInMonth[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    int month = rand() % 12 + 1;
    int maxDaysInMonth = daysInMonth[month - 1];
    int day = rand() % maxDaysInMonth + 1;

    sprintf(buffer, "%d.%d.", day, month);
}


void producerAndGenerator(int fd, int dateCountToGenerate, int dateCountPerSecond) {

    char buffer[20];

    int delay = 1000000 / dateCountPerSecond;

    for (int i = 0; i < dateCountToGenerate; i++) {

        generateRandomDate(buffer);

        char output[1024];

        sprintf(output, "%d. %s (%d)\n", i + 1, buffer, getpid());

        int ret = write(fd, output, strlen(output));

        if (ret < 0) {
            perror("Unable to write to pipe");
            exit(EXIT_FAILURE);
        }

        usleep(delay);
    }
}


void middleman(int readFd, int writeFdB, int writeFdC, int N) {

    char buffer[1024];
    char remaining[1024] = "";

    while (1) {

        int ret = read(readFd, buffer, sizeof(buffer) - 1);

        if (ret < 0) {
            perror("Unable to read from pipe");
            exit(EXIT_FAILURE);
        }
        else if (ret == 0) {
            printf("Pipe closed. Why?\n");
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


            char *numLineStart = strchr(start, '.');

            if (numLineStart != nullptr) {
                *numLineStart = '\0';

                char* dateStart = numLineStart + 2;

                char *pidStart = strchr(dateStart, '(');

                if (pidStart != nullptr) {
                    *(pidStart - 1) = '\0';

                    for (int i = 0; g_svatky[i][0] != nullptr; i++) {

                        if (strcmp(dateStart, g_svatky[i][0]) == 0) {
                            char output[1024];

                            sprintf(output, "%s. %s %s %s (%d)\n", start, dateStart, g_svatky[i][1], pidStart, getpid());

                            if (strlen(g_svatky[i][1]) <= N) {
                                int ret = write(writeFdB, output, strlen(output));

                                if (ret < 0) {
                                    perror("Middleman function: Unable to write to pipe B");
                                }

                                break;
                            }else{
                                int ret = write(writeFdC, output, strlen(output));

                                if (ret < 0) {
                                    perror("Middleman function: Unable to write to pipe C");
                                }

                                break;
                            }


                        }
                    }
                }
            }

            start = end + 1;
        }

        strcpy(remaining, start);
    }
}

void consumerCHILD3(int fd) {

    char buffer[1024];
    char remaining[1024] = "";

    while (1) {

        int ret = read(fd, buffer, sizeof(buffer) - 1);

        if (ret < 0) {
            perror("Unable to read from pipe");
            exit(EXIT_FAILURE);
        }
        else if (ret == 0) {
            exit(EXIT_SUCCESS);
        }

        buffer[ret] = '\0';

        strcat(remaining, buffer);

        char *start = remaining;

        while (1) {
            char output[1024];

            char *end = strchr(start, '\n');

            if (end == nullptr) {
                break;
            }

            *end = '\0';

            sprintf(output, "%s (%d)", start, getpid());

            fprintf(stdout, "kratke %s |\n",output);


            start = end + 1;
        }

        strcpy(remaining, start);
    }
}

void consumerCHILD4(int fd) {

    char buffer[1024];
    char remaining[1024] = "";

    while (1) {

        int ret = read(fd, buffer, sizeof(buffer) - 1);

        if (ret < 0) {
            perror("Unable to read from pipe");
            exit(EXIT_FAILURE);
        }
        else if (ret == 0) {
            exit(EXIT_SUCCESS);
        }

        buffer[ret] = '\0';

        strcat(remaining, buffer);

        char *start = remaining;

        while (1) {

            char output[1024];

            char *end = strchr(start, '\n');

            if (end == nullptr) {
                break;
            }

            *end = '\0';

            sprintf(output, "%s (%d)", start, getpid());

            fprintf(stdout, "                                               | dlouhe %s\n",output);


            start = end + 1;
        }

        strcpy(remaining, start);
    }
}


int main(int argc, char **argv) {

    srand(time(NULL));

    if (argc != 3) {
        fprintf(stderr, "Usage: ./svatky M N\n");
        return 1;
    }

    int dateCountToGenerate = atoi(argv[1]);
    int dateCountPerSecond = atoi(argv[2]);

    int N = dateCountPerSecond;

    int mypipefdA[2];

    if (pipe(mypipefdA) < 0) {
        perror("Unable to create first pipe (pipe A)");
        return 1;
    }

    int mypipefdB[2];

    if (pipe(mypipefdB) < 0) {
        perror("Unable to create first pipe (pipe B)");
        return 1;
    }

    int mypipefdC[2];

    if (pipe(mypipefdC) < 0) {
        perror("Unable to create first pipe (pipe C)");
        return 1;
    }

    pid_t child = fork();

    if (child < 0) {
        perror("Unable to create new child(first child) process");
        return 1;
    }

    if (child != 0) {
        // PARENT

    }
    else {
        // CHILD 1

        close(mypipefdA[0]);

        close(mypipefdB[0]);
        close(mypipefdB[1]);

        close(mypipefdC[0]);
        close(mypipefdC[1]);

        producerAndGenerator(mypipefdA[1], dateCountToGenerate, dateCountPerSecond);

        close(mypipefdA[1]);
        exit(EXIT_SUCCESS);

    }

    pid_t child2 = fork();

    if (child2 < 0) {
        perror("Unable to create new child(second child) process");
        return 1;
    }

    if (child2 != 0) {
        // PARENT

    }
    else {
        // CHILD 2
        close(mypipefdA[1]);
        close(mypipefdB[0]);
        close(mypipefdC[0]);

        middleman(mypipefdA[0], mypipefdB[1], mypipefdC[1], N);

        close(mypipefdA[0]);
        close(mypipefdB[1]);
        close(mypipefdC[1]);

        exit(EXIT_SUCCESS);
    }

    pid_t child3 = fork();

    if (child3 < 0) {
        perror("Unable to create new child(third child) process");
        return 1;
    }

    if (child3 != 0) {
        // PARENT

    }
    else {
        // CHILD

        close(mypipefdA[0]);
        close(mypipefdA[1]);

        close(mypipefdB[1]);

        close(mypipefdC[0]);
        close(mypipefdC[1]);

        consumerCHILD3(mypipefdB[0]);

        close(mypipefdB[0]);

        exit(EXIT_SUCCESS);

    }

    pid_t child4 = fork();

    if (child4 < 0) {
        perror("Unable to create new child(fourth child) process");
        return 1;
    }

    if (child4 != 0) {
        // PARENT

    }
    else {
        // CHILD

        close(mypipefdA[0]);
        close(mypipefdA[1]);

        close(mypipefdB[0]);
        close(mypipefdB[1]);

        close(mypipefdC[1]);

        consumerCHILD4(mypipefdC[0]);

        close(mypipefdC[0]);

        exit(EXIT_SUCCESS);
    }

    //PARENT ONLY CODE

    close(mypipefdA[0]);
    close(mypipefdA[1]);
    close(mypipefdB[0]);
    close(mypipefdB[1]);
    close(mypipefdC[0]);
    close(mypipefdC[1]);

    wait(nullptr);
    wait(nullptr);
    wait(nullptr);
    wait(nullptr);

    return 0;
}