#include "gc.h"
#include <stdlib.h>
#include <stdio.h>
#include "value.h"
#include "hashtable.h"

Obj* vm_objects = NULL; 
static int gc_object_count = 0;       
static int gc_threshold = 256;        
static Environment* gc_root_env = NULL; 

void gc_init(void) {
    vm_objects = NULL;
    gc_object_count = 0;
    gc_threshold = 256;
    gc_root_env = NULL;
}

void gc_set_root(Environment* root) {
    gc_root_env = root;
}

void gc_register_new_object(Obj* obj) {
    obj->next = vm_objects;
    vm_objects = obj;
    gc_object_count++;

    
    if (gc_object_count > gc_threshold && gc_root_env != NULL) {
        gc_collect(gc_root_env);
        
        gc_threshold = gc_object_count < 256 ? 256 : gc_object_count * 2;
    }
}


static void mark_table_value(const char* key, void* value, void* userdata) {
    (void)key;
    (void)userdata;
    if (value != NULL) {
        gc_mark_value(*(Value*)value);
    }
}

void gc_mark_obj(Obj* obj) {
    if (obj == NULL) return;
    if (obj->is_marked) return;
    obj->is_marked = true;

    switch (obj->type) {
        case OBJ_LIST: {
            ObjList* list = (ObjList*)obj;
            for (int i = 0; i < list->count; i++) {
                gc_mark_value(list->items[i]);
            }
            break;
        }
        case OBJ_DICT: {
            ObjDict* dict = (ObjDict*)obj;
            table_iterate(dict->items, mark_table_value, NULL);
            break;
        }
        case OBJ_FUNC: {
            ObjFunc* func = (ObjFunc*)obj;
            if (func->closure) {
                gc_mark_env(func->closure);
            }
            break;
        }
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)obj;
            table_iterate(klass->methods, mark_table_value, NULL);
            if (klass->constructor) {
                gc_mark_obj((Obj*)klass->constructor);
            }
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* inst = (ObjInstance*)obj;
            gc_mark_obj((Obj*)inst->klass);
            table_iterate(inst->fields, mark_table_value, NULL);
            break;
        }
        default: break;
    }
}

void gc_mark_value(Value value) {
    if (IS_OBJ(value)) {
        gc_mark_obj(AS_OBJ(value));
    }
}

void gc_mark_env(Environment* env) {
    while (env != NULL) {
        table_iterate(env->values, mark_table_value, NULL);
        env = env->enclosing;
    }
}

static void free_object(Obj* obj) {
    switch (obj->type) {
        case OBJ_STRING:
            free(((ObjString*)obj)->chars);
            break;
        case OBJ_LIST:
            free(((ObjList*)obj)->items);
            break;
        case OBJ_DICT:
            table_free(((ObjDict*)obj)->items);
            break;
        case OBJ_FUNC:
            free(((ObjFunc*)obj)->name);
            for(int i=0; i<((ObjFunc*)obj)->param_count; i++) free(((ObjFunc*)obj)->params[i]);
            free(((ObjFunc*)obj)->params);
            break;
        case OBJ_CLASS:
            free(((ObjClass*)obj)->name);
            table_free(((ObjClass*)obj)->methods);
            break;
        case OBJ_INSTANCE:
            table_free(((ObjInstance*)obj)->fields);
            break;
        default: break;
    }
    free(obj);
}

void gc_collect(Environment* root) {
    
    if (root != NULL) {
        gc_mark_env(root);
    }

    
    int alive = 0;
    Obj** object = &vm_objects;
    while (*object != NULL) {
        if (!(*object)->is_marked) {
            Obj* unreached = *object;
            *object = unreached->next;
            free_object(unreached);
        } else {
            (*object)->is_marked = false; 
            object = &(*object)->next;
            alive++;
        }
    }
    gc_object_count = alive;
}
