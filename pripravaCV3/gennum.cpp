//
// Created by vasek on 9/17/26.
//

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <time.h>

int main(int argc, char *argv[]) {

    int maxNumCount = atoi(argv[1]);
    int rowsPerMinute = atoi(argv[2]);

    srand(time(NULL));

    int highest = 1000;
    int lowest = 10;
    int range = (highest - lowest) + 1;

    // čas mezi dvěma řádky v nanosekundách
    long long nanoseconds = 60000000000LL / rowsPerMinute;

    struct timespec delay;
    delay.tv_sec = nanoseconds / 1000000000LL;
    delay.tv_nsec = nanoseconds % 1000000000LL;

    while (1) {

        // náhodný počet čísel od 1 do M
        int numCount = 1 + rand() % maxNumCount;

        for (int j = 0; j < numCount; j++) {
            int randNum = lowest + rand() % range;
            fprintf(stdout, "%d ", randNum);
        }

        fprintf(stdout, "\n");

        // okamžitě zapsat řádek do out.txt
        fflush(stdout);

        // počkat do dalšího řádku
        nanosleep(&delay, NULL);
    }

    return 0;
}