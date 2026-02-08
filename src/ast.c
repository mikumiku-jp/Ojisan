#include "ast.h"
#include <stdlib.h>
#include <string.h>

AstNode* ast_new_node(AstType type, int line) {
    AstNode* node = malloc(sizeof(AstNode));
    if (node) {
        memset(node, 0, sizeof(AstNode));
        node->type = type;
        node->line = line;
    }
    return node;
}

void ast_free(AstNode* node) {
    if (!node) return;

    switch (node->type) {
        case AST_VAR_DECL:
            free(node->as.var_decl.name);
            ast_free(node->as.var_decl.init);
            break;
        case AST_ASSIGNMENT:
            free(node->as.assignment.name);
            ast_free(node->as.assignment.value);
            break;
        case AST_IF:
            ast_free(node->as.if_stmt.condition);
            ast_free(node->as.if_stmt.then_branch);
            ast_free(node->as.if_stmt.else_branch);
            break;
        case AST_WHILE:
            ast_free(node->as.while_stmt.condition);
            ast_free(node->as.while_stmt.body);
            break;
        case AST_FOR_RANGE:
            free(node->as.for_range.var_name);
            ast_free(node->as.for_range.start);
            ast_free(node->as.for_range.end);
            ast_free(node->as.for_range.body);
            break;
        case AST_FOR_EACH:
            free(node->as.for_each.var_name);
            ast_free(node->as.for_each.collection);
            ast_free(node->as.for_each.body);
            break;
        case AST_FUNC_DECL:
            free(node->as.func_decl.name);
            for (int i = 0; i < node->as.func_decl.param_count; i++) free(node->as.func_decl.params[i]);
            free(node->as.func_decl.params);
            ast_free(node->as.func_decl.body);
            break;
        case AST_CLASS_DECL:
            free(node->as.class_decl.name);
            ast_free(node->as.class_decl.constructor);
            for (int i = 0; i < node->as.class_decl.method_count; i++) ast_free(node->as.class_decl.methods[i]);
            free(node->as.class_decl.methods);
            break;
        case AST_RETURN:
            ast_free(node->as.return_stmt.value);
            break;
        case AST_PRINT:
            ast_free(node->as.print_stmt.value);
            break;
        case AST_TRY:
            ast_free(node->as.try_stmt.try_block);
            free(node->as.try_stmt.catch_var);
            ast_free(node->as.try_stmt.catch_block);
            ast_free(node->as.try_stmt.finally_block);
            break;
        case AST_ARRAY_PUSH:
            free(node->as.array_push.array_name);
            ast_free(node->as.array_push.value);
            break;
        case AST_IMPORT:
            free(node->as.import_stmt.path);
            break;
        case AST_EXPR_STMT:
            ast_free(node->as.expr_stmt.expr);
            break;
        case AST_BLOCK:
            for (int i = 0; i < node->as.block.stmt_count; i++) ast_free(node->as.block.stmts[i]);
            free(node->as.block.stmts);
            break;
        case AST_BINARY:
            ast_free(node->as.binary.left);
            ast_free(node->as.binary.right);
            break;
        case AST_UNARY:
            ast_free(node->as.unary.operand);
            break;
        case AST_LITERAL:
            if (node->as.literal.type == LIT_STR) free(node->as.literal.s_val);
            break;
        case AST_VARIABLE:
            free(node->as.variable.name);
            break;
        case AST_CALL:
            ast_free(node->as.call.callee);
            for (int i = 0; i < node->as.call.arg_count; i++) ast_free(node->as.call.args[i]);
            free(node->as.call.args);
            break;
        case AST_GET:
            ast_free(node->as.get.object);
            free(node->as.get.name);
            break;
        case AST_INDEX_GET:
            ast_free(node->as.index_get.object);
            ast_free(node->as.index_get.index);
            break;
        case AST_INDEX_SET:
            ast_free(node->as.index_set.object);
            ast_free(node->as.index_set.index);
            ast_free(node->as.index_set.value);
            break;
        case AST_ARRAY_LITERAL:
            for (int i = 0; i < node->as.array_literal.count; i++) ast_free(node->as.array_literal.elements[i]);
            free(node->as.array_literal.elements);
            break;
        case AST_DICT_LITERAL:
            for (int i = 0; i < node->as.dict_literal.count; i++) {
                ast_free(node->as.dict_literal.keys[i]);
                ast_free(node->as.dict_literal.values[i]);
            }
            free(node->as.dict_literal.keys);
            free(node->as.dict_literal.values);
            break;
        case AST_INPUT:
            ast_free(node->as.input.prompt);
            break;
        case AST_NEW:
            free(node->as.new_expr.class_name);
            for (int i = 0; i < node->as.new_expr.arg_count; i++) ast_free(node->as.new_expr.args[i]);
            free(node->as.new_expr.args);
            break;
        case AST_CONVERT:
            ast_free(node->as.convert.target);
            break;
        case AST_TYPEOF:
            ast_free(node->as.typeof_expr.target);
            break;
        case AST_RANDOM:
            ast_free(node->as.random_expr.min);
            ast_free(node->as.random_expr.max);
            break;
        default:
            break;
    }
    free(node);
}
