#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>


int main(void) {
    char *msg = nullptr;
    size_t msg_len;

    const ssize_t read_res = getline(&msg, &msg_len, stdin);
    if (read_res == -1) {
        if (feof(stdin)) {
            free(msg);
            return EXIT_SUCCESS;
        }
        perror("[child1]: Ошибка getline");
        free(msg);
        return EXIT_FAILURE;
    }

    for (int i = 0; msg[i] != '\0'; i++) {
        msg[i] = (char) tolower((unsigned char) msg[i]);
    }

    if (printf("%s", msg) < 0) {
        perror("[child1]: Ошибка printf");
        free(msg);
        return EXIT_FAILURE;
    }

    fflush(stdout);

    free(msg);
    return EXIT_SUCCESS;
}
