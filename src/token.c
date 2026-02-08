#include "token.h"
#include <stdio.h>

void print_token(Token token) {
    printf("%d '%.*s'\n", token.type, token.length, token.start);
}
