//
// Created by vasek on 9/17/26.
//

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <stdlib.h>
#include "numgen.h"

int numgen(int rowCount, int numCount) {

    srand(time(NULL));

    int highest = 1000;
    int lowest = 10;
    int randNum = 0;

    int range = (highest - lowest) + 1;

    for (int i = 0; i < rowCount; i++) {

        for (int j = 0; j < numCount; j++) {
            randNum = lowest + rand() % range;
            fprintf(stdout, "%d ", randNum);
        }
        fprintf(stdout, "\n");
    }

    return 0;
}