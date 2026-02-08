#ifndef OJISAN_AST_H
#define OJISAN_AST_H

#include "token.h"
#include <stdbool.h>


typedef enum {
    
    AST_VAR_DECL,
    AST_ASSIGNMENT,
    AST_IF,
    AST_WHILE,
    AST_FOR_RANGE,
    AST_FOR_EACH,
    AST_FUNC_DECL,
    AST_CLASS_DECL,
    AST_RETURN,
    AST_PRINT,
    AST_BREAK,
    AST_CONTINUE,
    AST_TRY,
    AST_ARRAY_PUSH,
    AST_IMPORT,
    AST_EXPR_STMT,
    AST_BLOCK, 

    
    AST_BINARY,
    AST_UNARY,
    AST_LITERAL,
    AST_VARIABLE,
    AST_CALL,
    AST_GET,       
    AST_SET,       
    AST_INDEX_GET, 
    AST_INDEX_SET, 
    AST_THIS,
    AST_ARRAY_LITERAL,
    AST_DICT_LITERAL,
    AST_INPUT,
    AST_NEW,
    AST_CONVERT,   
    AST_TYPEOF,
    AST_RANDOM
} AstType;

typedef struct AstNode AstNode;


struct AstNode {
    AstType type;
    int line;
    
    union {
        
        struct { char* name; AstNode* init; } var_decl;
        struct { char* name; AstNode* value; } assignment; 
        struct { AstNode* condition; AstNode* then_branch; AstNode* else_branch; } if_stmt; 
        struct { AstNode* condition; AstNode* body; } while_stmt;
        struct { char* var_name; AstNode* start; AstNode* end; AstNode* body; } for_range;
        struct { char* var_name; AstNode* collection; AstNode* body; } for_each;
        struct { char* name; int param_count; char** params; AstNode* body; } func_decl;
        struct { char* name; AstNode* constructor; int method_count; AstNode** methods; } class_decl;
        struct { AstNode* value; } return_stmt;
        struct { AstNode* value; bool is_println; } print_stmt;
        struct { AstNode* try_block; char* catch_var; AstNode* catch_block; AstNode* finally_block; } try_stmt;
        struct { char* array_name; AstNode* value; } array_push;
        struct { char* path; } import_stmt;
        struct { AstNode* expr; } expr_stmt;
        struct { int stmt_count; AstNode** stmts; } block;

        
        struct { TokenType op; AstNode* left; AstNode* right; } binary;
        struct { TokenType op; AstNode* operand; } unary;
        struct { 
            enum { LIT_INT, LIT_FLOAT, LIT_STR, LIT_BOOL, LIT_NULL } type;
            union { long long i_val; double f_val; char* s_val; bool b_val; };
        } literal;
        struct { char* name; } variable;
        struct { AstNode* callee; int arg_count; AstNode** args; } call;
        struct { AstNode* object; char* name; } get;
        struct { AstNode* object; char* name; AstNode* value; } set;
        struct { AstNode* object; AstNode* index; } index_get;
        struct { AstNode* object; AstNode* index; AstNode* value; } index_set;
        
        struct { int count; AstNode** elements; } array_literal;
        struct { int count; AstNode** keys; AstNode** values; } dict_literal;
        struct { AstNode* prompt; } input;
        struct { char* class_name; int arg_count; AstNode** args; } new_expr;
        struct { bool to_string; AstNode* target; } convert; 
        struct { AstNode* target; } typeof_expr;
        struct { AstNode* min; AstNode* max; } random_expr;
    } as;
};


AstNode* ast_new_node(AstType type, int line);
void ast_free(AstNode* node);

#endif 
