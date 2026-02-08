#ifndef OJISAN_GC_H
#define OJISAN_GC_H

#include "value.h"
#include "env.h"

void gc_init(void);
void gc_set_root(Environment* root);
void gc_register_new_object(Obj* obj);
void gc_collect(Environment* root);
void gc_mark_obj(Obj* obj);
void gc_mark_value(Value value);
void gc_mark_env(Environment* env);

#endif 
