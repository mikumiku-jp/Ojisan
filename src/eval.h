#ifndef OJISAN_EVAL_H
#define OJISAN_EVAL_H

#include "ast.h"
#include "env.h"
#include "value.h"
#include <setjmp.h>

typedef enum {
    RES_OK,
    RES_RETURN,
    RES_BREAK,
    RES_CONTINUE,
    RES_ERROR
} EvalResultType;

typedef struct {
    EvalResultType type;
    Value value; 
} EvalResult;


typedef struct TryContext {
    jmp_buf buf;
    char error_message[512];
    struct TryContext* prev;
} TryContext;


extern TryContext* current_try_ctx;

EvalResult evaluate(AstNode* node, Environment* env);
void interpret(const char* source);

#endif 
