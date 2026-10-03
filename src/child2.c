#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

int main() {
    char *msg = nullptr;
    size_t msg_len;

    const ssize_t read_res = getline(&msg, &msg_len, stdin);
    if (read_res == -1) {
        if (feof(stdin)) {
            free(msg);
            return EXIT_SUCCESS;
        }
        perror("[child2]: Ошибка getline");
        free(msg);
        return EXIT_FAILURE;
    }

    for (int i = 0; msg[i] != '\0'; i++) {
        if (msg[i] == ' ') msg[i] = '_';
    }

    if (printf("%s", msg) < 0) {
        perror("[child2 ]: Ошибка printf");
        free(msg);
        return EXIT_FAILURE;
    }

    fflush(stdout);

    free(msg);
    return EXIT_SUCCESS;
}
