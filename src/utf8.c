#include "utf8.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


int utf8_encode(Codepoint cp, char *buf) {
    if (cp <= 0x7F) {
        buf[0] = (char)cp;
        return 1;
    } else if (cp <= 0x7FF) {
        buf[0] = (char)(0xC0 | (cp >> 6));
        buf[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    } else if (cp <= 0xFFFF) {
        buf[0] = (char)(0xE0 | (cp >> 12));
        buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    } else if (cp <= 0x10FFFF) {
        buf[0] = (char)(0xF0 | (cp >> 18));
        buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }
    return 0;
}


int utf8_decode(const char *str, Codepoint *cp) {
    unsigned char c = (unsigned char)str[0];
    if (c == 0) {
        *cp = 0;
        return 0;
    }
    
    if (c <= 0x7F) {
        *cp = c;
        return 1;
    } else if ((c & 0xE0) == 0xC0) {
        *cp = ((c & 0x1F) << 6) | ((unsigned char)str[1] & 0x3F);
        return 2;
    } else if ((c & 0xF0) == 0xE0) {
        *cp = ((c & 0x0F) << 12) |
              (((unsigned char)str[1] & 0x3F) << 6) |
              ((unsigned char)str[2] & 0x3F);
        return 3;
    } else if ((c & 0xF8) == 0xF0) {
        *cp = ((c & 0x07) << 18) |
              (((unsigned char)str[1] & 0x3F) << 12) |
              (((unsigned char)str[2] & 0x3F) << 6) |
              ((unsigned char)str[3] & 0x3F);
        return 4;
    }
    
    *cp = 0xFFFD; 
    return 1;
}

int utf8_strlen(const char *str) {
    int len = 0;
    const char *p = str;
    Codepoint cp;
    while (*p) {
        int w = utf8_decode(p, &cp);
        if (w == 0) break;
        p += w;
        len++;
    }
    return len;
}


char* utf8_normalize_digits(const char* src) {
    
    
    char* dest = malloc(strlen(src) + 1);
    if (!dest) return NULL;
    
    char* d = dest;
    const char* s = src;
    Codepoint cp;
    
    while (*s) {
        int w = utf8_decode(s, &cp);
        if (w == 0) break;
        
        
        if (cp >= 0xFF10 && cp <= 0xFF19) {
            *d++ = (char)(cp - 0xFF10 + '0');
        } else {
            
            for (int i = 0; i < w; i++) {
                *d++ = s[i];
            }
        }
        s += w;
    }
    *d = '\0';
    return dest;
}

bool utf8_is_space(Codepoint cp) {
    
    return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == 0x3000;
}

bool utf8_is_alpha(Codepoint cp) {
    
    
    return (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z');
}

bool utf8_is_digit(Codepoint cp) {
    return (cp >= '0' && cp <= '9'); 
}

bool utf8_is_alnum(Codepoint cp) {
    return utf8_is_alpha(cp) || utf8_is_digit(cp) || (cp == '_');
}
