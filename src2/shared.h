#ifndef SHARED_H
#define SHARED_H

#include <stddef.h>

typedef struct {
    size_t len; // Длина строки без  '\0'
    char text[]; // Строка переменного размера
} SharedData;

#endif
