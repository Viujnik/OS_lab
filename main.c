#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>


void error_exit(const char *func_name, const char *error_msg) {
    fprintf(stderr, "[%s]: Ошибка %s\n", func_name, error_msg);
    exit(EXIT_FAILURE);
}

size_t read_msg(char **msg) {
    size_t msg_len = 0;
    const ssize_t read_res = getline(msg, &msg_len, stdin);

    if (read_res == -1) {
        if (feof(stdin))
            printf("Сообщение длиной %lu байт было прочитано со стандартного ввода(stdin): EOF\n",
                   msg_len);
        else error_exit("getline", strerror(errno));
        return 0;
    }
    for (ssize_t i = 0; i < read_res; i++) {
        if ((*msg)[i] == '\n') {
            printf("Символ перевода строки найден на индексе %ld\n", i);
        }
    }
    return read_res;
}

int main(void) {
    char *msg = nullptr;

    while (1) {
        const size_t msg_len = read_msg(&msg);
        if (msg_len == 0 && feof(stdin)) {
            break;
        }
        printf("Прочитано %lu байт\n", msg_len);

        int pipe1fd[2], pipe2fd[2], pipe3fd[2];
        if (pipe(pipe1fd) == -1) error_exit("pipe", strerror(errno));
        if (pipe(pipe2fd) == -1) {
            const int saved_errno = errno;
            close(pipe1fd[0]);
            close(pipe1fd[1]);
            error_exit("pipe", strerror(saved_errno));
        }
        if (pipe(pipe3fd) == -1) {
            const int saved_errno = errno;
            close(pipe1fd[0]);
            close(pipe1fd[1]);
            close(pipe2fd[0]);
            close(pipe2fd[1]);
            error_exit("pipe", strerror(saved_errno));
        }
        const pid_t pid1 = fork();
        if (pid1 == -1) error_exit("fork", strerror(errno));

        if (pid1 == 0) {
            dup2(pipe1fd[0], STDIN_FILENO);
            dup2(pipe2fd[1], STDOUT_FILENO);

            close(pipe1fd[0]);
            close(pipe1fd[1]);
            close(pipe2fd[0]);
            close(pipe2fd[1]);
            close(pipe3fd[0]);
            close(pipe3fd[1]);

            execl("./child1", "./child1", nullptr);

            error_exit("execl", strerror(errno));
        }

        const pid_t pid2 = fork();
        if (pid2 == -1) {
            kill(pid1, SIGKILL);
            error_exit("fork", strerror(errno));
        }

        if (pid2 == 0) {
            dup2(pipe2fd[0], STDIN_FILENO);
            dup2(pipe3fd[1], STDOUT_FILENO);

            close(pipe1fd[0]);
            close(pipe1fd[1]);
            close(pipe2fd[0]);
            close(pipe2fd[1]);
            close(pipe3fd[0]);
            close(pipe3fd[1]);

            execl("./child2", "./child2", nullptr);

            error_exit("execl", strerror(errno));
        }

        close(pipe1fd[0]);
        close(pipe2fd[0]);
        close(pipe2fd[1]);
        close(pipe3fd[1]);

        const ssize_t write_res = write(pipe1fd[1], msg, msg_len);
        if (write_res == -1) error_exit("write", strerror(errno));

        close(pipe1fd[1]);

        memset(msg, 0, msg_len);

        const ssize_t read_res = read(pipe3fd[0], msg, msg_len);
        if (read_res == -1) error_exit("read", strerror(errno));

        if (read_res > 0) {
            if (msg[read_res - 1] == '\n') {
                msg[read_res - 1] = '\0';
            } else {
                msg[read_res] = '\0';
            }
        }

        printf("Результат: %s\n", msg);

        close(pipe3fd[0]);

        waitpid(pid1, nullptr, 0);
        waitpid(pid2, nullptr, 0);
    }

    free(msg);
    exit(EXIT_SUCCESS);
}
