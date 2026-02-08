#include "env.h"
#include <stdlib.h>
#include <string.h>

Environment* env_new(Environment* enclosing) {
    Environment* env = malloc(sizeof(Environment));
    env->enclosing = enclosing;
    env->values = table_create();
    env->ref_count = 1;
    if (enclosing) {
        env_retain(enclosing);
    }
    return env;
}

void env_retain(Environment* env) {
    if (env) {
        env->ref_count++;
    }
}

void env_release(Environment* env) {
    if (!env) return;
    env->ref_count--;
    if (env->ref_count <= 0) {
        table_free(env->values);
        if (env->enclosing) {
            env_release(env->enclosing);
        }
        free(env);
    }
}

void env_define(Environment* env, const char* name, Value value) {
    void* old_ptr;
    if (table_get(env->values, name, &old_ptr)) {
        free(old_ptr);
    }
    Value* v = malloc(sizeof(Value));
    *v = value;
    table_set(env->values, name, v);
}

bool env_get(Environment* env, const char* name, Value* out_value) {
    void* ptr;
    if (table_get(env->values, name, &ptr)) {
        *out_value = *(Value*)ptr;
        return true;
    }
    if (env->enclosing) {
        return env_get(env->enclosing, name, out_value);
    }
    return false;
}

bool env_assign(Environment* env, const char* name, Value value) {
    void* ptr;
    if (table_get(env->values, name, &ptr)) {
        *(Value*)ptr = value; 
        return true;
    }
    if (env->enclosing) {
        return env_assign(env->enclosing, name, value);
    }
    return false;
}
