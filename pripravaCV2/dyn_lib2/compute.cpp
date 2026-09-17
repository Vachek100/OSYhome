//
// Created by vasek on 9/17/26.
//

#include "compute.h"
#include <cstdio>
#include <cstring>

int compute(FILE* filename) {

    char buffer[1024];
    int line = 1;

    while (fgets(buffer, sizeof(buffer), filename) != nullptr) {

        int sum = 0;
        int num;
        int position = 0;
        int read;
        int last = 0;

        while (sscanf(buffer + position, "%d%n", &num, &read) == 1) {
            sum += last;
            last = num;
            position += read;
        }

        if (sum != last) {
            fprintf(stdout, "Radek %d: spatny soucet, ocekavano %d\n",
                    line, sum);
        }

        line++;
    }

    return 0;
}