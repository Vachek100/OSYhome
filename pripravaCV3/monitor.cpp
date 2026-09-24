//
// Created by vasek on 9/24/26.
//

#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <sys/stat.h>
#include <ctime>

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Pouziti: %s nazev souboru\n", argv[0]);
        return 1;
    }

    struct stat info;

    long oldSize = 0;
    long newSize = 0;

    char *fileName = argv[1];
    char buffer[100];

    int fd = open(fileName, O_RDONLY);

    if (fd == -1) {
        fprintf(stderr, "Soubor %s se nepodarilo otevrit\n", fileName);
        return 1;
    }

    int bytesRead = 0;

    // pocatecni stav souboru
    stat(fileName, &info);
    oldSize = info.st_size;

    while (true) {

        sleep(1);

        stat(fileName, &info);
        newSize = info.st_size;

        if (newSize > oldSize) {

            time_t now = time(NULL);

            fprintf(stdout, "[%.*s] Soubor narost o %ld bajtu\n",
                    19,
                    asctime(localtime(&now)),
                    newSize - oldSize);

            lseek(fd, oldSize, SEEK_SET);

            long remaining = newSize - oldSize;

            while (remaining > 0) {

                int toRead = remaining < 100 ? remaining : 100;

                bytesRead = read(fd, buffer, toRead);

                if (bytesRead == -1) {
                    fprintf(stderr, "Chyba pri cteni souboru\n");
                    return 1;
                }

                for (int i = 0; i < bytesRead; i++) {
                    fprintf(stdout, "%c", buffer[i]);
                }

                remaining -= bytesRead;
            }
        }

        if (newSize < oldSize) {

            time_t now = time(NULL);

            fprintf(stdout, "[%s] Soubor se zmensil o %ld bajtu\n",
                    asctime(localtime(&now)),
                    oldSize - newSize);

            lseek(fd, newSize, SEEK_SET);
        }

        oldSize = newSize;
    }

    return 0;
}