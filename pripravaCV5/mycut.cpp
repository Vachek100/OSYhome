//
// Created by vasek on 10/16/26.
//

#include <cstdio>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <cstring>
#include <cstdlib>
#include <time.h>
#include <fcntl.h>


int main(int argc, char **argv) {


    char buffer[1024];
    char remaining[1024] = "";

    while (1) {

        int ret = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);

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


            char *dot = strchr(start, '.');

            if (dot != nullptr) {
                write(STDOUT_FILENO, dot + 1, strlen(dot + 1));
            }

            start = end + 1;
        }

        strcpy(remaining, start);

    }


}
