#include "error.h"
#include "eval.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* get_prefix_message(ErrorType type) {
    switch (type) {
        case ERR_SYNTAX:
            return "ア、アレ😅💦 構文がおかしいヨ😱 ナンチャッテ…ゴメンネ😅";
        case ERR_RUNTIME:
            return "ヤバかった😱 実行中にエラーが出ちゃったヨ💦";
        case ERR_TYPE:
            return "チョット😅 型が合ってないヨ💦 ダイジョウブカナ😱";
        case ERR_UNDEFINED:
            return "エッ😅💦 ソレはボク知らないナ〜😱";
        case ERR_ZERO_DIV:
            return "ゼロで割っちゃダメだヨ❗😅💦 ナンチャッテ…マジだけど😱";
        case ERR_INDEX_OUT_OF_BOUNDS:
            return "ソノ番号は、範囲外だヨ😅💦";
        case ERR_ARGUMENT_COUNT:
            return "引数の数が合ってないヨ😅💦";
        default:
            return "ナンカおかしいヨ😓";
    }
}

void error_report(ErrorType type, int line, const char* fmt, ...) {
    
    char msg_buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    
    if (type != ERR_SYNTAX && current_try_ctx != NULL) {
        snprintf(current_try_ctx->error_message, sizeof(current_try_ctx->error_message),
                 "%s", msg_buf);
        longjmp(current_try_ctx->buf, 1);
    }

    
    fprintf(stderr, "%s\n", get_prefix_message(type));
    if (line > 0) {
        fprintf(stderr, "（%d行目ダヨ❗） ", line);
    }
    fprintf(stderr, "%s\n", msg_buf);
}

void error_print_raw(const char* msg) {
    fprintf(stderr, "%s\n", msg);
}
