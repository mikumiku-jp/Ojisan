#include "eval.h"
#include "error.h"
#include "builtins.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include "gc.h"


TryContext* current_try_ctx = NULL;


#define MAX_CALL_DEPTH 1000
static int call_depth = 0;


#define MAX_IMPORTS 256
static char* imported_paths[MAX_IMPORTS];
static int import_count = 0;


#define RETURN_OK(v) return (EvalResult){RES_OK, v}
#define RETURN_ERR() return (EvalResult){RES_ERROR, NULL_VAL}
#define IS_TRUTHY(v) (!IS_NULL(v) && (!IS_BOOL(v) || AS_BOOL(v)))

static EvalResult exec_block(AstNode* node, Environment* env);
static EvalResult call_function(ObjFunc* func, int argCount, Value* args);
static EvalResult call_native(ObjNative* native, int argCount, Value* args);


typedef struct { ObjList* list; } ForEachDictCtx;
static void for_each_dict_callback(const char* key, void* val, void* userdata) {
    (void)val;
    ForEachDictCtx* ctx = (ForEachDictCtx*)userdata;
    ObjString* s = copy_string_value(key, strlen(key));
    if (ctx->list->count + 1 > ctx->list->capacity) {
        ctx->list->capacity = ctx->list->capacity < 8 ? 8 : ctx->list->capacity * 2;
        ctx->list->items = realloc(ctx->list->items, sizeof(Value) * ctx->list->capacity);
    }
    ctx->list->items[ctx->list->count++] = OBJ_VAL(s);
}

EvalResult evaluate(AstNode* node, Environment* env) {
    if (!node) RETURN_ERR();

    switch (node->type) {
        case AST_LITERAL: {
            switch (node->as.literal.type) {
                case LIT_INT: RETURN_OK(INT_VAL(node->as.literal.i_val));
                case LIT_FLOAT: RETURN_OK(FLOAT_VAL(node->as.literal.f_val));
                case LIT_STR: RETURN_OK(OBJ_VAL(copy_string_value(node->as.literal.s_val, strlen(node->as.literal.s_val))));
                case LIT_BOOL: RETURN_OK(BOOL_VAL(node->as.literal.b_val));
                case LIT_NULL: RETURN_OK(NULL_VAL);
            }
            break;
        }
        case AST_BINARY: {
            
            if (node->as.binary.op == TOK_SHIKAMO) {
                EvalResult left = evaluate(node->as.binary.left, env);
                if (left.type != RES_OK) return left;
                if (!IS_TRUTHY(left.value)) RETURN_OK(BOOL_VAL(false));
                EvalResult right = evaluate(node->as.binary.right, env);
                if (right.type != RES_OK) return right;
                RETURN_OK(BOOL_VAL(IS_TRUTHY(right.value)));
            }
            if (node->as.binary.op == TOK_MOSHIKUWA) {
                EvalResult left = evaluate(node->as.binary.left, env);
                if (left.type != RES_OK) return left;
                if (IS_TRUTHY(left.value)) RETURN_OK(BOOL_VAL(true));
                EvalResult right = evaluate(node->as.binary.right, env);
                if (right.type != RES_OK) return right;
                RETURN_OK(BOOL_VAL(IS_TRUTHY(right.value)));
            }

            EvalResult left = evaluate(node->as.binary.left, env);
            if (left.type != RES_OK) return left;
            EvalResult right = evaluate(node->as.binary.right, env);
            if (right.type != RES_OK) return right;

            Value l = left.value;
            Value r = right.value;

            
            #define NUM_AS_DOUBLE(v) (IS_INT(v) ? (double)AS_INT(v) : AS_FLOAT(v))
            #define IS_NUM(v) (IS_INT(v) || IS_FLOAT(v))

            switch (node->as.binary.op) {
                
                case TOK_TO: 
                    
                    if (IS_INT(l) && IS_INT(r)) {
                        long long a = AS_INT(l), b = AS_INT(r);
                        if ((b > 0 && a > LLONG_MAX - b) || (b < 0 && a < LLONG_MIN - b)) {
                            
                            RETURN_OK(FLOAT_VAL((double)a + (double)b));
                        }
                        RETURN_OK(INT_VAL(a + b));
                    }
                    if (IS_NUM(l) && IS_NUM(r)) RETURN_OK(FLOAT_VAL(NUM_AS_DOUBLE(l) + NUM_AS_DOUBLE(r)));
                    
                    if (IS_OBJ(l) && AS_OBJ(l)->type == OBJ_STRING && IS_OBJ(r) && AS_OBJ(r)->type == OBJ_STRING) {
                         ObjString* s1 = (ObjString*)AS_OBJ(l);
                         ObjString* s2 = (ObjString*)AS_OBJ(r);
                         char* newStr = malloc(s1->length + s2->length + 1);
                         strcpy(newStr, s1->chars);
                         strcat(newStr, s2->chars);
                         ObjString* result = copy_string_value(newStr, s1->length + s2->length);
                         free(newStr);
                         RETURN_OK(OBJ_VAL(result));
                    }
                    
                    {
                        bool l_is_str = IS_OBJ(l) && AS_OBJ(l)->type == OBJ_STRING;
                        bool r_is_str = IS_OBJ(r) && AS_OBJ(r)->type == OBJ_STRING;
                        if (l_is_str || r_is_str) {
                            char lbuf[64] = {0}, rbuf[64] = {0};
                            const char* ls; const char* rs;
                            if (l_is_str) { ls = ((ObjString*)AS_OBJ(l))->chars; }
                            else if (IS_INT(l)) { snprintf(lbuf, sizeof(lbuf), "%lld", AS_INT(l)); ls = lbuf; }
                            else if (IS_FLOAT(l)) { snprintf(lbuf, sizeof(lbuf), "%g", AS_FLOAT(l)); ls = lbuf; }
                            else if (IS_BOOL(l)) { ls = AS_BOOL(l) ? "マジ" : "ウソ"; }
                            else if (IS_NULL(l)) { ls = "ナイナイ"; }
                            else { ls = ""; }
                            if (r_is_str) { rs = ((ObjString*)AS_OBJ(r))->chars; }
                            else if (IS_INT(r)) { snprintf(rbuf, sizeof(rbuf), "%lld", AS_INT(r)); rs = rbuf; }
                            else if (IS_FLOAT(r)) { snprintf(rbuf, sizeof(rbuf), "%g", AS_FLOAT(r)); rs = rbuf; }
                            else if (IS_BOOL(r)) { rs = AS_BOOL(r) ? "マジ" : "ウソ"; }
                            else if (IS_NULL(r)) { rs = "ナイナイ"; }
                            else { rs = ""; }
                            int ll = strlen(ls), rl = strlen(rs);
                            char* newStr = malloc(ll + rl + 1);
                            memcpy(newStr, ls, ll);
                            memcpy(newStr + ll, rs, rl);
                            newStr[ll + rl] = '\0';
                            ObjString* result = copy_string_value(newStr, ll + rl);
                            free(newStr);
                            RETURN_OK(OBJ_VAL(result));
                        }
                    }
                    break;
                case TOK_HIKU:
                    if (IS_INT(l) && IS_INT(r)) RETURN_OK(INT_VAL(AS_INT(l) - AS_INT(r)));
                    if (IS_NUM(l) && IS_NUM(r)) RETURN_OK(FLOAT_VAL(NUM_AS_DOUBLE(l) - NUM_AS_DOUBLE(r)));
                    break;
                case TOK_KAKERU:
                    
                    if (IS_INT(l) && IS_INT(r)) {
                        long long a = AS_INT(l), b = AS_INT(r);
                        if (a != 0 && b != 0 && ((a > 0) == (b > 0)
                            ? (a > LLONG_MAX / b) : (a < LLONG_MIN / b))) {
                            RETURN_OK(FLOAT_VAL((double)a * (double)b));
                        }
                        RETURN_OK(INT_VAL(a * b));
                    }
                    if (IS_NUM(l) && IS_NUM(r)) RETURN_OK(FLOAT_VAL(NUM_AS_DOUBLE(l) * NUM_AS_DOUBLE(r)));
                    break;
                case TOK_WARU:
                     if (IS_INT(l) && IS_INT(r)) {
                        if (AS_INT(r) == 0) { error_report(ERR_ZERO_DIV, node->line, "0で割っちゃダメだヨ😱💦"); RETURN_ERR(); }
                        RETURN_OK(INT_VAL(AS_INT(l) / AS_INT(r)));
                     }
                     if (IS_NUM(l) && IS_NUM(r)) {
                        if (NUM_AS_DOUBLE(r) == 0.0) { error_report(ERR_ZERO_DIV, node->line, "0で割っちゃダメだヨ😱💦"); RETURN_ERR(); }
                        RETURN_OK(FLOAT_VAL(NUM_AS_DOUBLE(l) / NUM_AS_DOUBLE(r)));
                     }
                     break;
                case TOK_AMARI:
                     if (IS_INT(l) && IS_INT(r)) {
                        if (AS_INT(r) == 0) { error_report(ERR_ZERO_DIV, node->line, "0で割っちゃダメだヨ😱💦"); RETURN_ERR(); }
                        RETURN_OK(INT_VAL(AS_INT(l) % AS_INT(r)));
                     }
                     if (IS_NUM(l) && IS_NUM(r)) {
                        if (NUM_AS_DOUBLE(r) == 0.0) { error_report(ERR_ZERO_DIV, node->line, "0で割っちゃダメだヨ😱💦"); RETURN_ERR(); }
                        RETURN_OK(FLOAT_VAL(fmod(NUM_AS_DOUBLE(l), NUM_AS_DOUBLE(r))));
                     }
                     break;

                
                case TOK_ONAJI_KANA: RETURN_OK(BOOL_VAL(value_equal(l, r)));
                case TOK_CHIGAU_KANA: RETURN_OK(BOOL_VAL(!value_equal(l, r)));
                case TOK_YORI_UE:
                    if (IS_INT(l) && IS_INT(r)) RETURN_OK(BOOL_VAL(AS_INT(l) > AS_INT(r)));
                    if (IS_NUM(l) && IS_NUM(r)) RETURN_OK(BOOL_VAL(NUM_AS_DOUBLE(l) > NUM_AS_DOUBLE(r)));
                    break;
                case TOK_YORI_SHITA:
                    if (IS_INT(l) && IS_INT(r)) RETURN_OK(BOOL_VAL(AS_INT(l) < AS_INT(r)));
                    if (IS_NUM(l) && IS_NUM(r)) RETURN_OK(BOOL_VAL(NUM_AS_DOUBLE(l) < NUM_AS_DOUBLE(r)));
                    break;
                case TOK_IJOU:
                    if (IS_INT(l) && IS_INT(r)) RETURN_OK(BOOL_VAL(AS_INT(l) >= AS_INT(r)));
                    if (IS_NUM(l) && IS_NUM(r)) RETURN_OK(BOOL_VAL(NUM_AS_DOUBLE(l) >= NUM_AS_DOUBLE(r)));
                    break;
                case TOK_IKA:
                    if (IS_INT(l) && IS_INT(r)) RETURN_OK(BOOL_VAL(AS_INT(l) <= AS_INT(r)));
                    if (IS_NUM(l) && IS_NUM(r)) RETURN_OK(BOOL_VAL(NUM_AS_DOUBLE(l) <= NUM_AS_DOUBLE(r)));
                    break;

                
                default: break;
            }

            #undef NUM_AS_DOUBLE
            #undef IS_NUM
            break;
        }
        case AST_UNARY: {
             EvalResult operand = evaluate(node->as.unary.operand, env);
             if (operand.type != RES_OK) return operand;
             if (node->as.unary.op == TOK_MAINASU) {
                 if (IS_INT(operand.value)) RETURN_OK(INT_VAL(-AS_INT(operand.value)));
                 if (IS_FLOAT(operand.value)) RETURN_OK(FLOAT_VAL(-AS_FLOAT(operand.value)));
                 error_report(ERR_TYPE, node->line, "数値じゃないとマイナスできないヨ😅💦");
                 RETURN_ERR();
             }
             if (node->as.unary.op == TOK_CHIGAU_YO) { 
                 RETURN_OK(BOOL_VAL(!IS_TRUTHY(operand.value)));
             }
             break;
        }
        case AST_EXPR_STMT: return evaluate(node->as.expr_stmt.expr, env);
        case AST_PRINT: {
            EvalResult res = evaluate(node->as.print_stmt.value, env);
            if (res.type != RES_OK) return res;
            value_print(res.value);
            if (node->as.print_stmt.is_println) printf("\n");
            RETURN_OK(NULL_VAL);
        }
        case AST_BLOCK: return exec_block(node, env);
        case AST_IF: {
            EvalResult cond = evaluate(node->as.if_stmt.condition, env);
            if (cond.type != RES_OK) return cond;
            
            if (IS_TRUTHY(cond.value)) {
                return exec_block(node->as.if_stmt.then_branch, env);
            } else if (node->as.if_stmt.else_branch) {
                
                if (node->as.if_stmt.else_branch->type == AST_IF) {
                     return evaluate(node->as.if_stmt.else_branch, env);
                } else {
                     return exec_block(node->as.if_stmt.else_branch, env);
                }
            }
            RETURN_OK(NULL_VAL);
        }
        case AST_WHILE: {
            while (true) {
                EvalResult cond = evaluate(node->as.while_stmt.condition, env);
                if (cond.type != RES_OK) return cond;
                if (!IS_TRUTHY(cond.value)) break;
                
                EvalResult res = exec_block(node->as.while_stmt.body, env); 
                if (res.type == RES_RETURN || res.type == RES_ERROR) return res;
                if (res.type == RES_BREAK) break;
                
            }
            RETURN_OK(NULL_VAL);
        }
        case AST_FOR_RANGE: {
            
            EvalResult start = evaluate(node->as.for_range.start, env);
            EvalResult end = evaluate(node->as.for_range.end, env);
            if (start.type != RES_OK || end.type != RES_OK) RETURN_ERR();
            if (!IS_INT(start.value) || !IS_INT(end.value)) {
                 error_report(ERR_TYPE, node->line, "ループ範囲は整数じゃないとダメだヨ😅💦");
                 RETURN_ERR();
            }
            
            Environment* loopEnv = env_new(env);
            env_define(loopEnv, node->as.for_range.var_name, start.value);
            
            long long current = AS_INT(start.value);
            long long limit = AS_INT(end.value);
            int step = (current <= limit) ? 1 : -1;
            
            while ((step > 0 && current <= limit) || (step < 0 && current >= limit)) {
                env_assign(loopEnv, node->as.for_range.var_name, INT_VAL(current));
                
                EvalResult res = exec_block(node->as.for_range.body, loopEnv);
                if (res.type == RES_RETURN || res.type == RES_ERROR) { env_release(loopEnv); return res; }
                if (res.type == RES_BREAK) break;
                
                current += step;
            }
            env_release(loopEnv);
            RETURN_OK(NULL_VAL);
        }
        case AST_VAR_DECL: {
             EvalResult val = evaluate(node->as.var_decl.init, env);
             if (val.type != RES_OK) return val;
             env_define(env, node->as.var_decl.name, val.value);
             RETURN_OK(val.value);
        }
        case AST_VARIABLE: {
             Value val;
             if (env_get(env, node->as.variable.name, &val)) {
                 RETURN_OK(val);
             }
             error_report(ERR_UNDEFINED, node->line, "変数「%s」が見つからないヨ😅💦", node->as.variable.name);
             RETURN_ERR();
        }
        case AST_NEW: {
             
             Value klassVal;
             if (!env_get(env, node->as.new_expr.class_name, &klassVal)) {
                 error_report(ERR_UNDEFINED, node->line, "クラス「%s」が見つからないヨ😅💦", node->as.new_expr.class_name);
                 RETURN_ERR();
             }
             if (!IS_OBJ(klassVal) || AS_OBJ(klassVal)->type != OBJ_CLASS) {
                 error_report(ERR_TYPE, node->line, "「%s」はクラスじゃないヨ😅💦", node->as.new_expr.class_name);
                 RETURN_ERR();
             }
             ObjClass* klass = (ObjClass*)AS_OBJ(klassVal);
             
             
             ObjInstance* instance = new_instance(klass);
             
             
             if (klass->constructor) {
                 
                 Value* args = malloc(sizeof(Value) * node->as.new_expr.arg_count);
                 for(int i=0; i<node->as.new_expr.arg_count; i++) {
                     EvalResult r = evaluate(node->as.new_expr.args[i], env);
                     if (r.type != RES_OK) { free(args); return r; }
                     args[i] = r.value;
                 }
                 
                 
                 Environment* ctorEnv = env_new(klass->constructor->closure);
                 env_define(ctorEnv, "this", OBJ_VAL(instance));
                 
                 
                 for (int i = 0; i < klass->constructor->param_count; i++) {
                    if (i < node->as.new_expr.arg_count) {
                        env_define(ctorEnv, klass->constructor->params[i], args[i]);
                    } else {
                        env_define(ctorEnv, klass->constructor->params[i], NULL_VAL);
                    }
                 }
                 free(args);
                 
                 EvalResult res = exec_block(klass->constructor->body, ctorEnv);
                 env_release(ctorEnv);
                 if (res.type == RES_ERROR) return res;
             }
             RETURN_OK(OBJ_VAL(instance));
        }
        case AST_GET: {
             EvalResult objRes = evaluate(node->as.get.object, env);
             if (objRes.type != RES_OK) return objRes;
             Value objVal = objRes.value;
             
             if (!IS_OBJ(objVal)) { error_report(ERR_TYPE, node->line, "オブジェクトじゃないヨ😅💦"); RETURN_ERR(); }
             
             if (AS_OBJ(objVal)->type == OBJ_INSTANCE) {
                 ObjInstance* inst = (ObjInstance*)AS_OBJ(objVal);
                 void* valPtr;
                 if (table_get(inst->fields, node->as.get.name, &valPtr)) {
                     RETURN_OK(*(Value*)valPtr);
                 }
                 
                 void* methodPtr;
                 if (table_get(inst->klass->methods, node->as.get.name, &methodPtr)) {
                     
                     
                     RETURN_OK(*(Value*)methodPtr);
                 }
                 error_report(ERR_UNDEFINED, node->line, "「%s」なんてメンバ持ってないヨ😅💦", node->as.get.name);
                 RETURN_ERR();
             } else if (AS_OBJ(objVal)->type == OBJ_STRING) {
                 
                 if (strcmp(node->as.get.name, "length") == 0) {
                     RETURN_OK(INT_VAL(((ObjString*)AS_OBJ(objVal))->length));
                 }
             }
             RETURN_ERR();
        }
        case AST_SET: {
             EvalResult objRes = evaluate(node->as.set.object, env);
             if (objRes.type != RES_OK) return objRes;
             Value objVal = objRes.value;
             
             if (!IS_OBJ(objVal) || AS_OBJ(objVal)->type != OBJ_INSTANCE) {
                 error_report(ERR_TYPE, node->line, "インスタンスじゃないと代入できないヨ😅💦");
                 RETURN_ERR();
             }
             ObjInstance* inst = (ObjInstance*)AS_OBJ(objVal);
             
             EvalResult valRes = evaluate(node->as.set.value, env);
             if (valRes.type != RES_OK) return valRes;
             
             
             Value* vPtr = malloc(sizeof(Value));
             *vPtr = valRes.value;
             table_set(inst->fields, node->as.set.name, vPtr);
             RETURN_OK(valRes.value);
        }
        case AST_THIS: {
             Value val;
             if (env_get(env, "this", &val)) RETURN_OK(val);
             error_report(ERR_RUNTIME, node->line, "ここはクラスの中じゃないヨ😅💦");
             RETURN_ERR();
        }
        case AST_ARRAY_LITERAL: {
             ObjList* list = new_list();
             if (node->as.array_literal.count > 0) {
                 list->items = malloc(sizeof(Value) * node->as.array_literal.count);
                 list->capacity = node->as.array_literal.count;
                 list->count = node->as.array_literal.count;
                 for(int i=0; i<list->count; i++) {
                     EvalResult r = evaluate(node->as.array_literal.elements[i], env);
                     if (r.type != RES_OK) { free(list->items); return r; } 
                     list->items[i] = r.value;
                 }
                 
             }
             RETURN_OK(OBJ_VAL((Obj*)list));
        }
        case AST_ASSIGNMENT: {
             EvalResult val = evaluate(node->as.assignment.value, env);
             if (val.type != RES_OK) return val;
             if (env_assign(env, node->as.assignment.name, val.value)) {
                 RETURN_OK(val.value);
             }
             error_report(ERR_UNDEFINED, node->line, "変数「%s」が見つからないヨ😅💦", node->as.assignment.name);
             RETURN_ERR();
        }
        case AST_FUNC_DECL: {
             ObjFunc* func = new_function(node->as.func_decl.name, node->as.func_decl.param_count, node->as.func_decl.params, node->as.func_decl.body);
             func->closure = env;
             env_retain(env);
             env_define(env, node->as.func_decl.name, OBJ_VAL(func));
             RETURN_OK(OBJ_VAL(func));
        }
        case AST_CLASS_DECL: {
             ObjClass* klass = new_class(node->as.class_decl.name);
             for (int i = 0; i < node->as.class_decl.method_count; i++) {
                 AstNode* methodNode = node->as.class_decl.methods[i];
                 ObjFunc* method = new_function(methodNode->as.func_decl.name, 
                                                methodNode->as.func_decl.param_count,
                                                methodNode->as.func_decl.params,
                                                methodNode->as.func_decl.body);
                 method->closure = env;
                 env_retain(env);
                 Value* vPtr = malloc(sizeof(Value));
                 *vPtr = OBJ_VAL(method);
                 table_set(klass->methods, method->name, vPtr);
             }
             if (node->as.class_decl.constructor) {
                 AstNode* ctorNode = node->as.class_decl.constructor;
                 klass->constructor = new_function(ctorNode->as.func_decl.name,
                                                   ctorNode->as.func_decl.param_count,
                                                   ctorNode->as.func_decl.params,
                                                   ctorNode->as.func_decl.body);
                 klass->constructor->closure = env;
                 env_retain(env);
             }
             env_define(env, klass->name, OBJ_VAL(klass));
             RETURN_OK(NULL_VAL);
        }
        case AST_RETURN: {
             EvalResult val = evaluate(node->as.return_stmt.value, env);
             if (val.type != RES_OK) return val;
             return (EvalResult){RES_RETURN, val.value};
        }
        case AST_CALL: {
             
             Value thisVal = NULL_VAL;
             bool is_method_call = false;

             if (node->as.call.callee && node->as.call.callee->type == AST_GET) {
                 
                 EvalResult objRes = evaluate(node->as.call.callee->as.get.object, env);
                 if (objRes.type != RES_OK) return objRes;
                 thisVal = objRes.value;

                 if (IS_OBJ(thisVal) && AS_OBJ(thisVal)->type == OBJ_INSTANCE) {
                     is_method_call = true;
                 }
             }

             EvalResult callee = evaluate(node->as.call.callee, env);
             if (callee.type != RES_OK) return callee;

             
             Value* args = NULL;
             if (node->as.call.arg_count > 0) {
                 args = malloc(sizeof(Value) * node->as.call.arg_count);
                 for (int i = 0; i < node->as.call.arg_count; i++) {
                     EvalResult a = evaluate(node->as.call.args[i], env);
                     if (a.type != RES_OK) { free(args); return a; }
                     args[i] = a.value;
                 }
             }

             if (!IS_OBJ(callee.value)) { error_report(ERR_TYPE, node->line, "それは関数じゃないヨ😅💦"); if(args) free(args); RETURN_ERR(); }

             EvalResult ret;
             if (AS_OBJ(callee.value)->type == OBJ_FUNC) {
                 ObjFunc* func = (ObjFunc*)AS_OBJ(callee.value);
                 if (is_method_call) {
                     
                     call_depth++;
                     if (call_depth > MAX_CALL_DEPTH) {
                         call_depth--;
                         error_report(ERR_RUNTIME, node->line, "再帰が深すぎるヨ😱💦 スタックオーバーフロー防止で止めたヨ");
                         if(args) free(args);
                         RETURN_ERR();
                     }
                     
                     Environment* fnEnv = env_new(func->closure);
                     env_define(fnEnv, "this", thisVal);
                     for (int i = 0; i < func->param_count; i++) {
                         Value val = NULL_VAL;
                         if (i < node->as.call.arg_count) val = args[i];
                         env_define(fnEnv, func->params[i], val);
                     }
                     EvalResult res = exec_block(func->body, fnEnv);
                     env_release(fnEnv);
                     call_depth--;
                     if (res.type == RES_RETURN) ret = (EvalResult){RES_OK, res.value};
                     else if (res.type == RES_ERROR) ret = res;
                     else ret = (EvalResult){RES_OK, NULL_VAL};
                 } else {
                     ret = call_function(func, node->as.call.arg_count, args);
                 }
             } else if (AS_OBJ(callee.value)->type == OBJ_NATIVE) {
                 ret = call_native((ObjNative*)AS_OBJ(callee.value), node->as.call.arg_count, args);
             } else {
                 error_report(ERR_TYPE, node->line, "それは関数じゃないヨ😅💦");
                 ret = (EvalResult){RES_ERROR, NULL_VAL};
             }

             if (args) free(args);
             return ret;
        }
        case AST_BREAK: return (EvalResult){RES_BREAK, NULL_VAL};
        case AST_CONTINUE: return (EvalResult){RES_CONTINUE, NULL_VAL};

        case AST_FOR_EACH: {
            EvalResult collRes = evaluate(node->as.for_each.collection, env);
            if (collRes.type != RES_OK) return collRes;
            if (!IS_OBJ(collRes.value)) {
                error_report(ERR_TYPE, node->line, "配列か辞書じゃないとfor-eachできないヨ😅💦");
                RETURN_ERR();
            }

            if (AS_OBJ(collRes.value)->type == OBJ_LIST) {
                
                ObjList* list = (ObjList*)AS_OBJ(collRes.value);
                Environment* loopEnv = env_new(env);
                env_define(loopEnv, node->as.for_each.var_name, NULL_VAL);
                for (int i = 0; i < list->count; i++) {
                    env_assign(loopEnv, node->as.for_each.var_name, list->items[i]);
                    EvalResult res = exec_block(node->as.for_each.body, loopEnv);
                    if (res.type == RES_RETURN || res.type == RES_ERROR) { env_release(loopEnv); return res; }
                    if (res.type == RES_BREAK) break;
                }
                env_release(loopEnv);
            } else if (AS_OBJ(collRes.value)->type == OBJ_DICT) {
                
                ObjDict* dict = (ObjDict*)AS_OBJ(collRes.value);
                
                ObjList* keys = new_list();
                ForEachDictCtx feCtx = { .list = keys };
                table_iterate(dict->items, for_each_dict_callback, &feCtx);

                Environment* loopEnv = env_new(env);
                env_define(loopEnv, node->as.for_each.var_name, NULL_VAL);
                for (int i = 0; i < keys->count; i++) {
                    env_assign(loopEnv, node->as.for_each.var_name, keys->items[i]);
                    EvalResult res = exec_block(node->as.for_each.body, loopEnv);
                    if (res.type == RES_RETURN || res.type == RES_ERROR) { env_release(loopEnv); return res; }
                    if (res.type == RES_BREAK) break;
                }
                env_release(loopEnv);
            } else {
                error_report(ERR_TYPE, node->line, "配列か辞書じゃないとfor-eachできないヨ😅💦");
                RETURN_ERR();
            }
            RETURN_OK(NULL_VAL);
        }

        case AST_INDEX_GET: {
            EvalResult objRes = evaluate(node->as.index_get.object, env);
            if (objRes.type != RES_OK) return objRes;
            EvalResult idxRes = evaluate(node->as.index_get.index, env);
            if (idxRes.type != RES_OK) return idxRes;

            if (IS_OBJ(objRes.value) && AS_OBJ(objRes.value)->type == OBJ_LIST) {
                ObjList* list = (ObjList*)AS_OBJ(objRes.value);
                if (!IS_INT(idxRes.value)) {
                    error_report(ERR_TYPE, node->line, "配列のインデックスは整数じゃないとダメだヨ😅💦");
                    RETURN_ERR();
                }
                long long idx = AS_INT(idxRes.value);
                if (idx < 0 || idx >= list->count) {
                    error_report(ERR_INDEX_OUT_OF_BOUNDS, node->line, "インデックス %lld は範囲外だヨ😅💦", idx);
                    RETURN_ERR();
                }
                RETURN_OK(list->items[idx]);
            }
            if (IS_OBJ(objRes.value) && AS_OBJ(objRes.value)->type == OBJ_DICT) {
                ObjDict* dict = (ObjDict*)AS_OBJ(objRes.value);
                if (!IS_OBJ(idxRes.value) || AS_OBJ(idxRes.value)->type != OBJ_STRING) {
                    error_report(ERR_TYPE, node->line, "辞書のキーは文字列じゃないとダメだヨ😅💦");
                    RETURN_ERR();
                }
                char* key = ((ObjString*)AS_OBJ(idxRes.value))->chars;
                void* valPtr;
                if (table_get(dict->items, key, &valPtr)) {
                    RETURN_OK(*(Value*)valPtr);
                }
                RETURN_OK(NULL_VAL);
            }
            error_report(ERR_TYPE, node->line, "インデックスアクセスできないヨ😅💦");
            RETURN_ERR();
        }

        case AST_INDEX_SET: {
            EvalResult objRes = evaluate(node->as.index_set.object, env);
            if (objRes.type != RES_OK) return objRes;
            EvalResult idxRes = evaluate(node->as.index_set.index, env);
            if (idxRes.type != RES_OK) return idxRes;
            EvalResult valRes = evaluate(node->as.index_set.value, env);
            if (valRes.type != RES_OK) return valRes;

            if (IS_OBJ(objRes.value) && AS_OBJ(objRes.value)->type == OBJ_LIST) {
                ObjList* list = (ObjList*)AS_OBJ(objRes.value);
                if (!IS_INT(idxRes.value)) {
                    error_report(ERR_TYPE, node->line, "配列のインデックスは整数じゃないとダメだヨ😅💦");
                    RETURN_ERR();
                }
                long long idx = AS_INT(idxRes.value);
                if (idx < 0 || idx >= list->count) {
                    error_report(ERR_INDEX_OUT_OF_BOUNDS, node->line, "インデックス %lld は範囲外だヨ😅💦", idx);
                    RETURN_ERR();
                }
                list->items[idx] = valRes.value;
                RETURN_OK(valRes.value);
            }
            if (IS_OBJ(objRes.value) && AS_OBJ(objRes.value)->type == OBJ_DICT) {
                ObjDict* dict = (ObjDict*)AS_OBJ(objRes.value);
                if (!IS_OBJ(idxRes.value) || AS_OBJ(idxRes.value)->type != OBJ_STRING) {
                    error_report(ERR_TYPE, node->line, "辞書のキーは文字列じゃないとダメだヨ😅💦");
                    RETURN_ERR();
                }
                char* key = ((ObjString*)AS_OBJ(idxRes.value))->chars;
                Value* vPtr = malloc(sizeof(Value));
                *vPtr = valRes.value;
                table_set(dict->items, key, vPtr);
                RETURN_OK(valRes.value);
            }
            error_report(ERR_TYPE, node->line, "インデックス代入できないヨ😅💦");
            RETURN_ERR();
        }

        case AST_ARRAY_PUSH: {
            Value arrVal;
            if (!env_get(env, node->as.array_push.array_name, &arrVal)) {
                error_report(ERR_UNDEFINED, node->line, "配列「%s」が見つからないヨ😅💦", node->as.array_push.array_name);
                RETURN_ERR();
            }
            if (!IS_OBJ(arrVal) || AS_OBJ(arrVal)->type != OBJ_LIST) {
                error_report(ERR_TYPE, node->line, "「%s」は配列じゃないヨ😅💦", node->as.array_push.array_name);
                RETURN_ERR();
            }
            ObjList* list = (ObjList*)AS_OBJ(arrVal);
            EvalResult valRes = evaluate(node->as.array_push.value, env);
            if (valRes.type != RES_OK) return valRes;
            if (list->count + 1 > list->capacity) {
                list->capacity = list->capacity < 8 ? 8 : list->capacity * 2;
                list->items = realloc(list->items, sizeof(Value) * list->capacity);
            }
            list->items[list->count++] = valRes.value;
            RETURN_OK(NULL_VAL);
        }

        case AST_DICT_LITERAL: {
            ObjDict* dict = new_dict();
            for (int i = 0; i < node->as.dict_literal.count; i++) {
                EvalResult keyRes = evaluate(node->as.dict_literal.keys[i], env);
                if (keyRes.type != RES_OK) return keyRes;
                EvalResult valRes = evaluate(node->as.dict_literal.values[i], env);
                if (valRes.type != RES_OK) return valRes;
                if (!IS_OBJ(keyRes.value) || AS_OBJ(keyRes.value)->type != OBJ_STRING) {
                    error_report(ERR_TYPE, node->line, "辞書のキーは文字列じゃないとダメだヨ😅💦");
                    RETURN_ERR();
                }
                char* key = ((ObjString*)AS_OBJ(keyRes.value))->chars;
                Value* vPtr = malloc(sizeof(Value));
                *vPtr = valRes.value;
                table_set(dict->items, key, vPtr);
            }
            RETURN_OK(OBJ_VAL(dict));
        }

        case AST_TRY: {
            TryContext tryCtx;
            tryCtx.prev = current_try_ctx;
            tryCtx.error_message[0] = '\0';
            current_try_ctx = &tryCtx;

            EvalResult result;
            if (setjmp(tryCtx.buf) == 0) {
                
                result = exec_block(node->as.try_stmt.try_block, env);
                current_try_ctx = tryCtx.prev;
            } else {
                
                current_try_ctx = tryCtx.prev;
                if (node->as.try_stmt.catch_block) {
                    Environment* catchEnv = env_new(env);
                    if (node->as.try_stmt.catch_var) {
                        ObjString* errStr = copy_string_value(tryCtx.error_message, strlen(tryCtx.error_message));
                        env_define(catchEnv, node->as.try_stmt.catch_var, OBJ_VAL(errStr));
                    }
                    result = exec_block(node->as.try_stmt.catch_block, catchEnv);
                    env_release(catchEnv);
                } else {
                    result = (EvalResult){RES_OK, NULL_VAL};
                }
            }
            
            if (node->as.try_stmt.finally_block) {
                EvalResult finResult = exec_block(node->as.try_stmt.finally_block, env);
                if (finResult.type == RES_ERROR) return finResult;
            }
            return result;
        }

        case AST_IMPORT: {
            const char* path = node->as.import_stmt.path;
            
            const char* dot = strrchr(path, '.');
            if (!dot || (strcmp(dot, ".ojs") != 0 && strcmp(dot, ".oji") != 0)) {
                error_report(ERR_RUNTIME, node->line,
                    "インポートは「.ojs」か「.oji」ファイルだけダヨ😅💦: \"%s\"", path);
                RETURN_ERR();
            }
            
            for (int i = 0; i < import_count; i++) {
                if (strcmp(imported_paths[i], path) == 0) {
                    RETURN_OK(NULL_VAL); 
                }
            }
            if (import_count >= MAX_IMPORTS) {
                error_report(ERR_RUNTIME, node->line, "インポートが多すぎるヨ😱💦");
                RETURN_ERR();
            }
            imported_paths[import_count++] = strdup(path);
            
            FILE* f = fopen(path, "rb");
            if (!f) {
                error_report(ERR_RUNTIME, node->line,
                    "ファイルが開けないヨ😅💦: \"%s\"", path);
                RETURN_ERR();
            }
            fseek(f, 0L, SEEK_END);
            size_t fsize = ftell(f);
            rewind(f);
            char* src = malloc(fsize + 1);
            size_t rd = fread(src, 1, fsize, f);
            src[rd] = '\0';
            fclose(f);
            
            AstNode* prog = parse_program(src);
            if (prog) {
                EvalResult res = (EvalResult){RES_OK, NULL_VAL};
                for (int i = 0; i < prog->as.block.stmt_count; i++) {
                    res = evaluate(prog->as.block.stmts[i], env);
                    if (res.type == RES_ERROR) break;
                }
                ast_free(prog);
                free(src);
                if (res.type == RES_ERROR) return res;
            } else {
                free(src);
            }
            RETURN_OK(NULL_VAL);
        }

        case AST_INPUT:
        case AST_CONVERT:
        case AST_TYPEOF:
        case AST_RANDOM:
            
            error_report(ERR_RUNTIME, node->line, "内部エラーだヨ😅💦");
            RETURN_ERR();

        default:
            error_report(ERR_RUNTIME, node->line, "未対応のASTノードだヨ😅💦");
            RETURN_ERR();
    }
    error_report(ERR_RUNTIME, node->line, "式の評価に失敗したヨ😅💦");
    RETURN_ERR();
}


static EvalResult exec_block(AstNode* node, Environment* env) {
    if (node->type != AST_BLOCK) return evaluate(node, env);

    Environment* blockEnv = env_new(env);
    for (int i = 0; i < node->as.block.stmt_count; i++) {
        EvalResult res = evaluate(node->as.block.stmts[i], blockEnv);
        if (res.type != RES_OK) {
            env_release(blockEnv); 
            return res;
        }
    }
    env_release(blockEnv);
    RETURN_OK(NULL_VAL);
}

static EvalResult call_function(ObjFunc* func, int argCount, Value* args) {
     
     call_depth++;
     if (call_depth > MAX_CALL_DEPTH) {
         call_depth--;
         error_report(ERR_RUNTIME, 0, "再帰が深すぎるヨ😱💦 スタックオーバーフロー防止で止めたヨ");
         RETURN_ERR();
     }

     Environment* fnEnv = env_new(func->closure);
     for (int i = 0; i < func->param_count; i++) {
         Value val = NULL_VAL;
         if (i < argCount) val = args[i];
         env_define(fnEnv, func->params[i], val);
     }

     
     EvalResult res = exec_block(func->body, fnEnv);
     env_release(fnEnv);
     call_depth--;

     if (res.type == RES_RETURN) return (EvalResult){RES_OK, res.value}; 
     if (res.type == RES_ERROR) return res;
     RETURN_OK(NULL_VAL);
}

static EvalResult call_native(ObjNative* native, int argCount, Value* args) {
    Value res = native->function(argCount, args);
    RETURN_OK(res);
}

void interpret(const char* source) {
    gc_init();
    
    for (int i = 0; i < import_count; i++) free(imported_paths[i]);
    import_count = 0;
    AstNode* program = parse_program(source);
    
    
    if (!program) return;

    Environment* global = env_new(NULL);
    register_builtins(global);
    gc_set_root(global); 

    call_depth = 0; 
    evaluate(program, global);

    gc_set_root(NULL);
    env_release(global);
    ast_free(program);
    gc_collect(NULL); 
}
