#include <float.h> /* DBL_EPSILON для оценки хвоста ряда */
#include <math.h>  /* NAN, exp и log1p */

#include "contract.h"

float Pi(int K) {
    /* K — число шагов.
       NAN даёт вызывающему способ заметить недопустимый аргумент. */
    if (K <= 0)
        return NAN;

#if IMPLEMENTATION == 1
    /* Нумерация с нуля даёт знаменатели 1, 3, 5, ... . Дробь считаем
       в double, иначе целочисленное деление отбросит дробную часть. */
    double sum = 0;
    for (int i = 0; i < K; ++i) {
        /* % даёт остаток от деления. Чётный индекс даёт плюс.
           Для K=4 получаем 1 - 1/3 + 1/5 - 1/7. */
        double sign = i % 2 == 0 ? 1.0 : -1.0;
        sum += sign / (2.0 * i + 1.0);
    }
    return (float) (4.0 * sum);
#elif IMPLEMENTATION == 2
    /* Каждый шаг умножает на пару 2n/(2n-1) и 2n/(2n+1).
       Индекс n начинается с 1, поэтому знаменатели ненулевые. */
    double product = 1;
    for (int i = 0; i < K; ++i) {
        /* Храним сразу чётный числитель 2n: 2, 4, 6 и так далее. */
        double even = 2.0 * (i + 1.0);
        product *= (even / (even - 1.0)) * (even / (even + 1.0));
    }
    return (float) (2.0 * product);
#else
#error Define IMPLEMENTATION as 1 or 2
#endif
}

float E(int x) {
    /* Первая формула содержит 1/x. Для обеих библиотек принимаем
       одинаковые аргументы, поэтому 0 и отрицательные x дают NAN. */
    if (x <= 0)
        return NAN;
#if IMPLEMENTATION == 1
    /* exp(x * ln(1 + 1/x)) равносильно степени. log1p принимает 1/x
       отдельно, чтобы сохранить точность при прибавлении малого числа к 1. */
    return (float) exp((double) x * log1p(1.0 / x));
#else
    /* sum уже содержит 1/0!. Деление предыдущего члена на индекс
       даёт 1/n! без вычисления растущего целочисленного факториала. */
    double sum = 1.0;
    double term = 1.0;
    for (int i = 0; i < x; ++i) {
        double n = i + 1.0;
        term /= n;
        sum += term;

        /* После 1/n! первый пропущенный член равен 1/(n+1)!.
           Каждый следующий не больше половины предыдущего, поэтому
           весь остаток меньше удвоенного первого пропущенного члена.
           Прерываем цикл только когда эта оценка меньше DBL_EPSILON.
           Это ограничение на отброшенный хвост, а не на ошибку округления. */
        double tail_bound = 2.0 * term / (n + 1.0);
        if (tail_bound < DBL_EPSILON)
            break;
    }
    return (float) sum;
#endif
}
