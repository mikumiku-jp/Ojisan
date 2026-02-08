#include "value.h"
#include "gc.h"
#include "hashtable.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


typedef struct {
    bool first;
} DictPrintContext;


static void dict_print_entry(const char* key, void* value, void* userdata) {
    DictPrintContext* ctx = (DictPrintContext*)userdata;
    if (!ctx->first) {
        printf("、");
    }
    ctx->first = false;
    printf("%s→", key);
    value_print(*(Value*)value);
}

void value_print(Value value) {
    switch (value.type) {
        case VAL_NULL: printf("ナイナイ"); break;
        case VAL_BOOL: printf(AS_BOOL(value) ? "マジ" : "ウソ"); break;
        case VAL_INT: printf("%lld", AS_INT(value)); break;
        case VAL_FLOAT: printf("%g", AS_FLOAT(value)); break;
        case VAL_OBJ:
            switch (AS_OBJ(value)->type) {
                case OBJ_STRING: printf("%s", ((ObjString*)AS_OBJ(value))->chars); break;
                case OBJ_LIST: {
                    ObjList* list = (ObjList*)AS_OBJ(value);
                    printf("【");
                    for (int i = 0; i < list->count; i++) {
                        if (i > 0) printf("、");
                        value_print(list->items[i]);
                    }
                    printf("】");
                    break;
                }
                case OBJ_DICT: {
                    ObjDict* dict = (ObjDict*)AS_OBJ(value);
                    printf("《");
                    DictPrintContext ctx = { .first = true };
                    table_iterate(dict->items, dict_print_entry, &ctx);
                    printf("》");
                    break;
                }
                case OBJ_FUNC: printf("関数「%s」チャンだヨ😁", ((ObjFunc*)AS_OBJ(value))->name ? ((ObjFunc*)AS_OBJ(value))->name : "無名"); break;
                case OBJ_CLASS: printf("クラス「%s」サンだヨ😁", ((ObjClass*)AS_OBJ(value))->name); break;
                case OBJ_INSTANCE: printf("「%s」サンのインスタンスだヨ😁", ((ObjInstance*)AS_OBJ(value))->klass->name); break;
                case OBJ_NATIVE: printf("ネイティブ関数だヨ😁"); break;
            }
            break;
    }
}

bool value_equal(Value a, Value b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_NULL: return true;
        case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
        case VAL_INT: return AS_INT(a) == AS_INT(b);
        case VAL_FLOAT: return AS_FLOAT(a) == AS_FLOAT(b);
        case VAL_OBJ:
            if (AS_OBJ(a)->type == OBJ_STRING && AS_OBJ(b)->type == OBJ_STRING) {
                return strcmp(((ObjString*)AS_OBJ(a))->chars, ((ObjString*)AS_OBJ(b))->chars) == 0;
            }
            return AS_OBJ(a) == AS_OBJ(b);
    }
    return false;
}

const char* value_type_name(Value value) {
    switch (value.type) {
        case VAL_NULL: return "ナイナイダヨ😁";
        case VAL_BOOL: return "真偽値ダヨ😁";
        case VAL_INT: return "整数ダヨ😁";
        case VAL_FLOAT: return "浮動小数点ダヨ😁";
        case VAL_OBJ:
            switch (AS_OBJ(value)->type) {
                case OBJ_STRING: return "文字列ダヨ😁";
                case OBJ_LIST: return "配列ダヨ😁";
                case OBJ_DICT: return "辞書ダヨ😁";
                case OBJ_FUNC: return "関数ダヨ😁";
                case OBJ_CLASS: return "クラスダヨ😁";
                case OBJ_INSTANCE: return "インスタンスダヨ😁";
                case OBJ_NATIVE: return "ネイティブ関数ダヨ😁";
            }
            break;
    }
    return "不明ダヨ😅💦";
}


static Obj* allocate_obj(size_t size, ObjType type) {
    Obj* object = (Obj*)malloc(size); 
    object->type = type;
    object->is_marked = false;
    
    
    extern void gc_register_new_object(Obj* obj);
    gc_register_new_object(object);
    
    return object;
}

ObjString* copy_string_value(const char* chars, int length) {
    char* heapChars = malloc(length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    
    ObjString* hashStr = (ObjString*)allocate_obj(sizeof(ObjString), OBJ_STRING);
    hashStr->chars = heapChars;
    hashStr->length = length;
    hashStr->hash = hash_string(chars, length);
    return hashStr;
}

ObjList* new_list(void) {
    ObjList* list = (ObjList*)allocate_obj(sizeof(ObjList), OBJ_LIST);
    list->count = 0;
    list->capacity = 0;
    list->items = NULL;
    return list;
}

ObjDict* new_dict(void) {
    ObjDict* dict = (ObjDict*)allocate_obj(sizeof(ObjDict), OBJ_DICT);
    dict->items = table_create();
    return dict;
}

ObjFunc* new_function(char* name, int param_count, char** params, AstNode* body) {
    ObjFunc* func = (ObjFunc*)allocate_obj(sizeof(ObjFunc), OBJ_FUNC);
    func->name = name ? strdup(name) : NULL;
    func->param_count = param_count;
    
    
    func->params = malloc(sizeof(char*) * param_count);
    for(int i=0; i<param_count; i++) func->params[i] = strdup(params[i]);
    func->body = body; 
    func->closure = NULL;
    return func;
}

ObjClass* new_class(char* name) {
    ObjClass* klass = (ObjClass*)allocate_obj(sizeof(ObjClass), OBJ_CLASS);
    klass->name = strdup(name);
    klass->constructor = NULL;
    klass->methods = table_create();
    return klass;
}

ObjInstance* new_instance(ObjClass* klass) {
    ObjInstance* instance = (ObjInstance*)allocate_obj(sizeof(ObjInstance), OBJ_INSTANCE);
    instance->klass = klass;
    instance->fields = table_create();
    return instance;
}

ObjNative* new_native(NativeFn function) {
    ObjNative* native = (ObjNative*)allocate_obj(sizeof(ObjNative), OBJ_NATIVE);
    native->function = function;
    return native;
}
