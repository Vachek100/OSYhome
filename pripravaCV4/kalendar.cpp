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

    sprintf(buffer, "%d.%d.\n", day, month);
}


void producerAndGenerator(int fd, int dateCountToGenerate, int dateCountPerSecond) {

    char buffer[20];

    int delay = 1000000 / dateCountPerSecond;

    for (int i = 0; i < dateCountToGenerate; i++) {

        generateRandomDate(buffer);

        int ret = write(fd, buffer, strlen(buffer));

        if (ret < 0) {
            perror("Unable to write to pipe");
            exit(EXIT_FAILURE);
        }

        usleep(delay);
    }
}


void consumer(int fd) {

    char buffer[1024];
    char remaining[1024] = "";

    while (1) {

        int ret = read(fd, buffer, sizeof(buffer) - 1);

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

            for (int i = 0; g_svatky[i][0] != nullptr; i++) {

                if (strcmp(start, g_svatky[i][0]) == 0) {
                    fprintf(stdout, "%s %s\n", start, g_svatky[i][1]);
                    break;
                }
            }

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

    int mypipefd[2];

    if (pipe(mypipefd) < 0) {
        perror("Unable to create pipe");
        return 1;
    }

    pid_t child = fork();

    if (child < 0) {
        perror("Unable to create new child process");
        return 1;
    }

    if (child != 0) {

        // PARENT
        close(mypipefd[1]);

        consumer(mypipefd[0]);

        close(mypipefd[0]);

        wait(nullptr);

    }
    else {

        // CHILD
        close(mypipefd[0]);

        producerAndGenerator(
            mypipefd[1],
            dateCountToGenerate,
            dateCountPerSecond
        );

        close(mypipefd[1]);
    }

    return 0;
}