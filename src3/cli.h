#ifndef LAB5_CLI_H
#define LAB5_CLI_H

enum {
    INPUT_ERROR = -2,
    INPUT_END   = -1,
    CHANGE_LIB  = 0,
    CALC_PI     = 1,
    CALC_E      = 2
};

int read_command(int *number);
int print_result(float value);

#endif
