#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "shared.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Использование: %s путь_к_файлу\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    int fd = open(argv[1], O_RDWR);

    if (fd == -1) {
        perror("[child2] open");
        return EXIT_FAILURE;
    }

    struct stat st;

    if (fstat(fd, &st) == -1) {
        perror("[child2] fstat");
        close(fd);
        return EXIT_FAILURE;
    }

    if (st.st_size < (off_t) (sizeof(SharedData) + 1)) {
        fprintf(stderr, "[child2] Файл слишком мал\n");
        close(fd);
        return EXIT_FAILURE;
    }

    size_t map_size = st.st_size;

    SharedData *data = mmap(
        NULL,
        map_size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    if (data == MAP_FAILED) {
        perror("[child2] mmap");
        close(fd);
        return EXIT_FAILURE;
    }

    if (data->len >= map_size - sizeof(*data)) {
        fprintf(stderr, "[child2] Некорректная длина строки\n");
        munmap(data, map_size);
        close(fd);
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < data->len; i++) {
        if (data->text[i] == ' ') {
            data->text[i] = '_';
        }
    }

    if (msync(data, map_size, MS_SYNC) == -1) {
        perror("[child2] msync");
        munmap(data, map_size);
        close(fd);
        return EXIT_FAILURE;
    }

    if (munmap(data, map_size) == -1) {
        perror("[child2] munmap");
        close(fd);
        return EXIT_FAILURE;
    }

    if (close(fd) == -1) {
        perror("[child2] close");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
