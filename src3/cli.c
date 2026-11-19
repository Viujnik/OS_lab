#define _POSIX_C_SOURCE 200809L
#include <errno.h>     /* ERANGE */
#include <limits.h>    /* INT_MAX */
#include <math.h>      /* isfinite */
#include <stdio.h>     /* stdin, stdout, stderr и тп */
#include <stdlib.h>    /* strtol и free */
#include <string.h>    /* memchr, strcmp и strtok */
#include <sys/types.h> /* ssize_t */

#include "cli.h"

/* text указывает на один токен, number — на int в main. При успехе
   через *number записываем число и возвращаем 1. При отказе — 0. */
static int parse_positive_int(const char *text, int *number) {
    /* Отсутствующий токен даёт NULL. Передавать его в strtol нельзя. */
    if (text == NULL)
        return 0;

    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);

    /* end == text: цифр не было. *end != '\0': после числа остался
       хвост, например "abc" в "4abc". ERANGE: число не влезло в long.
       Даже допустимый long может выходить за диапазон положительного int. */
    if (end == text || *end != '\0' || errno == ERANGE ||
        value <= 0 || value > INT_MAX)
        return 0;

    *number = (int) value;
    return 1;
}

/* Читает до первой корректной команды, конца ввода или ошибки чтения.
   number заполняется только для 1 и 2. Плохую строку пропускаем целиком,
   чтобы следующая команда начиналась с чистого разбора. */
int read_command(int *number) {
    /* line хранит адрес буфера, capacity — сколько байтов в нём выделено.
       NULL и 0 говорят getline выделить память самостоятельно. */
    char *line = NULL;
    size_t capacity = 0;

    /* Токены разделяют: пробел, \t — табуляция, \r — возврат каретки,
       \n — новая строка, \v — вертикальная табуляция, \f — перевод страницы.
       Последние два встречаются редко, но тоже считаются пробельными. */
    const char *spaces = " \t\r\n\v\f";
    int command;

    while (1) {
        /* &line и &capacity — адреса самих переменных. getline обновляет
           обе, например если старый буфер мал. Возвращает число прочитанных
           байтов (с '\n', если он был), или -1 при EOF/ошибке. */
        ssize_t length = getline(&line, &capacity, stdin);
        if (length < 0) {
            /* feof сообщает, был ли конец ввода. ferror проверяет,
               была ли ошибка потока. Ошибка выделения памяти тоже даст -1. */
            command = INPUT_END;
            if (!feof(stdin) || ferror(stdin)) {
                perror("getline");
                command = INPUT_ERROR;
            }
            break;
        }

        if (memchr(line, '\0', (size_t) length) != NULL) {
            fputs("Ошибка: нулевой байт во вводе.\n", stderr);
            continue;
        }

        char *word = strtok(line, spaces);
        if (word == NULL)
            continue;

        if (strcmp(word, "0") == 0)
            command = CHANGE_LIB;
        else if (strcmp(word, "1") == 0)
            command = CALC_PI;
        else if (strcmp(word, "2") == 0)
            command = CALC_E;
        else {
            fputs("Ошибка: ожидается команда 0, 1 или 2.\n", stderr);
            continue;
        }

        char *argument = strtok(NULL, spaces);
        if (command == CHANGE_LIB) {
            if (argument != NULL) {
                fputs("Ошибка: команда 0 вводится без аргументов.\n", stderr);
                continue;
            }
        } else if (!parse_positive_int(argument, number) ||
                   strtok(NULL, spaces) != NULL) {
            fputs("Ошибка: нужно одно положительное число типа int.\n", stderr);
            continue;
        }
        break;
    }

    free(line);
    return command;
}

/* Принимает результат библиотеки. Возвращает 0 при ошибке вывода
   и 1, если приложение может прочитать следующую команду. */
int print_result(float value) {
    if (!isfinite(value)) {
        fputs("Ошибка вычисления.\n", stderr);
        return 1;
    }

    if (printf("%.9g\n", (double) value) < 0 || fflush(stdout) == EOF) {
        perror("stdout");
        return 0;
    }
    return 1;
}
