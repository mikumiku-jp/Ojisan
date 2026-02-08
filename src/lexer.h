#ifndef OJISAN_LEXER_H
#define OJISAN_LEXER_H

#include "token.h"

void lexer_init(const char* source);
Token lexer_scan_token(void);


typedef struct {
    const char* start;
    const char* current;
    int line;
} LexerState;

LexerState lexer_save_state(void);
void lexer_restore_state(LexerState state);

#endif 
