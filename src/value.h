#ifndef OJISAN_VALUE_H
#define OJISAN_VALUE_H

#include <stdbool.h>
#include <stdint.h>
#include "ast.h" 


typedef struct Obj Obj;
typedef struct ObjString ObjString;
typedef struct ObjList ObjList;
typedef struct ObjDict ObjDict;
typedef struct ObjFunc ObjFunc;
typedef struct ObjClass ObjClass;
typedef struct ObjInstance ObjInstance;
typedef struct HashTable HashTable; 

typedef enum {
    VAL_NULL,
    VAL_BOOL,
    VAL_INT,
    VAL_FLOAT,
    VAL_OBJ
} ValueType;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        long long integer;
        double number;
        Obj* obj;
    } as;
} Value;

typedef enum {
    OBJ_STRING,
    OBJ_LIST,
    OBJ_DICT,
    OBJ_FUNC,
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_NATIVE
} ObjType;

struct Obj {
    ObjType type;
    bool is_marked; 
    struct Obj* next; 
};

struct ObjString {
    Obj obj;
    char* chars;
    int length;
    uint32_t hash;
};

struct ObjList {
    Obj obj;
    int count;
    int capacity;
    Value* items;
};

struct ObjDict {
    Obj obj;
    struct HashTable* items; 
    
    
};

struct ObjFunc {
    Obj obj;
    char* name;
    int param_count;
    char** params;
    AstNode* body;
    struct Environment* closure; 
};

struct ObjClass {
    Obj obj;
    char* name;
    ObjFunc* constructor;
    struct HashTable* methods; 
};

struct ObjInstance {
    Obj obj;
    ObjClass* klass;
    struct HashTable* fields; 
};

typedef Value (*NativeFn)(int argCount, Value* args);
typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;


#define IS_NULL(v) ((v).type == VAL_NULL)
#define IS_BOOL(v) ((v).type == VAL_BOOL)
#define IS_INT(v) ((v).type == VAL_INT)
#define IS_FLOAT(v) ((v).type == VAL_FLOAT)
#define IS_OBJ(v) ((v).type == VAL_OBJ)

#define AS_BOOL(v) ((v).as.boolean)
#define AS_INT(v) ((v).as.integer)
#define AS_FLOAT(v) ((v).as.number)
#define AS_OBJ(v) ((v).as.obj)

#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = value}})
#define NULL_VAL ((Value){VAL_NULL, {.number = 0}})
#define INT_VAL(value) ((Value){VAL_INT, {.integer = value}})
#define FLOAT_VAL(value) ((Value){VAL_FLOAT, {.number = value}})
#define OBJ_VAL(object) ((Value){VAL_OBJ, {.obj = (Obj*)object}})


void value_print(Value value);
bool value_equal(Value a, Value b);
const char* value_type_name(Value value);


ObjString* copy_string_value(const char* chars, int length);
ObjList* new_list(void);
ObjDict* new_dict(void);
ObjFunc* new_function(char* name, int param_count, char** params, AstNode* body);
ObjClass* new_class(char* name);
ObjInstance* new_instance(ObjClass* klass);
ObjNative* new_native(NativeFn function);

#endif 
