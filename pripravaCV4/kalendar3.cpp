//
// Created by vasek on 10/3/26.
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

#include "svatky.hpp"


void generateRandomDate(char *buffer) {

    int daysInMonth[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    int month = rand() % 12 + 1;
    int maxDaysInMonth = daysInMonth[month - 1];
    int day = rand() % maxDaysInMonth + 1;

    sprintf(buffer, "%d.%d.\n", day, month);
}


void producerAndGenerator(int fd, int dateCountToGenerate, int dateCountPerSecond) {

    char buffer[20];

    int delay = 1000000 / dateCountPerSecond;

    for (int i = 0; i < dateCountToGenerate; i++) {

        generateRandomDate(buffer);

        int ret = write(fd, buffer, strlen(buffer));

        if (ret < 0) {
            perror("Producer and Generator function: Unable to write to pipe");
            exit(EXIT_FAILURE);
        }

        usleep(delay);
    }
}


void middleman(int readFd, int writeFdB) {

    char buffer[1024];
    char remaining[1024] = "";

    while (1) {

        int ret = read(readFd, buffer, sizeof(buffer) - 1);

        if (ret < 0) {
            perror("Middleman function: Unable to read from pipe");
            exit(EXIT_FAILURE);
        }
        else if (ret == 0) {
            printf("Middleman function: Pipe closed. Why?\n");
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

                for (int i = 0; g_svatky[i][0] != nullptr; i++) {

                    if (strcmp(start, g_svatky[i][0]) == 0) {

                        char output[1024];

                        sprintf(output, "%s %s\n", start, g_svatky[i][1]);

                        int ret = write(writeFdB, output, strlen(output));

                        if (ret < 0) {
                            perror("Unable to write to pipe");
                            exit(EXIT_FAILURE);
                        }

                        break;
                    }
                }


            start = end + 1;
        }

        strcpy(remaining, start);
    }
}

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
            printf("Consumer function: Pipe closed. Why?\n");
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

            fprintf(stdout, "%d. %s\n", numLines, start);

            numLines++;

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

    int mypipefdA[2];
    int mypipefdB[2];

    //CREATION OF THE FIRST PIPE
    if (pipe(mypipefdA) < 0) {
        perror("Unable to create first pipe");
        return 1;
    }

    //CREATION OF THE SECOND PIPE
    if (pipe(mypipefdB) < 0) {
        perror("Unable to create second pipe");
        return 1;
    }

    //CREATION OF FIRST CHILD
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

        producerAndGenerator(
            mypipefdA[1],
            dateCountToGenerate,
            dateCountPerSecond
        );

        close(mypipefdA[1]);

        exit(EXIT_SUCCESS);
    }

    // PARENT ONLY


    //CREATION OF SECOND CHILD
    pid_t child2 = fork();

    if (child2 < 0) {
        perror("Unable to create new child(second child) process");
        return 1;
    }

    if (child2 != 0) {
        // PARENT


    }else {
        // CHILD 2
        close(mypipefdA[1]);
        close(mypipefdB[0]);

        middleman(mypipefdA[0], mypipefdB[1]);

        close(mypipefdA[0]);
        close(mypipefdB[1]);

        exit(EXIT_SUCCESS);
    }

    //CREATION OF THIRD CHILD
    pid_t child3 = fork();

    if (child3 < 0) {
        perror("Unable to create new child(third child) process");
        return 1;
    }

    if (child3 != 0) {
        //PARENT
    }else {
        //CHILD
        close(mypipefdA[1]);
        close(mypipefdA[0]);
        close(mypipefdB[1]);

        consumer(mypipefdB[0]);

        close(mypipefdB[0]);

        exit(EXIT_SUCCESS);
    }

    //PARENT ONLY

    close(mypipefdA[1]);
    close(mypipefdA[0]);
    close(mypipefdB[1]);
    close(mypipefdB[0]);

    //waiting for first child
    wait(nullptr);
    //waiting for second child
    wait(nullptr);
    //waiting for third child
    wait(nullptr);

    return 0;
}