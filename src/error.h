#ifndef OJISAN_ERROR_H
#define OJISAN_ERROR_H

#include <stdarg.h>

typedef enum {
    ERR_SYNTAX,
    ERR_RUNTIME,
    ERR_TYPE,
    ERR_UNDEFINED,
    ERR_ZERO_DIV,
    ERR_INDEX_OUT_OF_BOUNDS,
    ERR_ARGUMENT_COUNT
} ErrorType;

void error_report(ErrorType type, int line, const char* fmt, ...);
void error_print_raw(const char* msg);

#endif 
