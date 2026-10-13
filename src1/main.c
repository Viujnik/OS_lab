#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdnoreturn.h>
#include <time.h>

int quiet_mode = 0;

noreturn void error_exit(const char *func_name, const char *error_msg) {
    fprintf(stderr, "[%s]: Ошибка: %s\n", func_name, error_msg);
    exit(EXIT_FAILURE);
}

void check_pthread_error(const int err, const char *func_name) {
    if (err != 0) {
        fprintf(stderr, "[%s]: Ошибка потока: %s\n", func_name, strerror(err));
        exit(EXIT_FAILURE);
    }
}

double **allocate_matrix(int rows, int cols) {
    double **matrix = malloc(rows * sizeof(double *));
    if (!matrix) {
        error_exit("allocate_matrix", "Не удалось выделить память под строки");
    }
    for (int i = 0; i < rows; i++) {
        matrix[i] = (double *) malloc(cols * sizeof(double));
        if (!matrix[i]) {
            for (int k = 0; k < i; ++k) {
                free(matrix[k]);
            }
            free(matrix);
            error_exit("allocate_matrix", "Не удалось выделить память под столбцы");
        }
    }
    return matrix;
}

void free_matrix(double **matrix, int rows) {
    if (!matrix) return;
    for (int i = 0; i < rows; i++) {
        free(matrix[i]);
    }
    free(matrix);
}

void input_matrix(const char *name, double **matrix, int rows, int cols) {
    if (!quiet_mode) {
        printf("\n--- Ввод матрицы %s (%dx%d) ---\n", name, rows, cols);
    }
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (!quiet_mode) {
                printf("%s[%d][%d] = ", name, i, j);
            }
            if (scanf("%lf", &matrix[i][j]) != 1) {
                error_exit("input_matrix", "Некорректный ввод числа");
            }
        }
    }
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF);
}

void print_matrix(const char *name, double **matrix, int rows, int cols) {
    if (quiet_mode) return;

    printf("\n--- Матрица %s (%dx%d) ---\n", name, rows, cols);
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%8.2f ", matrix[i][j]);
        }
        printf("\n");
    }
}

typedef struct {
    int start_row;
    int end_row;
    int rows;
    int cols;
    int k_rows;
    int k_cols;
    double **src;
    double **dst;
    double **kernel;
} ThreadData;

void *apply_filter_worker(void *arg) {
    ThreadData *data = (ThreadData *) arg;
    int k_center_r = data->k_rows / 2;
    int k_center_c = data->k_cols / 2;

    for (int r = data->start_row; r < data->end_row; r++) {
        for (int c = 0; c < data->cols; c++) {
            double sum = 0.0;

            for (int kr = 0; kr < data->k_rows; kr++) {
                for (int kc = 0; kc < data->k_cols; kc++) {
                    int in_r = r + kr - k_center_r;
                    int in_c = c + kc - k_center_c;

                    if (in_r >= 0 && in_r < data->rows && in_c >= 0 && in_c < data->cols) {
                        sum += data->src[in_r][in_c] * data->kernel[kr][kc];
                    }
                }
            }
            data->dst[r][c] = sum;
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int max_threads = 4;

    int opt;
    while ((opt = getopt(argc, argv, "t:q")) != -1) {
        if (opt == 't') {
            max_threads = atoi(optarg);
            if (max_threads <= 0) {
                error_exit("main", "Количество потоков должно быть > 0");
            }
        } else if (opt == 'q') {
            quiet_mode = 1;
        } else {
            fprintf(stderr, "Использование: %s -t <макс_потоков> [-q]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (!quiet_mode) {
        printf("Ограничение на количество активных потоков: %d\n", max_threads);
        printf("Введите размер исходной матрицы (строки столбцы): ");
    }

    int rows, cols;
    if (scanf("%d %d", &rows, &cols) != 2 || rows <= 0 || cols <= 0) {
        error_exit("main", "Некорректный размер исходной матрицы");
    }

    if (!quiet_mode) {
        printf("Введите размер окна свертки (строки столбцы, желательно нечетные): ");
    }
    int k_rows, k_cols;
    if (scanf("%d %d", &k_rows, &k_cols) != 2 || k_rows <= 0 || k_cols <= 0) {
        error_exit("main", "Некорректный размер матрицы свертки");
    }

    if (!quiet_mode) {
        printf("Введите количество применения фильтра K: ");
    }
    int K_steps;
    if (scanf("%d", &K_steps) != 1 || K_steps <= 0) {
        error_exit("main", "Некорректное количество итераций K");
    }

    double **src_matrix = allocate_matrix(rows, cols);
    double **dst_matrix = allocate_matrix(rows, cols);
    double **kernel = allocate_matrix(k_rows, k_cols);

    input_matrix("Исходная", src_matrix, rows, cols);
    input_matrix("Свертка (Ядро)", kernel, k_rows, k_cols);

    print_matrix("Исходная перед фильтрацией", src_matrix, rows, cols);

    int num_threads = (rows < max_threads) ? rows : max_threads;

    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    ThreadData *thread_data = malloc(num_threads * sizeof(ThreadData));

    if (!threads || !thread_data) {
        error_exit("main", "Ошибка выделения памяти под потоки");
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int step = 0; step < K_steps; step++) {
        int rows_per_thread = rows / num_threads;
        int extra_rows = rows % num_threads;
        int current_row = 0;

        for (int i = 0; i < num_threads; i++) {
            int count = rows_per_thread + (i < extra_rows ? 1 : 0);

            thread_data[i].start_row = current_row;
            thread_data[i].end_row = current_row + count;
            thread_data[i].rows = rows;
            thread_data[i].cols = cols;
            thread_data[i].k_rows = k_rows;
            thread_data[i].k_cols = k_cols;
            thread_data[i].src = src_matrix;
            thread_data[i].dst = dst_matrix;
            thread_data[i].kernel = kernel;

            current_row += count;

            int res = pthread_create(&threads[i], NULL, apply_filter_worker, &thread_data[i]);
            check_pthread_error(res, "pthread_create");
        }

        for (int i = 0; i < num_threads; i++) {
            int res = pthread_join(threads[i], NULL);
            check_pthread_error(res, "pthread_join");
        }

        double **temp = src_matrix;
        src_matrix = dst_matrix;
        dst_matrix = temp;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed_ms = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;

    fprintf(stderr, "[Threads: %d] Чистое время фильтрации: %.2f мс\n", num_threads, elapsed_ms);
    print_matrix("Результат фильтрации", src_matrix, rows, cols);

    free(threads);
    free(thread_data);
    free_matrix(src_matrix, rows);
    free_matrix(dst_matrix, rows);
    free_matrix(kernel, k_rows);

    return 0;
}
