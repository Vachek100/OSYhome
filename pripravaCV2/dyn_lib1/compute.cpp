//
// Created by vasek on 9/17/26.
//

#include "compute.h"
#include <cstdio>
#include <cstring>

int compute(FILE* filename)
{
    char buffer[1024];

    while (fgets(buffer, sizeof(buffer), filename) != nullptr) {

        int sum = 0;
        int num;
        int position = 0;
        int read;

        while (sscanf(buffer + position, "%d%n", &num, &read) == 1) {
            sum += num;
            position += read;
        }

        buffer[strcspn(buffer, "\n")] = '\0';

        fprintf(stdout, "%s %d\n",buffer, sum);
    }

    return 0;
}