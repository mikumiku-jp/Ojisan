#ifndef OJISAN_UTF8_H
#define OJISAN_UTF8_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


typedef uint32_t Codepoint;


int utf8_encode(Codepoint cp, char *buf);
int utf8_decode(const char *str, Codepoint *cp);
int utf8_strlen(const char *str);
char* utf8_normalize_digits(const char* src); 
bool utf8_is_space(Codepoint cp);
bool utf8_is_alpha(Codepoint cp);
bool utf8_is_alnum(Codepoint cp);
bool utf8_is_digit(Codepoint cp);

#endif 
