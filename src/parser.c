#include "parser.h"
#include "lexer.h"
#include "error.h" 
#include "utf8.h" 
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static Token current;
static Token previous;
static bool panic_mode = false;
static bool had_error = false;

static void advance() {
    previous = current;
    for (;;) {
        current = lexer_scan_token();
        if (current.type != TOK_ERROR) break;
        error_report(ERR_SYNTAX, current.line, "%.*s", current.length, current.start);
        had_error = true;
    }
}

static bool check(TokenType type) {
    return current.type == type;
}

static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

static void consume(TokenType type, const char* message) {
    if (check(type)) {
        advance();
        return;
    }
    error_report(ERR_SYNTAX, current.line, "%s (got %d)", message, current.type);
    had_error = true;
}

static char* copy_string(Token token) {
    char* str = malloc(token.length + 1);
    if (!str) return NULL;
    memcpy(str, token.start, token.length);
    str[token.length] = '\0';
    return str;
}


static bool is_param_start() {
    if (!check(TOK_IDENTIFIER)) return false;
    
    LexerState lstate = lexer_save_state();
    Token save_current = current;
    Token save_previous = previous;
    advance(); 
    bool result = check(TOK_CHAN); 
    
    lexer_restore_state(lstate);
    current = save_current;
    previous = save_previous;
    return result;
}


static AstNode* statement();
static AstNode* expression();
static AstNode* or_expr();
static AstNode* and_expr();
static AstNode* eq_expr();
static AstNode* cmp_expr();
static AstNode* add_expr();
static AstNode* mul_expr();
static AstNode* unary_expr();
static AstNode* postfix_expr();
static AstNode* primary();
static AstNode* parse_block_until(TokenType* terminators, int term_count);
static bool check_start_of_expr();


AstNode* parse_program(const char* source) {
    lexer_init(source);
    had_error = false;
    panic_mode = false;
    advance();

    
    AstNode* prog = ast_new_node(AST_BLOCK, 0);
    prog->as.block.stmts = NULL;
    prog->as.block.stmt_count = 0;
    int capacity = 0;

    while (!check(TOK_EOF)) {
        AstNode* stmt = statement();
        if (stmt) {
            if (prog->as.block.stmt_count + 1 > capacity) {
                capacity = capacity < 8 ? 8 : capacity * 2;
                prog->as.block.stmts = realloc(prog->as.block.stmts, sizeof(AstNode*) * capacity);
            }
            prog->as.block.stmts[prog->as.block.stmt_count++] = stmt;
        } else {
            advance(); 
        }
    }
    return prog;
}


static AstNode* parse_block_until(TokenType* terminators, int term_count) {
    AstNode* block = ast_new_node(AST_BLOCK, current.line);
    block->as.block.stmts = NULL;
    block->as.block.stmt_count = 0;
    int capacity = 0;

    while (!check(TOK_EOF)) {
        
        bool is_term = false;
        for (int i = 0; i < term_count; i++) {
            if (check(terminators[i])) {
                is_term = true;
                break;
            }
        }
        if (is_term) break;

        AstNode* stmt = statement();
        if (stmt) {
            if (block->as.block.stmt_count + 1 > capacity) {
                capacity = capacity < 8 ? 8 : capacity * 2;
                block->as.block.stmts = realloc(block->as.block.stmts, sizeof(AstNode*) * capacity);
            }
            block->as.block.stmts[block->as.block.stmt_count++] = stmt;
        } else {
             
             if (!check(TOK_EOF)) advance();
        }
    }
    return block;
}

static AstNode* statement() {
    if (match(TOK_TORIYOSE)) { 
        consume(TOK_STRING, "ファイルパスの文字列が必要ダヨ😅💦");
        char* raw = copy_string(previous);
        int len = strlen(raw);
        
        char* path = malloc(len - 6 + 1);
        memcpy(path, raw + 3, len - 6);
        path[len - 6] = '\0';
        free(raw);
        AstNode* node = ast_new_node(AST_IMPORT, previous.line);
        node->as.import_stmt.path = path;
        return node;
    }
    if (match(TOK_CHOTTO_KIITE)) { 
        consume(TOK_IDENTIFIER, "変数名が必要ダヨ😅💦");
        char* name = copy_string(previous);
        consume(TOK_CHAN_WA, "「チャンは」が必要ダヨ😅💦");
        AstNode* init = expression();
        consume(TOK_NANDA, "「ナンダ😘」が必要ダヨ😅💦");
        AstNode* node = ast_new_node(AST_VAR_DECL, previous.line);
        node->as.var_decl.name = name;
        node->as.var_decl.init = init;
        return node;
    }

    if (match(TOK_BOKU_NO)) { 
        consume(TOK_IDENTIFIER, "フィールド名が必要ダヨ😅💦");
        char* name = copy_string(previous);
        consume(TOK_CHAN_WA, "「チャンは」が必要ダヨ😅💦");
        AstNode* val = expression();
        consume(TOK_NI_NACCHATTA, "「ニナッチャッタ😅💦」が必要ダヨ😅💦");
        
        AstNode* node = ast_new_node(AST_SET, previous.line);
        node->as.set.object = ast_new_node(AST_THIS, previous.line);
        node->as.set.name = name;
        node->as.set.value = val;
        return node;
    }

    if (match(TOK_MOSHIKASHITE)) { 
        AstNode* cond = expression();
        consume(TOK_KANA, "「カナ❓」が必要ダヨ😅💦");
        
        
        TokenType terms[] = {TOK_NANCHATTE, TOK_SOUJANAKATTARA, TOK_OKKEE};
        AstNode* thenBranch = parse_block_until(terms, 3);
        
        AstNode* node = ast_new_node(AST_IF, previous.line);
        node->as.if_stmt.condition = cond;
        node->as.if_stmt.then_branch = thenBranch;
        node->as.if_stmt.else_branch = NULL;

        AstNode** current_else_ptr = &node->as.if_stmt.else_branch;
        
        while (match(TOK_NANCHATTE)) {
            AstNode* elseif_cond = expression();
            consume(TOK_KANA, "「カナ❓」が必要ダヨ😅💦");
            AstNode* elseif_block = parse_block_until(terms, 3);
            
            AstNode* new_if = ast_new_node(AST_IF, previous.line);
            new_if->as.if_stmt.condition = elseif_cond;
            new_if->as.if_stmt.then_branch = elseif_block;
            new_if->as.if_stmt.else_branch = NULL;
            
            *current_else_ptr = new_if;
            current_else_ptr = &new_if->as.if_stmt.else_branch;
        }
        
        if (match(TOK_SOUJANAKATTARA)) {
             TokenType term_ok[] = {TOK_OKKEE};
             AstNode* else_block = parse_block_until(term_ok, 1);
             *current_else_ptr = else_block;
        }
        
        consume(TOK_OKKEE, "最後は「オッケー👍」ダヨ😅💦");
        
        
        return node;
    }

    if (match(TOK_KININARU)) { 
        AstNode* cond = expression();
        consume(TOK_AIDA_WA, "「の間はネ😘」が必要ダヨ😅💦");
        TokenType term[] = {TOK_MOU_II};
        AstNode* body = parse_block_until(term, 1);
        consume(TOK_MOU_II, "最後は「もういいカナ😤」ダヨ😅💦");
        
        AstNode* node = ast_new_node(AST_WHILE, previous.line);
        node->as.while_stmt.condition = cond;
        node->as.while_stmt.body = body;
        return node;
    }

    
    if (match(TOK_IDENTIFIER)) {
        Token identToken = previous;
        char* name = copy_string(identToken);

        if (match(TOK_CHAN_WA)) { 
             AstNode* expr = expression();
             consume(TOK_NI_NACCHATTA, "「ニナッチャッタ😅💦」が必要ダヨ😅💦");
             AstNode* node = ast_new_node(AST_ASSIGNMENT, previous.line);
             node->as.assignment.name = name;
             node->as.assignment.value = expr;
             return node;
        } else if (match(TOK_CHAN_GA)) { 
             
             AstNode* expr1 = expression();
             if (match(TOK_KARA)) { 
                 AstNode* expr2 = expression();
                 consume(TOK_MADE_KANKEI, "「まで関係あるんだけどサ😁」が必要ダヨ😅💦");
                 TokenType term[] = {TOK_MOU_II};
                 AstNode* body = parse_block_until(term, 1);
                 consume(TOK_MOU_II, "最後は「もういいカナ😤」ダヨ😅💦");
                 
                 AstNode* node = ast_new_node(AST_FOR_RANGE, previous.line);
                 node->as.for_range.var_name = name;
                 node->as.for_range.start = expr1;
                 node->as.for_range.end = expr2;
                 node->as.for_range.body = body;
                 return node;
             } else if (match(TOK_NO_MEMBER)) { 
                 TokenType term[] = {TOK_MOU_II};
                 AstNode* body = parse_block_until(term, 1);
                 consume(TOK_MOU_II, "最後は「もういいカナ😤」ダヨ😅💦");
                 
                 AstNode* node = ast_new_node(AST_FOR_EACH, previous.line);
                 node->as.for_each.var_name = name;
                 node->as.for_each.collection = expr1;
                 node->as.for_each.body = body;
                 return node;
             } else {
                 error_report(ERR_SYNTAX, current.line, "ループの構文がおかしいヨ😅💦");
                 free(name);
                 return NULL;
             }
        } else if (match(TOK_YARIKATA)) { 
             
             
             char** params = NULL;
             int param_count = 0;
             int param_cap = 0;
             
             while (is_param_start()) {
                 consume(TOK_IDENTIFIER, "引数名が必要ダヨ😅💦");
                 char* pname = copy_string(previous);
                 consume(TOK_CHAN, "「チャン」をつけてネ😘");

                 if (param_count + 1 > param_cap) {
                     param_cap = param_cap < 8 ? 8 : param_cap * 2;
                     params = realloc(params, sizeof(char*) * param_cap);
                 }
                 params[param_count++] = pname;

                 if (!match(TOK_COMMA)) break;
             }

             TokenType term[] = {TOK_YARIKATA_OSHIMAI};
             AstNode* body = parse_block_until(term, 1);
             consume(TOK_YARIKATA_OSHIMAI, "「やり方おしまい❗」が必要ダヨ😅💦");
             
             AstNode* node = ast_new_node(AST_FUNC_DECL, previous.line);
             node->as.func_decl.name = name;
             node->as.func_decl.params = params;
             node->as.func_decl.param_count = param_count;
             node->as.func_decl.body = body;
             return node;
        } else if (match(TOK_SAN_KOTO)) { 
             
             
             AstNode* ctor = NULL;
             AstNode** methods = NULL;
             int method_count = 0;
             int method_cap = 0;
             
             while (!check(TOK_SAN_KOTO_OSHIMAI) && !check(TOK_EOF)) {
                 if (match(TOK_HAJIMEMASHITE)) { 
                     
                     
                     char** params = NULL;
                     int param_count = 0;
                     int param_cap = 0;
                     while (is_param_start()) {
                         consume(TOK_IDENTIFIER, "引数名が必要ダヨ😅💦");
                         char* pname = copy_string(previous);
                         consume(TOK_CHAN, "「チャン」をつけてネ😘");
                         if (param_count + 1 > param_cap) {
                             param_cap = param_cap < 8 ? 8 : param_cap * 2;
                             params = realloc(params, sizeof(char*) * param_cap);
                         }
                         params[param_count++] = pname;
                         if (!match(TOK_COMMA)) break;
                     }

                     TokenType terms[] = {TOK_HAJIME_OSHIMAI};
                     AstNode* body = parse_block_until(terms, 1);
                     consume(TOK_HAJIME_OSHIMAI, "「ハジメマシテおしまい❗」が必要ダヨ😅💦");
                     
                     ctor = ast_new_node(AST_FUNC_DECL, previous.line); 
                     ctor->as.func_decl.name = strdup("constructor");
                     ctor->as.func_decl.params = params;
                     ctor->as.func_decl.param_count = param_count;
                     ctor->as.func_decl.body = body;
                     
                 } else if (match(TOK_IDENTIFIER)) { 
                     
                     char* mname = copy_string(previous);
                     if (check(TOK_SAN_KOTO_OSHIMAI)) {
                         
                         free(mname);
                         break;
                     } else if (match(TOK_YARIKATA)) {
                         
                         char** params = NULL;
                         int param_count = 0;
                         int param_cap = 0;
                         while (is_param_start()) {
                             consume(TOK_IDENTIFIER, "引数名");
                             char* pname = copy_string(previous);
                             consume(TOK_CHAN, "チャン");
                             if (param_count + 1 > param_cap) {
                                 param_cap = param_cap < 8 ? 8 : param_cap * 2;
                                 params = realloc(params, sizeof(char*) * param_cap);
                             }
                             params[param_count++] = pname;
                             if (!match(TOK_COMMA)) break;
                         }
                         
                         TokenType terms[] = {TOK_YARIKATA_OSHIMAI};
                         AstNode* body = parse_block_until(terms, 1);
                         consume(TOK_YARIKATA_OSHIMAI, "「やり方おしまい❗」が必要ダヨ😅💦");
                         
                         AstNode* method = ast_new_node(AST_FUNC_DECL, previous.line);
                         method->as.func_decl.name = mname;
                         method->as.func_decl.params = params;
                         method->as.func_decl.param_count = param_count;
                         method->as.func_decl.body = body;
                         
                         if (method_count + 1 > method_cap) {
                             method_cap = method_cap < 8 ? 8 : method_cap * 2;
                             methods = realloc(methods, sizeof(AstNode*) * method_cap);
                         }
                         methods[method_count++] = method;
                     } else {
                         free(mname);
                         error_report(ERR_SYNTAX, current.line, "クラスの中ではメソッドかコンストラクタしか書けないヨ😅💦");
                         advance(); 
                     }
                 } else {
                     advance(); 
                 }
             }
             consume(TOK_SAN_KOTO_OSHIMAI, "「サンのコトおしまい❗」が必要ダヨ😅💦");
             
             AstNode* node = ast_new_node(AST_CLASS_DECL, previous.line);
             node->as.class_decl.name = name;
             node->as.class_decl.constructor = ctor;
             node->as.class_decl.methods = methods;
             node->as.class_decl.method_count = method_count;
             return node;
        } else if (match(TOK_CHAN_NI)) { 
             AstNode* val = expression();
             consume(TOK_WO_TSUIKA, "「を追加ダヨ😁」が必要ダヨ😅💦");
             AstNode* node = ast_new_node(AST_ARRAY_PUSH, previous.line);
             node->as.array_push.array_name = name;
             node->as.array_push.value = val;
             return node;
        } 
        
        
        {
             
             AstNode* expr;
             if (match(TOK_ONEGAI)) {
                 
                 int call_line = previous.line;
                 AstNode* var = ast_new_node(AST_VARIABLE, previous.line);
                 var->as.variable.name = name;
                 AstNode* call = ast_new_node(AST_CALL, previous.line);
                 call->as.call.callee = var;
                 AstNode** args = NULL;
                 int arg_count = 0;
                 int arg_cap = 0;
                 if (check_start_of_expr() && current.line == call_line) {
                     do {
                         AstNode* arg = expression();
                         if (arg_count + 1 > arg_cap) {
                             arg_cap = arg_cap < 8 ? 8 : arg_cap * 2;
                             args = realloc(args, sizeof(AstNode*) * arg_cap);
                         }
                         args[arg_count++] = arg;
                     } while (match(TOK_COMMA));
                 }
                 call->as.call.args = args;
                 call->as.call.arg_count = arg_count;
                 expr = call;
             } else if (match(TOK_SAN_WO_TSUKURU)) {
                 
                 int call_line = previous.line;
                 AstNode* node = ast_new_node(AST_NEW, previous.line);
                 node->as.new_expr.class_name = name;
                 AstNode** args = NULL;
                 int arg_count = 0;
                 if (check_start_of_expr() && current.line == call_line) {
                     int cap = 0;
                     do {
                         AstNode* arg = expression();
                         if (arg_count + 1 > cap) {
                             cap = cap < 8 ? 8 : cap * 2;
                             args = realloc(args, sizeof(AstNode*) * cap);
                         }
                         args[arg_count++] = arg;
                     } while (match(TOK_COMMA));
                 }
                 node->as.new_expr.args = args;
                 node->as.new_expr.arg_count = arg_count;
                 expr = node;
             } else if (match(TOK_CHAN)) {
                 expr = ast_new_node(AST_VARIABLE, previous.line);
                 expr->as.variable.name = name;
             } else {
                 free(name);
                 error_report(ERR_SYNTAX, current.line, "ステートメントの解釈に失敗したヨ😅💦");
                 return NULL;
             }

             
             if (match(TOK_NAGASA_CHAN)) {
                 AstNode* length_call = ast_new_node(AST_CALL, previous.line);
                 AstNode* fn = ast_new_node(AST_VARIABLE, previous.line);
                 fn->as.variable.name = strdup("長さを教えてヨ😃");
                 length_call->as.call.callee = fn;
                 length_call->as.call.arg_count = 1;
                 length_call->as.call.args = malloc(sizeof(AstNode*));
                 length_call->as.call.args[0] = expr;
                 expr = length_call;
             }

             
             while (match(TOK_NO)) {
                 if (match(TOK_IDENTIFIER)) {
                     char* prop_name = copy_string(previous);
                     if (match(TOK_ONEGAI)) {
                         
                         int call_line = previous.line;
                         AstNode* get = ast_new_node(AST_GET, previous.line);
                         get->as.get.object = expr;
                         get->as.get.name = prop_name;
                         AstNode* call = ast_new_node(AST_CALL, previous.line);
                         call->as.call.callee = get;
                         AstNode** args = NULL;
                         int arg_count = 0;
                         int arg_cap = 0;
                         if (check_start_of_expr() && current.line == call_line) {
                             do {
                                 AstNode* arg = expression();
                                 if (arg_count + 1 > arg_cap) {
                                     arg_cap = arg_cap < 8 ? 8 : arg_cap * 2;
                                     args = realloc(args, sizeof(AstNode*) * arg_cap);
                                 }
                                 args[arg_count++] = arg;
                             } while (match(TOK_COMMA));
                         }
                         call->as.call.args = args;
                         call->as.call.arg_count = arg_count;
                         expr = call;
                     } else if (match(TOK_CHAN)) {
                         
                         if (check(TOK_BANME_CHAN) || check(TOK_BANME_CHAN_WA)) {
                             AstNode* varNode = ast_new_node(AST_VARIABLE, previous.line);
                             varNode->as.variable.name = prop_name;
                             if (match(TOK_BANME_CHAN_WA)) {
                                 AstNode* value = expression();
                                 consume(TOK_NI_NACCHATTA, "「ニナッチャッタ😅💦」が必要ダヨ😅💦");
                                 AstNode* node = ast_new_node(AST_INDEX_SET, previous.line);
                                 node->as.index_set.object = expr;
                                 node->as.index_set.index = varNode;
                                 node->as.index_set.value = value;
                                 return node;
                             } else {
                                 advance(); 
                                 AstNode* node = ast_new_node(AST_INDEX_GET, previous.line);
                                 node->as.index_get.object = expr;
                                 node->as.index_get.index = varNode;
                                 expr = node;
                             }
                         } else {
                             
                             AstNode* get = ast_new_node(AST_GET, previous.line);
                             get->as.get.object = expr;
                             get->as.get.name = prop_name;
                             expr = get;
                         }
                     } else {
                         free(prop_name);
                         break;
                     }
                 } else if (check(TOK_NUMBER)) {
                     
                     AstNode* idx = expression();
                     if (match(TOK_BANME_CHAN_WA)) {
                         AstNode* value = expression();
                         consume(TOK_NI_NACCHATTA, "「ニナッチャッタ😅💦」が必要ダヨ😅💦");
                         AstNode* node = ast_new_node(AST_INDEX_SET, previous.line);
                         node->as.index_set.object = expr;
                         node->as.index_set.index = idx;
                         node->as.index_set.value = value;
                         return node;
                     }
                     consume(TOK_BANME_CHAN, "「番目チャン」が必要ダヨ😅💦");
                     AstNode* index_get = ast_new_node(AST_INDEX_GET, previous.line);
                     index_get->as.index_get.object = expr;
                     index_get->as.index_get.index = idx;
                     expr = index_get;
                 } else if (match(TOK_NAGASA_CHAN)) {
                     
                     AstNode* length_call = ast_new_node(AST_CALL, previous.line);
                     AstNode* fn = ast_new_node(AST_VARIABLE, previous.line);
                     fn->as.variable.name = strdup("長さを教えてヨ😃");
                     length_call->as.call.callee = fn;
                     length_call->as.call.arg_count = 1;
                     length_call->as.call.args = malloc(sizeof(AstNode*));
                     length_call->as.call.args[0] = expr;
                     expr = length_call;
                 } else {
                     break;
                 }
             }

             
             while (check(TOK_TO) || check(TOK_HIKU) || check(TOK_KAKERU) ||
                    check(TOK_WARU) || check(TOK_AMARI) || check(TOK_ONAJI_KANA) ||
                    check(TOK_CHIGAU_KANA) || check(TOK_YORI_UE) || check(TOK_YORI_SHITA) ||
                    check(TOK_IJOU) || check(TOK_IKA) || check(TOK_SHIKAMO) ||
                    check(TOK_MOSHIKUWA)) {
                 advance();
                 TokenType op = previous.type;
                 AstNode* right = expression();
                 AstNode* bin = ast_new_node(AST_BINARY, previous.line);
                 bin->as.binary.op = op;
                 bin->as.binary.left = expr;
                 bin->as.binary.right = right;
                 expr = bin;
             }

             
             if (match(TOK_OHHA)) {
                 AstNode* node = ast_new_node(AST_PRINT, previous.line);
                 node->as.print_stmt.value = expr;
                 node->as.print_stmt.is_println = true;
                 return node;
             }
             if (match(TOK_TSUBUYAKI)) {
                 AstNode* node = ast_new_node(AST_PRINT, previous.line);
                 node->as.print_stmt.value = expr;
                 node->as.print_stmt.is_println = false;
                 return node;
             }
             if (match(TOK_NANDA)) {
                 
                 
                 AstNode* assignment = ast_new_node(AST_ASSIGNMENT, previous.line);
                 if (expr->type == AST_VARIABLE) {
                     assignment->as.assignment.name = strdup(expr->as.variable.name);
                 } else {
                     assignment->as.assignment.name = strdup("");
                 }
                 
                 
                 ast_free(assignment);
             }

             AstNode* node = ast_new_node(AST_EXPR_STMT, previous.line);
             node->as.expr_stmt.expr = expr;
             return node;
        }
    }

    if (match(TOK_KOTAE)) {
        AstNode* expr = expression();
        consume(TOK_DA_YO, "「ダヨ😁」が必要ダヨ😅💦");
        AstNode* node = ast_new_node(AST_RETURN, previous.line);
        node->as.return_stmt.value = expr;
        return node;
    }
    
    
    if (match(TOK_DOKIDOKI)) { 
         TokenType term[] = {TOK_YABAKATTA, TOK_DOKIDOKI_OSHIMAI};
         AstNode* tryBlock = parse_block_until(term, 2);
         
         char* catchVar = NULL;
         AstNode* catchBlock = NULL;
         
         if (match(TOK_YABAKATTA)) {
             consume(TOK_IDENTIFIER, "エラー変数名が必要ダヨ😅💦");
             catchVar = copy_string(previous);
             consume(TOK_CHAN, "「チャン」をつけてネ😘");
             
             TokenType term2[] = {TOK_DOCCHI_NI_SHITEMO, TOK_DOKIDOKI_OSHIMAI};
             catchBlock = parse_block_until(term2, 2);
         }
         
         AstNode* finallyBlock = NULL;
         if (match(TOK_DOCCHI_NI_SHITEMO)) {
             TokenType term3[] = {TOK_DOKIDOKI_OSHIMAI};
             finallyBlock = parse_block_until(term3, 1);
         }
         
         consume(TOK_DOKIDOKI_OSHIMAI, "「ドキドキおしまい❗」が必要ダヨ😅💦");
         
         AstNode* node = ast_new_node(AST_TRY, previous.line);
         node->as.try_stmt.try_block = tryBlock;
         node->as.try_stmt.catch_var = catchVar;
         node->as.try_stmt.catch_block = catchBlock;
         node->as.try_stmt.finally_block = finallyBlock;
         return node;
    }

    if (match(TOK_MOU_MURI)) return ast_new_node(AST_BREAK, previous.line);
    if (match(TOK_TSUGI_IKOU)) return ast_new_node(AST_CONTINUE, previous.line);

    
    AstNode* expr = expression();

    if (match(TOK_OHHA)) {
        AstNode* node = ast_new_node(AST_PRINT, previous.line);
        node->as.print_stmt.value = expr;
        node->as.print_stmt.is_println = true;
        return node;
    }
    if (match(TOK_TSUBUYAKI)) {
        AstNode* node = ast_new_node(AST_PRINT, previous.line);
        node->as.print_stmt.value = expr;
        node->as.print_stmt.is_println = false;
        return node;
    }

    AstNode* node = ast_new_node(AST_EXPR_STMT, previous.line);
    node->as.expr_stmt.expr = expr;
    return node;
}

static AstNode* expression() {
    return or_expr();
}

static AstNode* or_expr() {
    AstNode* expr = and_expr();
    while (match(TOK_MOSHIKUWA)) {
        AstNode* right = and_expr();
        AstNode* node = ast_new_node(AST_BINARY, previous.line);
        node->as.binary.op = TOK_MOSHIKUWA;
        node->as.binary.left = expr;
        node->as.binary.right = right;
        expr = node;
    }
    return expr;
}

static AstNode* and_expr() {
    AstNode* expr = eq_expr();
    while (match(TOK_SHIKAMO)) {
        AstNode* right = eq_expr();
        AstNode* node = ast_new_node(AST_BINARY, previous.line);
        node->as.binary.op = TOK_SHIKAMO;
        node->as.binary.left = expr;
        node->as.binary.right = right;
        expr = node;
    }
    return expr;
}


static AstNode* eq_expr() {
    AstNode* expr = cmp_expr();
    while (match(TOK_ONAJI_KANA) || match(TOK_CHIGAU_KANA)) {
        TokenType op = previous.type;
        AstNode* right = cmp_expr();
        AstNode* node = ast_new_node(AST_BINARY, previous.line);
        node->as.binary.op = op;
        node->as.binary.left = expr;
        node->as.binary.right = right;
        expr = node;
    }
    return expr;
}

static AstNode* cmp_expr() {
    AstNode* expr = add_expr();
    while (match(TOK_YORI_UE) || match(TOK_YORI_SHITA) || match(TOK_IJOU) || match(TOK_IKA)) {
        TokenType op = previous.type;
        AstNode* right = add_expr();
        AstNode* node = ast_new_node(AST_BINARY, previous.line);
        node->as.binary.op = op;
        node->as.binary.left = expr;
        node->as.binary.right = right;
        expr = node;
    }
    return expr;
}

static AstNode* add_expr() {
    AstNode* expr = mul_expr();
    while (match(TOK_TO) || match(TOK_HIKU)) {
        TokenType op = previous.type;
        AstNode* right = mul_expr();
        AstNode* node = ast_new_node(AST_BINARY, previous.line);
        node->as.binary.op = op;
        node->as.binary.left = expr;
        node->as.binary.right = right;
        expr = node;
    }
    return expr;
}

static AstNode* mul_expr() {
    AstNode* expr = unary_expr();
    while (match(TOK_KAKERU) || match(TOK_WARU) || match(TOK_AMARI)) {
        TokenType op = previous.type;
        AstNode* right = unary_expr();
        AstNode* node = ast_new_node(AST_BINARY, previous.line);
        node->as.binary.op = op;
        node->as.binary.left = expr;
        node->as.binary.right = right;
        expr = node;
    }
    return expr;
}

static AstNode* unary_expr() {
    if (match(TOK_MAINASU)) {
        AstNode* operand = unary_expr();
        AstNode* node = ast_new_node(AST_UNARY, previous.line);
        node->as.unary.op = TOK_MAINASU;
        node->as.unary.operand = operand;
        return node;
    }
    if (match(TOK_CHIGAU_YO)) {
        AstNode* operand = postfix_expr();
        AstNode* node = ast_new_node(AST_UNARY, previous.line);
        node->as.unary.op = TOK_CHIGAU_YO;
        node->as.unary.operand = operand;
        return node;
    }
    
    
    if (match(TOK_KATA_WO) || match(TOK_SUUJI_NI) || match(TOK_MOJI_NI) || match(TOK_NAGASA_WO)) {
        
        char* name = NULL;
        if (previous.type == TOK_KATA_WO) name = strdup("型を教えてヨ😃");
        else if (previous.type == TOK_SUUJI_NI) name = strdup("数字にしてネ😘");
        else if (previous.type == TOK_MOJI_NI) name = strdup("文字にしてネ😘");
        else if (previous.type == TOK_NAGASA_WO) name = strdup("長さを教えてヨ😃");
        
        AstNode* func = ast_new_node(AST_VARIABLE, previous.line);
        func->as.variable.name = name;
        
        AstNode* arg = unary_expr(); 
        
        AstNode* call = ast_new_node(AST_CALL, previous.line);
        call->as.call.callee = func;
        call->as.call.arg_count = 1;
        call->as.call.args = malloc(sizeof(AstNode*));
        call->as.call.args[0] = arg;
        
        return call;
    }

    return postfix_expr();
}


static bool check_start_of_expr() {
    return check(TOK_NUMBER) || check(TOK_STRING) ||
           check(TOK_IDENTIFIER) || check(TOK_LBRACKET) ||
           check(TOK_LDICT) || check(TOK_MAJI) ||
           check(TOK_USO) || check(TOK_NAI_NAI) ||
           check(TOK_BOKU_NO) || check(TOK_CHIGAU_YO) ||
           check(TOK_MAINASU) ||
           check(TOK_LPAREN) ||
           
           check(TOK_KATA_WO) || check(TOK_SUUJI_NI) ||
           check(TOK_MOJI_NI) || check(TOK_NAGASA_WO) ||
           check(TOK_RANDOM_CHAN) || check(TOK_OSHIETE_YO);
}


static AstNode* postfix_expr() {
    AstNode* expr = primary();
    
    
    while (true) {
        if (match(TOK_NO)) { 
            if (match(TOK_IDENTIFIER)) {
                Token ident = previous;
                if (match(TOK_ONEGAI)) { 
                    AstNode* get = ast_new_node(AST_GET, previous.line);
                    get->as.get.object = expr;
                    get->as.get.name = copy_string(ident); 
                    
                    AstNode* call = ast_new_node(AST_CALL, previous.line);
                    call->as.call.callee = get;
                    
                    
                    AstNode** args = NULL;
                    int arg_count = 0;
                    int arg_cap = 0;
                    
                    if (check_start_of_expr()) {
                        do {
                            AstNode* arg = expression();
                            if (arg_count + 1 > arg_cap) {
                                arg_cap = arg_cap < 8 ? 8 : arg_cap * 2;
                                args = realloc(args, sizeof(AstNode*) * arg_cap);
                            }
                            args[arg_count++] = arg;
                        } while (match(TOK_COMMA));
                    }
                    
                    call->as.call.args = args;
                    call->as.call.arg_count = arg_count;
                    expr = call;
                } else if (match(TOK_CHAN)) { 
                    
                    if (check(TOK_BANME_CHAN) || check(TOK_BANME_CHAN_WA)) {
                        AstNode* varNode = ast_new_node(AST_VARIABLE, previous.line);
                        varNode->as.variable.name = copy_string(ident);
                        if (match(TOK_BANME_CHAN_WA)) {
                            
                            AstNode* value = expression();
                            consume(TOK_NI_NACCHATTA, "「ニナッチャッタ😅💦」が必要ダヨ😅💦");
                            AstNode* node = ast_new_node(AST_INDEX_SET, previous.line);
                            node->as.index_set.object = expr;
                            node->as.index_set.index = varNode;
                            node->as.index_set.value = value;
                            return node; 
                        } else {
                            advance(); 
                            AstNode* node = ast_new_node(AST_INDEX_GET, previous.line);
                            node->as.index_get.object = expr;
                            node->as.index_get.index = varNode;
                            expr = node;
                        }
                    } else {
                        AstNode* get = ast_new_node(AST_GET, previous.line);
                        get->as.get.object = expr;
                        get->as.get.name = copy_string(ident);
                        expr = get;
                    }
                } else {
                    error_report(ERR_SYNTAX, current.line, "「の」の後はメンバが必要ダヨ😅💦");
                    break;
                }
            } else if (match(TOK_NAGASA_CHAN)) { 
                 AstNode* get = ast_new_node(AST_GET, previous.line);
                 get->as.get.object = expr;
                 get->as.get.name = strdup("length");
                 expr = get;
            } else { 
                 AstNode* index = expression();
                 if (match(TOK_BANME_CHAN_WA)) {
                     
                     AstNode* value = expression();
                     consume(TOK_NI_NACCHATTA, "「ニナッチャッタ😅💦」が必要ダヨ😅💦");
                     AstNode* node = ast_new_node(AST_INDEX_SET, previous.line);
                     node->as.index_set.object = expr;
                     node->as.index_set.index = index;
                     node->as.index_set.value = value;
                     return node; 
                 } else if (match(TOK_BANME_CHAN)) {
                     AstNode* node = ast_new_node(AST_INDEX_GET, previous.line);
                     node->as.index_get.object = expr;
                     node->as.index_get.index = index;
                     expr = node;
                 } else {
                     
                     break;
                 }
            }
        } else if (match(TOK_NAGASA_CHAN)) {
            
            AstNode* length_call = ast_new_node(AST_CALL, previous.line);
            AstNode* fn = ast_new_node(AST_VARIABLE, previous.line);
            fn->as.variable.name = strdup("長さを教えてヨ😃");
            length_call->as.call.callee = fn;
            length_call->as.call.arg_count = 1;
            length_call->as.call.args = malloc(sizeof(AstNode*));
            length_call->as.call.args[0] = expr;
            expr = length_call;
        } else {
            break;
        }
    }
    return expr;
}

static AstNode* primary() {
    if (match(TOK_NUMBER)) {
        AstNode* node = ast_new_node(AST_LITERAL, previous.line);
        char* val = copy_string(previous);
        if (strchr(val, '.')) {
            node->as.literal.type = LIT_FLOAT;
            node->as.literal.f_val = strtod(val, NULL);
        } else {
            node->as.literal.type = LIT_INT;
            node->as.literal.i_val = strtoll(val, NULL, 10);
        }
        free(val);
        return node;
    }
    if (match(TOK_STRING)) {
        AstNode* node = ast_new_node(AST_LITERAL, previous.line);
        node->as.literal.type = LIT_STR;
        char* raw = copy_string(previous);
        int len = strlen(raw);
        if (len >= 6) {
            
            int inner_len = len - 6;
            char* inner = malloc(inner_len + 1);
            
            int j = 0;
            for (int i = 3; i < len - 3; i++) {
                if (raw[i] == '\\' && i + 1 < len - 3) {
                    char next = raw[i + 1];
                    switch (next) {
                        case 'n': inner[j++] = '\n'; i++; break;
                        case 't': inner[j++] = '\t'; i++; break;
                        case 'r': inner[j++] = '\r'; i++; break;
                        case '\\': inner[j++] = '\\'; i++; break;
                        default:
                            
                            if (i + 3 < len - 3 && memcmp(raw + i + 1, "」", 3) == 0) {
                                memcpy(inner + j, "」", 3); j += 3; i += 3;
                            } else {
                                inner[j++] = raw[i]; 
                            }
                            break;
                    }
                } else {
                    inner[j++] = raw[i];
                }
            }
            inner[j] = '\0';
            free(raw);
            node->as.literal.s_val = inner;
        } else {
            node->as.literal.s_val = raw;
        }
        return node;
    }
    if (match(TOK_MAJI)) {
        AstNode* node = ast_new_node(AST_LITERAL, previous.line);
        node->as.literal.type = LIT_BOOL;
        node->as.literal.b_val = true;
        return node;
    }
    if (match(TOK_USO)) {
        AstNode* node = ast_new_node(AST_LITERAL, previous.line);
        node->as.literal.type = LIT_BOOL;
        node->as.literal.b_val = false;
        return node;
    }
    if (match(TOK_NAI_NAI)) {
        AstNode* node = ast_new_node(AST_LITERAL, previous.line);
        node->as.literal.type = LIT_NULL;
        return node;
    }
    
    
    if (match(TOK_RANDOM_CHAN)) {
        
        AstNode* func = ast_new_node(AST_VARIABLE, previous.line);
        func->as.variable.name = strdup("ランダムチャン😃");
        AstNode* call = ast_new_node(AST_CALL, previous.line);
        call->as.call.callee = func;
        call->as.call.arg_count = 0;
        call->as.call.args = NULL;
        return call;
    }
    
    if (match(TOK_LBRACKET)) { 
        
        AstNode** elements = NULL;
        int count = 0;
        int cap = 0;
        if (!check(TOK_RBRACKET)) {
             do {
                 AstNode* elem = expression();
                 if (count + 1 > cap) {
                     cap = cap < 8 ? 8 : cap * 2;
                     elements = realloc(elements, sizeof(AstNode*) * cap);
                 }
                 elements[count++] = elem;
             } while (match(TOK_COMMA));
        }
        consume(TOK_RBRACKET, "】が必要ダヨ😅💦");
        AstNode* node = ast_new_node(AST_ARRAY_LITERAL, previous.line);
        node->as.array_literal.elements = elements;
        node->as.array_literal.count = count;
        return node;
    }
    
    if (match(TOK_LDICT)) { 
        AstNode** keys = NULL;
        AstNode** values = NULL;
        int count = 0;
        int cap = 0;
        if (!check(TOK_RDICT)) {
            do {
                AstNode* key = expression();
                consume(TOK_ARROW, "「→」が必要ダヨ😅💦");
                AstNode* val = expression();
                if (count + 1 > cap) {
                    cap = cap < 8 ? 8 : cap * 2;
                    keys = realloc(keys, sizeof(AstNode*) * cap);
                    values = realloc(values, sizeof(AstNode*) * cap);
                }
                keys[count] = key;
                values[count] = val;
                count++;
            } while (match(TOK_COMMA));
        }
        consume(TOK_RDICT, "》が必要ダヨ😅💦");
        AstNode* node = ast_new_node(AST_DICT_LITERAL, previous.line);
        node->as.dict_literal.keys = keys;
        node->as.dict_literal.values = values;
        node->as.dict_literal.count = count;
        return node;
    }

    if (match(TOK_OSHIETE_YO)) { 
        AstNode* func = ast_new_node(AST_VARIABLE, previous.line);
        func->as.variable.name = strdup("チョット教えてヨ😃");
        AstNode* call = ast_new_node(AST_CALL, previous.line);
        call->as.call.callee = func;
        
        if (check_start_of_expr()) {
            call->as.call.arg_count = 1;
            call->as.call.args = malloc(sizeof(AstNode*));
            call->as.call.args[0] = expression();
        } else {
            call->as.call.arg_count = 0;
            call->as.call.args = NULL;
        }
        return call;
    }

    if (match(TOK_LPAREN)) { 
         AstNode* expr = expression();
         consume(TOK_RPAREN, "）が必要ダヨ😅💦");
         return expr;
    }

    
    if (match(TOK_IDENTIFIER)) {
        
        Token ident = previous;
        char* name = copy_string(ident);
        
        if (match(TOK_ONEGAI)) { 
             AstNode* node = ast_new_node(AST_CALL, previous.line);
             AstNode* var = ast_new_node(AST_VARIABLE, previous.line);
             var->as.variable.name = name;
             node->as.call.callee = var;
             
             AstNode** args = NULL;
             int arg_count = 0;
             int arg_cap = 0;
             
             if (check_start_of_expr()) {
                 do {
                     AstNode* arg = expression();
                     if (arg_count + 1 > arg_cap) {
                         arg_cap = arg_cap < 8 ? 8 : arg_cap * 2;
                         args = realloc(args, sizeof(AstNode*) * arg_cap);
                     }
                     args[arg_count++] = arg;
                 } while (match(TOK_COMMA));
             }
             
             node->as.call.args = args;
             node->as.call.arg_count = arg_count;
             return node;
        } else if (match(TOK_SAN_WO_TSUKURU)) { 
             AstNode* node = ast_new_node(AST_NEW, previous.line);
             node->as.new_expr.class_name = name;
             
             
             AstNode** args = NULL;
             int arg_count = 0;
             if (check_start_of_expr()) {
                 int cap = 0;
                 do {
                     AstNode* arg = expression();
                     if (arg_count + 1 > cap) {
                         cap = cap < 8 ? 8 : cap * 2;
                         args = realloc(args, sizeof(AstNode*) * cap);
                     }
                     args[arg_count++] = arg;
                 } while (match(TOK_COMMA));
             }
             
             node->as.new_expr.args = args;
             node->as.new_expr.arg_count = arg_count;
             return node;
        } else if (match(TOK_CHAN)) { 
             AstNode* node = ast_new_node(AST_VARIABLE, previous.line);
             node->as.variable.name = name;
             return node;
        } else {
             free(name);
             error_report(ERR_SYNTAX, current.line, "変数なら「チャン」をつけてネ😅💦");
             return NULL;
        }
    }
    
    if (match(TOK_BOKU_NO)) { 
        consume(TOK_IDENTIFIER, "フィールド名が必要ダヨ😅💦");
        char* field = copy_string(previous);
        consume(TOK_CHAN, "「チャン」をつけてネ😘");
        AstNode* node = ast_new_node(AST_GET, previous.line);
        node->as.get.object = ast_new_node(AST_THIS, previous.line);
        node->as.get.name = field;
        return node;
    }

    error_report(ERR_SYNTAX, current.line, "式が期待されてるヨ😅💦");
    if (!check(TOK_EOF)) advance(); 
    return NULL;
}
