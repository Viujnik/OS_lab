#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include "shared.h"

static int run_child(const char *program, const char *path) {
    pid_t pid = fork();

    if (pid == -1) {
        perror("[parent] fork");
        return -1;
    }

    if (pid == 0) {
        execl(program, program, path, (char *) NULL);
        perror("[parent] execl");
        _exit(127);
    }

    int status;
    pid_t result;

    do {
        result = waitpid(pid, &status, 0);
    } while (result == -1 && errno == EINTR);

    if (result == -1) {
        perror("[parent] waitpid");
        return -1;
    }

    if (!WIFEXITED(status) ||
        WEXITSTATUS(status) != EXIT_SUCCESS) {
        fprintf(stderr,
                "[parent] Процесс %s завершился с ошибкой\n",
                program);
        return -1;
    }

    return 0;
}

static void cleanup(SharedData *data, size_t map_size, int fd, const char *path) {
    if (data != MAP_FAILED) {
        if (munmap(data, map_size) == -1) {
            perror("[parent] munmap");
        }
    }

    if (fd != -1) {
        if (close(fd) == -1) {
            perror("[parent] close");
        }
    }

    if (path != NULL) {
        if (unlink(path) == -1 && errno != ENOENT) {
            perror("[parent] unlink");
        }
    }
}

int main(void) {
    char *input = NULL;
    size_t input_capacity = 0;

    while (1) {
        printf("Введите строку (EOF для выхода): ");
        fflush(stdout);

        ssize_t read_len = getline(&input, &input_capacity, stdin);

        if (read_len == -1) {
            if (feof(stdin)) {
                break;
            }

            perror("[parent] getline");
            free(input);
            return EXIT_FAILURE;
        }

        size_t len = (size_t) read_len;

        if (len > SIZE_MAX - sizeof(SharedData) - 1) {
            fprintf(stderr, "[parent] Строка слишком длинная\n");
            free(input);
            return EXIT_FAILURE;
        }

        size_t map_size = sizeof(SharedData) + len + 1;

        char path[] = "/tmp/lab4_shared_XXXXXX";
        int fd = mkstemp(path);

        if (fd == -1) {
            perror("[parent] mkstemp");
            free(input);
            return EXIT_FAILURE;
        }

        if (ftruncate(fd, (off_t) map_size) == -1) {
            perror("[parent] ftruncate");
            cleanup(MAP_FAILED, 0, fd, path);
            free(input);
            return EXIT_FAILURE;
        }

        SharedData *data = mmap(
            NULL,
            map_size,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            fd,
            0
        );

        if (data == MAP_FAILED) {
            perror("[parent] mmap");
            cleanup(MAP_FAILED, 0, fd, path);
            free(input);
            return EXIT_FAILURE;
        }

        data->len = len;
        memcpy(data->text, input, len + 1);

        if (msync(data, map_size, MS_SYNC) == -1) {
            perror("[parent] msync");
            cleanup(data, map_size, fd, path);
            free(input);
            return EXIT_FAILURE;
        }

        if (run_child("./child1", path) == -1) {
            cleanup(data, map_size, fd, path);
            free(input);
            return EXIT_FAILURE;
        }

        if (run_child("./child2", path) == -1) {
            cleanup(data, map_size, fd, path);
            free(input);
            return EXIT_FAILURE;
        }

        if (len > 0 && data->text[len - 1] == '\n') {
            data->text[len - 1] = '\0';
        }

        printf("Результат: %s\n", data->text);

        cleanup(data, map_size, fd, path);
    }

    free(input);
    return EXIT_SUCCESS;
}
