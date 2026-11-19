#include <stdio.h>  /* fputs и stderr для сообщения об ошибке команды */
#include <stdlib.h> /* коды завершения процесса */

#include "cli.h"
#include "contract.h"

int main(void) {
    /* Программа №1 скомпонована с библиотекой 1. Загрузчик подготовил
       ссылки на Pi и E к моменту запуска программы. Вызываем их по имени. */
    while (1) {
        /* number — локальная переменная этого вызова main. &number
           передаёт её адрес. Для 0 и EOF считывать number не нужно. */
        int number;
        int command = read_command(&number);
        if (command == INPUT_END)
            return EXIT_SUCCESS;
        if (command == INPUT_ERROR)
            return EXIT_FAILURE;

        /* Команда 0 нужна программе №2. Здесь библиотеку выбрали при сборке,
           поэтому сообщение отправляется в stderr, а цикл продолжается. */
        if (command == CHANGE_LIB) {
            fputs("Ошибка: команда 0 только для программы №2.\n", stderr);
            continue;
        }

        /* read_command оставил только 1 или 2. Оператор ?: вычисляет ровно
           одну ветку: Pi для 1, E для 2. number копируется как аргумент int. */
        float value = command == CALC_PI ? Pi(number) : E(number);
        if (!print_result(value))
            return EXIT_FAILURE;
    }
}
