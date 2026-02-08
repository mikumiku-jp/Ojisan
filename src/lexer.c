#include "lexer.h"
#include "utf8.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct {
    const char* start;
    const char* current;
    int line;
} Lexer;

Lexer lexer;


typedef struct {
    const char* pattern;
    TokenType type;
} KeywordEntry;


static const KeywordEntry keywords[] = {
    {"まで関係あるんだけどサ😁",   TOK_MADE_KANKEI},
    {"のメンバーなんだけどサ😁",   TOK_NO_MEMBER},
    {"チャンのやり方教えるネ😘",   TOK_YARIKATA},
    {"サンのコト教えるヨ😃",       TOK_SAN_KOTO},
    {"サンのコトおしまい❗",        TOK_SAN_KOTO_OSHIMAI},
    {"チャンにオネガイ😃",         TOK_ONEGAI},
    {"チョット教えてヨ😃",         TOK_OSHIETE_YO},
    {"チョット聞いてヨ😃",         TOK_CHOTTO_KIITE},
    {"気になるんだけど😚",         TOK_KININARU},
    {"ハジメマシテおしまい❗",      TOK_HAJIME_OSHIMAI},
    {"ニナッチャッタ😅💦",         TOK_NI_NACCHATTA},
    {"ドキドキおしまい❗",          TOK_DOKIDOKI_OSHIMAI},
    {"ドキドキするけど😅💦",       TOK_DOKIDOKI},
    {"やり方おしまい❗",            TOK_YARIKATA_OSHIMAI},
    {"ソウジャナカッタラ😅",       TOK_SOUJANAKATTARA},
    {"ドッチニシテモ😤",           TOK_DOCCHI_NI_SHITEMO},
    {"ハジメマシテ😘",             TOK_HAJIMEMASHITE},
    {"取り寄せてヨ😃",             TOK_TORIYOSE},
    {"サンを作るヨ😃",             TOK_SAN_WO_TSUKURU},
    {"数字にしてネ😘",             TOK_SUUJI_NI},
    {"文字にしてネ😘",             TOK_MOJI_NI},
    {"型を教えてヨ😃",             TOK_KATA_WO},
    {"長さを教えてヨ😃",           TOK_NAGASA_WO},
    {"ランダムチャン😃",           TOK_RANDOM_CHAN},
    {"もしかして😍",               TOK_MOSHIKASHITE},
    {"ナンチャッテ😃",             TOK_NANCHATTE},
    {"ちがうカナ❓",                TOK_CHIGAU_KANA},
    {"おなじカナ❓",                TOK_ONAJI_KANA},
    {"もういいカナ😤",             TOK_MOU_II},
    {"ヤバかった😱",               TOK_YABAKATTA},
    {"を追加ダヨ😁",               TOK_WO_TSUIKA},
    {"次イコウヨ😃",               TOK_TSUGI_IKOU},
    {"の間はネ😘",                 TOK_AIDA_WA},
    {"もうムリ😱💦",               TOK_MOU_MURI},
    {"ツブヤキ📱",                 TOK_TSUBUYAKI},
    {"オッハー❗",                  TOK_OHHA},
    {"オッケー👍",                 TOK_OKKEE},
    {"の長さチャン",               TOK_NAGASA_CHAN},
    {"番目チャンは",               TOK_BANME_CHAN_WA},
    {"番目チャン",                 TOK_BANME_CHAN},
    {"チガウヨ",                   TOK_CHIGAU_YO},
    {"より上❗",                    TOK_YORI_UE},
    {"より下❗",                    TOK_YORI_SHITA},
    {"コタエは",                    TOK_KOTAE},
    {"ダヨ😁",                     TOK_DA_YO},
    {"ナンダ😘",                   TOK_NANDA},
    {"ナイナイ",                    TOK_NAI_NAI},
    {"もしくは",                    TOK_MOSHIKUWA},
    {"以上❗",                      TOK_IJOU},
    {"以下❗",                      TOK_IKA},
    {"ボクの",                      TOK_BOKU_NO},
    {"しかも",                      TOK_SHIKAMO},
    {"かける",                      TOK_KAKERU},
    {"カナ❓",                      TOK_KANA},
    {"チャンは",                    TOK_CHAN_WA},
    {"チャンが",                    TOK_CHAN_GA},
    {"チャンに",                    TOK_CHAN_NI},
    {"あまり",                      TOK_AMARI},
    {"マイナス",                    TOK_MAINASU},
    {"マジ",                        TOK_MAJI},
    {"チャン",                      TOK_CHAN},
    {"ひく",                        TOK_HIKU},
    {"わる",                        TOK_WARU},
    {"から",                        TOK_KARA},
    {"ウソ",                        TOK_USO},
    {"と",                          TOK_TO},
    {"の",                          TOK_NO},
    {"、",                          TOK_COMMA},
    {"→",                          TOK_ARROW},
    {"【",                          TOK_LBRACKET},
    {"】",                          TOK_RBRACKET},
    {"《",                          TOK_LDICT},
    {"》",                          TOK_RDICT},
    {"(",                           TOK_LPAREN},
    {")",                           TOK_RPAREN},
    {NULL, 0}
};

void lexer_init(const char* source) {
    lexer.start = source;
    lexer.current = source;
    lexer.line = 1;
}

LexerState lexer_save_state(void) {
    LexerState state;
    state.start = lexer.start;
    state.current = lexer.current;
    state.line = lexer.line;
    return state;
}

void lexer_restore_state(LexerState state) {
    lexer.start = state.start;
    lexer.current = state.current;
    lexer.line = state.line;
}

static bool is_at_end() {
    return *lexer.current == '\0';
}

static Token make_token(TokenType type) {
    Token token;
    token.type = type;
    token.start = lexer.start;
    token.length = (int)(lexer.current - lexer.start);
    token.line = lexer.line;
    return token;
}

static Token error_token(const char* message) {
    Token token;
    token.type = TOK_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = lexer.line;
    return token;
}

static bool match_keyword(TokenType* out_type) {
    for (int i = 0; keywords[i].pattern != NULL; i++) {
        int len = strlen(keywords[i].pattern);
        if (memcmp(lexer.start, keywords[i].pattern, len) == 0) {
            
            
            lexer.current = lexer.start + len;
            *out_type = keywords[i].type;
            return true;
        }
    }
    return false;
}

Token lexer_scan_token() {
    
    for (;;) {
        Codepoint cp;
        const char* p = lexer.current;
        int w = utf8_decode(p, &cp);
        if (w == 0) break;

        if (cp == ' ' || cp == '\r' || cp == '\t' || cp == 0x3000 ) {
            lexer.current += w;
        } else if (cp == '\n') {
            lexer.line++;
            lexer.current += w;
        } else {
            
            
            if (strncmp(lexer.current, "（ココだけの話…", strlen("（ココだけの話…")) == 0) {
                
                lexer.current += strlen("（ココだけの話…");
                while (!is_at_end()) {
                    w = utf8_decode(lexer.current, &cp);
                    if (cp == '\n') lexer.line++;
                    if (memcmp(lexer.current, "）", 3) == 0) { 
                         
                         
                        lexer.current += strlen("）");
                        break;
                    }
                    lexer.current += w;
                }
            } else {
                break;
            }
        }
    }

    lexer.start = lexer.current;

    if (is_at_end()) return make_token(TOK_EOF);

    
    TokenType type;
    if (match_keyword(&type)) {
        return make_token(type);
    }

    
    {
        Codepoint cp;
        int w = utf8_decode(lexer.current, &cp);
        if (utf8_is_digit(cp)) {
            bool has_dot = false;
            while (!is_at_end()) {
                w = utf8_decode(lexer.current, &cp);
                if (utf8_is_digit(cp)) {
                    lexer.current += w;
                } else if (cp == '.' && !has_dot) {
                     has_dot = true;
                     lexer.current++; 
                } else {
                    break;
                }
            }
            return make_token(TOK_NUMBER);
        }
    }

    
    if (memcmp(lexer.current, "「", 3) == 0) {
         lexer.current += 3;
         while (!is_at_end()) {
             if (*lexer.current == '\\') {
                 
                 lexer.current++;
                 if (!is_at_end()) lexer.current++;
             } else if (memcmp(lexer.current, "」", 3) == 0) {
                 lexer.current += 3;
                 return make_token(TOK_STRING);
             } else {
                 if (*lexer.current == '\n') lexer.line++;
                 lexer.current++;
             }
         }
         return error_token("文字列が閉じてないヨ😅💦");
    }

    
    int len = 0;

    
    Codepoint cp;
    int w = utf8_decode(lexer.current, &cp);
    if (utf8_is_alnum(cp) || cp > 0x7F) { 
        while (!is_at_end()) {
             
             
             TokenType dummy;
             const char* saved_start = lexer.start;
             lexer.start = lexer.current; 
             bool is_keyword = match_keyword(&dummy);
             lexer.start = saved_start;   
             lexer.current = lexer.start + len; 
             
             if (is_keyword && dummy != TOK_NO && dummy != TOK_TO) {
                 
                 
                 break;
             }
             
             w = utf8_decode(lexer.current, &cp);
             if (utf8_is_space(cp)) break; 
             
             lexer.current += w;
             len += w;
        }
        
        if (len > 0) {
            return make_token(TOK_IDENTIFIER);
        }
    }

    
    lexer.current++;
    return error_token("ナニコレ？読めないヨ😅💦");
}
