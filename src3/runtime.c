#include <dlfcn.h>  /* dlopen, dlsym, dlerror, dlclose и флаги загрузки */
#include <stdio.h>  /* stdout, stderr и функции печати */
#include <stdlib.h> /* EXIT_SUCCESS и EXIT_FAILURE */

#include "cli.h"
#include "contract.h"

/* Компилятор на macOS определяет __APPLE__. Поэтому уже при сборке
   выбирается правильное расширение. Соседние строковые литералы
   C склеивает: "./liblab5_impl1" ".so" даёт "./liblab5_impl1.so". */
#ifdef __APPLE__
#define LIBRARY_SUFFIX ".dylib"
#else
#define LIBRARY_SUFFIX ".so"
#endif


typedef struct {
    void *handle;
    Function pi;
    Function e;
} Library;

/* Освобождает одну ссылку на библиотеку. Передаём ровно тот handle,
   который вернул dlopen. Возвращает 1 при успехе или 0 с диагностикой.
   После вызова код больше не пользуется адресами из этой библиотеки. */
static int close_library(void *handle) {
    if (dlclose(handle) == 0)
        return 1;

    const char *error = dlerror();
    if (error == NULL)
        error = "неизвестная ошибка";
    fprintf(stderr, "dlclose: %s\n", error);
    return 0;
}

/* Принимает handle и название функции. Возвращает адрес с известным
   нам типом или NULL, если адрес получить не удалось. */
static Function find_function(void *handle, const char *name) {
    /* Очищаем ошибку от предыдущих операций загрузчика. */
    dlerror();

    /* По handle загрузчик определяет объект для поиска. В Linux поиск
       опирается на имена экспортов в динамической таблице символов ELF
       объекта и его зависимостей. Pi и E определены
       в самой библиотеке. dlsym возвращает адрес в текущем процессе.
       Условный пример: база объекта 0x5000, смещение Pi 0x0,
       смещение E 0x100. Адрес E после учёта базы — 0x5100.
       В исходнике вручную базу и смещение не складываем. */
    void *address = dlsym(handle, name);
    const char *error = dlerror();
    if (error != NULL || address == NULL) {
        if (error == NULL)
            error = "пустой адрес";
        fprintf(stderr, "dlsym %s: %s\n", name, error);
        return NULL;
    }

    /* POSIX разрешает преобразовать полученный адрес. dlsym проверяет
       имя, а сигнатуру float(int) мы знаем из общего contract.h. */
    return address;
}

/* version — 1 или 2. out — адрес структуры, куда записать результат.
   Возвращает 1 после записи всех трёх значений. При ошибке возвращает
   0 и оставляет *out без изменений, чтобы нельзя было взять полупустую пару. */
static int open_library(int version, Library *out) {
    const char *path = "./liblab5_impl1" LIBRARY_SUFFIX;
    if (version == 2)
        path = "./liblab5_impl2" LIBRARY_SUFFIX;

    /* "./" отсчитывается от рабочего каталога. RTLD_NOW разрешает
       ссылки загружаемой библиотеки сразу. RTLD_LOCAL ограничивает
       доступ к её экспортам для других загружаемых библиотек.
       | объединяет флаги. NULL означает, что открыть объект не удалось. */
    void *handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL) {
        const char *error = dlerror();
        if (error == NULL)
            error = "неизвестная ошибка";
        fprintf(stderr, "dlopen %s: %s\n", path, error);
        return 0;
    }

    /* Пока проверяем новую библиотеку, старые адреса в main действуют.
       Если одной функции нет, закрываем только что полученный handle. */
    Function pi = find_function(handle, "Pi");
    Function e = find_function(handle, "E");
    if (pi == NULL || e == NULL) {
        close_library(handle);
        return 0;
    }

    out->handle = handle;
    out->pi = pi;
    out->e = e;
    return 1;
}

int main(void) {
    /* Номер и загруженная библиотека принадлежат этому main. В active
       пока нет действительных значений. open_library заполнит её через
       &active, если первая библиотека откроется и обе функции найдутся. */
    int version = 1;
    Library active;
    if (!open_library(version, &active))
        return EXIT_FAILURE;

    int result = EXIT_SUCCESS;
    while (1) {
        int number;
        int command = read_command(&number);
        if (command == INPUT_END)
            break;
        if (command == INPUT_ERROR) {
            result = EXIT_FAILURE;
            break;
        }

        if (command == CHANGE_LIB) {
            /* 0 является командой пользователя. Внутри version — 1 или 2.
               3 - 1 даёт 2, а 3 - 2 даёт 1. next — временное место для
               новой библиотеки. При ошибке продолжаем со старой active. */
            int next_version = 3 - version;
            Library next;
            if (!open_library(next_version, &next))
                continue;

            /* После успеха сохраняем только старую метку для закрытия.
               Присваивание структуры копирует все три поля. Теперь
               дальнейшие вызовы берут адреса из новой библиотеки. */
            void *previous_handle = active.handle;
            active = next;
            version = next_version;
            if (!close_library(previous_handle)) {
                result = EXIT_FAILURE;
                break;
            }
            if (printf("Реализация %d\n", version) < 0 || fflush(stdout) == EOF) {
                perror("stdout");
                result = EXIT_FAILURE;
                break;
            }
            continue;
        }

        /* Для 1 берём адрес active.pi, для 2 — active.e. Запись
           function(number) вызывает код по этому адресу в нашем процессе. */
        Function function = command == CALC_PI ? active.pi : active.e;
        float value = function(number);
        if (!print_result(value)) {
            result = EXIT_FAILURE;
            break;
        }
    }

    /* Даже при ошибке чтения или вывода освобождаем текущую библиотеку. */
    if (!close_library(active.handle))
        return EXIT_FAILURE;
    return result;
}
