#ifndef OJISAN_ENV_H
#define OJISAN_ENV_H

#include "value.h"
#include "hashtable.h"

typedef struct Environment Environment;

struct Environment {
    Environment* enclosing;
    HashTable* values; 
    int ref_count;     
};

Environment* env_new(Environment* enclosing);
void env_retain(Environment* env);
void env_release(Environment* env);
void env_define(Environment* env, const char* name, Value value);
bool env_get(Environment* env, const char* name, Value* out_value);
bool env_assign(Environment* env, const char* name, Value value);

#endif 
