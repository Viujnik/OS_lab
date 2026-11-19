/* Открывает объявления POSIX, в частности getline, в строгом режиме C11. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>     /* errno и признак выхода за диапазон ERANGE */
#include <limits.h>    /* максимальное значение int — INT_MAX */
#include <math.h>      /* проверка результата isfinite */
#include <stdio.h>     /* stdin, stdout, stderr и функции ввода-вывода */
#include <stdlib.h>    /* strtol и free */
#include <string.h>    /* memchr, strcmp и strtok */
#include <sys/types.h> /* знаковый тип длины ssize_t */

#include "cli.h"

/* text указывает на один токен, number — на int в main. При успехе
   через *number записываем число и возвращаем 1. При отказе — 0. */
static int parse_positive_int(const char *text, int *number) {
    /* Отсутствующий токен даёт NULL. Передавать его в strtol нельзя. */
    if (text == NULL)
        return 0;

    /* strtol читает основание 10 и записывает в end адрес первого
       непрочитанного байта. errno обнуляем перед вызовом, иначе он
       может содержать ошибку от предыдущей операции. */
    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);

    /* end == text: цифр не было. *end != '\0': после числа остался
       хвост, например "abc" в "4abc". ERANGE: число не влезло в long.
       Даже допустимый long может выходить за диапазон положительного int. */
    if (end == text || *end != '\0' || errno == ERANGE ||
        value <= 0 || value > INT_MAX)
        return 0;

    /* Теперь сужение безопасно. Звёздочка даёт доступ к int вызывающего. */
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

    /* Условие 1 истинно. Из цикла выходим через break; continue начинает
       очередное чтение. Так можно пропускать ошибки и пустые строки. */
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

        /* Ищем '\0' ровно среди length полученных байтов. Ноль, который
           getline сам дописал после них, в поиск не входит. Внутренний
           ноль скрыл бы от строковых функций хвост команды. */
        if (memchr(line, '\0', (size_t) length) != NULL) {
            fputs("Ошибка: нулевой байт во вводе.\n", stderr);
            continue;
        }

        /* Первый strtok получает адрес строки. Он пропускает разделители,
           возвращает адрес начала первого токена и ставит '\0' после него.
           NULL при следующем вызове означает продолжить в этой же строке.
           strtok помнит позицию внутри себя. Мы разбираем одну строку за раз.

           Пример для "1 4\n". Буфер в динамической памяти, адрес условный:
               адрес         0x9000  0x9001  0x9002  0x9003  0x9004
               до strtok       '1'     ' '     '4'     '\n'    '\0'
               после двух      '1'     '\0'    '4'     '\0'    '\0'
           word = 0x9000, argument = 0x9002. Оба адреса ведут в line.
           После free(line) использовать word и argument уже нельзя. */
        char *word = strtok(line, spaces);
        if (word == NULL)
            continue;

        /* strcmp возвращает 0, только если обе строки одинаковы. */
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

        /* Для 0 после номера больше ничего быть не должно. Для 1 и 2
           нужен один int. Третий токен означал бы лишний аргумент. */
        char *argument = strtok(NULL, spaces);
        if (command == CHANGE_LIB) {
            if (argument != NULL) {
                fputs("Ошибка: команда 0 вводится без аргументов.\n", stderr);
                continue;
            }
            /* || слева направо. Если число уже ошибочное, первый операнд
               истинный и дополнительный вызов strtok не нужен. */
        } else if (!parse_positive_int(argument, number) ||
                   strtok(NULL, spaces) != NULL) {
            fputs("Ошибка: нужно одно положительное число типа int.\n", stderr);
            continue;
        }
        break;
    }

    /* Буфер принадлежит этой функции. В main уходит код команды и,
       при 1 или 2, скопированное целое число. Адреса токенов не нужны. */
    free(line);
    return command;
}

/* Принимает результат библиотеки. Возвращает 0 при ошибке вывода
   и 1, если приложение может прочитать следующую команду. */
int print_result(float value) {
    /* NAN — договорённость об ошибке аргумента при прямом вызове библиотеки.
       CLI проверил вход раньше, поэтому здесь защищаем границу вывода. */
    if (!isfinite(value)) {
        fputs("Ошибка вычисления.\n", stderr);
        return 1;
    }

    /* %.9g печатает до девяти значащих цифр. float в printf передаётся
       как double. fflush отправляет буфер stdout сейчас, включая случай,
       когда вывод перенаправлен. Оба вызова могут сообщить ошибку. */
    if (printf("%.9g\n", (double) value) < 0 || fflush(stdout) == EOF) {
        perror("stdout");
        return 0;
    }
    return 1;
}
