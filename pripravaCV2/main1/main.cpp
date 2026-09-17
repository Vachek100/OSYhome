//
// Created by vasek on 9/17/26.
//

#include <cstdlib>
#include "numgen.h"

int main(int argc, char * argv[]) {

    int rowCount = atoi(argv[1]);
    int numCount = atoi(argv[2]);

    numgen(rowCount, numCount);

    return 0;
}