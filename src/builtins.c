#include "builtins.h"
#include "hashtable.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <wchar.h>

#ifdef _WIN32
#include <io.h>

typedef void* HANDLE;
typedef unsigned long DWORD;
typedef int BOOL;
#define STD_INPUT_HANDLE ((DWORD)-10)
#define INVALID_HANDLE_VALUE ((HANDLE)(long long)-1)
#define CP_UTF8 65001
__declspec(dllimport) HANDLE __stdcall GetStdHandle(DWORD);
__declspec(dllimport) BOOL __stdcall ReadConsoleW(HANDLE, void*, DWORD, DWORD*, void*);
__declspec(dllimport) int __stdcall WideCharToMultiByte(unsigned int, DWORD, const wchar_t*, int, char*, int, const char*, int*);
__declspec(dllimport) int __stdcall MultiByteToWideChar(unsigned int, DWORD, const char*, int, wchar_t*, int);


typedef void* HINTERNET;
#define WINHTTP_ACCESS_TYPE_DEFAULT_PROXY 0
#define WINHTTP_NO_PROXY_NAME NULL
#define WINHTTP_NO_PROXY_BYPASS NULL
#define WINHTTP_NO_REFERER NULL
#define WINHTTP_DEFAULT_ACCEPT_TYPES NULL
#define WINHTTP_NO_ADDITIONAL_HEADERS NULL
#define WINHTTP_NO_REQUEST_DATA NULL
#define WINHTTP_FLAG_SECURE 0x00800000
#define WINHTTP_QUERY_STATUS_CODE 19
#define WINHTTP_QUERY_RAW_HEADERS_CRLF 21
#define WINHTTP_QUERY_FLAG_NUMBER 0x20000000
#define INTERNET_DEFAULT_HTTP_PORT 80
#define INTERNET_DEFAULT_HTTPS_PORT 443
#define ICU_DECODE 0x10000000
#define WINHTTP_FLAG_ESCAPE_DISABLE 0x00000040

typedef struct {
    DWORD   dwStructSize;
    wchar_t* lpszScheme;
    DWORD   dwSchemeLength;
    int     nScheme;
    wchar_t* lpszHostName;
    DWORD   dwHostNameLength;
    int     nPort;
    wchar_t* lpszUserName;
    DWORD   dwUserNameLength;
    wchar_t* lpszPassword;
    DWORD   dwPasswordLength;
    wchar_t* lpszUrlPath;
    DWORD   dwUrlPathLength;
    wchar_t* lpszExtraInfo;
    DWORD   dwExtraInfoLength;
} URL_COMPONENTS_W;

__declspec(dllimport) HINTERNET __stdcall WinHttpOpen(const wchar_t*, DWORD, const wchar_t*, const wchar_t*, DWORD);
__declspec(dllimport) HINTERNET __stdcall WinHttpConnect(HINTERNET, const wchar_t*, int, DWORD);
__declspec(dllimport) HINTERNET __stdcall WinHttpOpenRequest(HINTERNET, const wchar_t*, const wchar_t*, const wchar_t*, const wchar_t*, const wchar_t**, DWORD);
__declspec(dllimport) BOOL __stdcall WinHttpSendRequest(HINTERNET, const wchar_t*, DWORD, void*, DWORD, DWORD, DWORD);
__declspec(dllimport) BOOL __stdcall WinHttpReceiveResponse(HINTERNET, void*);
__declspec(dllimport) BOOL __stdcall WinHttpQueryDataAvailable(HINTERNET, DWORD*);
__declspec(dllimport) BOOL __stdcall WinHttpReadData(HINTERNET, void*, DWORD, DWORD*);
__declspec(dllimport) BOOL __stdcall WinHttpCloseHandle(HINTERNET);
__declspec(dllimport) BOOL __stdcall WinHttpQueryHeaders(HINTERNET, DWORD, const wchar_t*, void*, DWORD*, DWORD*);
__declspec(dllimport) BOOL __stdcall WinHttpCrackUrl(const wchar_t*, DWORD, DWORD, URL_COMPONENTS_W*);
__declspec(dllimport) BOOL __stdcall WinHttpAddRequestHeaders(HINTERNET, const wchar_t*, DWORD, DWORD);
#define WINHTTP_ADDREQ_FLAG_ADD 0x20000000
#define WINHTTP_ADDREQ_FLAG_REPLACE 0x80000000


static wchar_t* utf8_to_wide(const char* utf8) {
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (len <= 0) return NULL;
    wchar_t* wide = malloc(sizeof(wchar_t) * len);
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, len);
    return wide;
}


static char* wide_to_utf8(const wchar_t* wide, int wide_len) {
    int len = WideCharToMultiByte(CP_UTF8, 0, wide, wide_len, NULL, 0, NULL, NULL);
    if (len <= 0) return NULL;
    char* utf8 = malloc(len + 1);
    WideCharToMultiByte(CP_UTF8, 0, wide, wide_len, utf8, len, NULL, NULL);
    utf8[len] = '\0';
    return utf8;
}


static char* winhttp_request(const char* method, const char* url,
                              const char* body, int body_len,
                              const char* extra_headers,
                              int* out_status, char** out_headers) {
    wchar_t* wurl = utf8_to_wide(url);
    if (!wurl) return NULL;

    
    URL_COMPONENTS_W uc;
    memset(&uc, 0, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = {0};
    wchar_t path[2048] = {0};
    wchar_t scheme[16] = {0};
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 2048;
    uc.lpszScheme = scheme;
    uc.dwSchemeLength = 16;

    if (!WinHttpCrackUrl(wurl, 0, 0, &uc)) {
        free(wurl);
        return NULL;
    }
    free(wurl);

    BOOL is_https = (wcsncmp(scheme, L"https", 5) == 0);
    int port = uc.nPort;
    if (port == 0) port = is_https ? 443 : 80;

    HINTERNET hSession = WinHttpOpen(L"OjisanLang/1.0",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      NULL, NULL, 0);
    if (!hSession) return NULL;

    HINTERNET hConnect = WinHttpConnect(hSession, host, port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return NULL; }

    wchar_t* wmethod = utf8_to_wide(method);
    DWORD flags = is_https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, wmethod, path,
                                             NULL, NULL, NULL, flags);
    free(wmethod);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return NULL;
    }

    
    if (extra_headers && extra_headers[0]) {
        wchar_t* wheaders = utf8_to_wide(extra_headers);
        if (wheaders) {
            WinHttpAddRequestHeaders(hRequest, wheaders, (DWORD)-1,
                                      WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
            free(wheaders);
        }
    }

    
    DWORD blen = body ? (DWORD)body_len : 0;
    DWORD total = blen;
    if (!WinHttpSendRequest(hRequest,
                             WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                             body ? (void*)body : WINHTTP_NO_REQUEST_DATA,
                             blen, total, 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return NULL;
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return NULL;
    }

    
    if (out_status) {
        DWORD status = 0;
        DWORD sz = sizeof(status);
        DWORD idx = 0;
        WinHttpQueryHeaders(hRequest,
                             WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             NULL, &status, &sz, &idx);
        *out_status = (int)status;
    }

    
    if (out_headers) {
        DWORD hdr_size = 0;
        DWORD idx = 0;
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                             NULL, NULL, &hdr_size, &idx);
        if (hdr_size > 0) {
            wchar_t* whdr = malloc(hdr_size);
            idx = 0;
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                                     NULL, whdr, &hdr_size, &idx)) {
                *out_headers = wide_to_utf8(whdr, (int)(hdr_size / sizeof(wchar_t)));
            } else {
                *out_headers = NULL;
            }
            free(whdr);
        } else {
            *out_headers = NULL;
        }
    }

    
    char* result = NULL;
    int result_len = 0;
    int result_cap = 0;
    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
        if (result_len + (int)avail + 1 > result_cap) {
            result_cap = result_len + (int)avail + 256;
            result = realloc(result, result_cap);
        }
        DWORD read = 0;
        WinHttpReadData(hRequest, result + result_len, avail, &read);
        result_len += (int)read;
        avail = 0;
    }
    if (result) {
        result = realloc(result, result_len + 1);
        result[result_len] = '\0';
    } else {
        result = malloc(1);
        result[0] = '\0';
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}
#endif

static Value builtin_clock(int argCount, Value* args) {
    (void)argCount; (void)args;
    return FLOAT_VAL((double)clock() / CLOCKS_PER_SEC);
}

static Value builtin_print(int argCount, Value* args) {
    for (int i = 0; i < argCount; i++) {
        value_print(args[i]);
        if (i < argCount - 1) printf(" ");
    }
    printf("\n");
    return NULL_VAL;
}

static Value builtin_input(int argCount, Value* args) {
    if (argCount > 0) {
        value_print(args[0]);
        fflush(stdout);
    }
#ifdef _WIN32
    
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn != INVALID_HANDLE_VALUE && _isatty(_fileno(stdin))) {
        wchar_t wbuf[1024];
        DWORD read_count = 0;
        if (ReadConsoleW(hIn, wbuf, 1023, &read_count, NULL) && read_count > 0) {
            wbuf[read_count] = L'\0';
            
            if (read_count > 0 && wbuf[read_count - 1] == L'\n') wbuf[--read_count] = L'\0';
            if (read_count > 0 && wbuf[read_count - 1] == L'\r') wbuf[--read_count] = L'\0';
            
            int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wbuf, (int)read_count, NULL, 0, NULL, NULL);
            if (utf8_len > 0) {
                char* utf8_buf = malloc(utf8_len + 1);
                WideCharToMultiByte(CP_UTF8, 0, wbuf, (int)read_count, utf8_buf, utf8_len, NULL, NULL);
                utf8_buf[utf8_len] = '\0';
                Value result = OBJ_VAL(copy_string_value(utf8_buf, utf8_len));
                free(utf8_buf);
                return result;
            }
        }
        return NULL_VAL;
    }
#endif
    
    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), stdin)) {
        int len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
            len--;
        }
        return OBJ_VAL(copy_string_value(buffer, len));
    }
    return NULL_VAL;
}

static Value builtin_type(int argCount, Value* args) {
    if (argCount < 1) return NULL_VAL;
    const char* name = value_type_name(args[0]);
    return OBJ_VAL(copy_string_value(name, strlen(name)));
}


static int value_to_string_buf(Value value, char* buffer, int buf_size);


typedef struct {
    char* buffer;
    int buf_size;
    int offset;
    bool first;
} DictStringContext;

static void dict_string_entry(const char* key, void* val, void* userdata) {
    DictStringContext* ctx = (DictStringContext*)userdata;
    if (!ctx->first) {
        ctx->offset += snprintf(ctx->buffer + ctx->offset, ctx->buf_size - ctx->offset, "、");
    }
    ctx->first = false;
    ctx->offset += snprintf(ctx->buffer + ctx->offset, ctx->buf_size - ctx->offset, "%s→", key);
    ctx->offset += value_to_string_buf(*(Value*)val, ctx->buffer + ctx->offset, ctx->buf_size - ctx->offset);
}

static int value_to_string_buf(Value value, char* buffer, int buf_size) {
    switch (value.type) {
        case VAL_NULL: return snprintf(buffer, buf_size, "ナイナイ");
        case VAL_BOOL: return snprintf(buffer, buf_size, AS_BOOL(value) ? "マジ" : "ウソ");
        case VAL_INT: return snprintf(buffer, buf_size, "%lld", AS_INT(value));
        case VAL_FLOAT: return snprintf(buffer, buf_size, "%g", AS_FLOAT(value));
        case VAL_OBJ:
            switch (AS_OBJ(value)->type) {
                case OBJ_STRING:
                    return snprintf(buffer, buf_size, "%s", ((ObjString*)AS_OBJ(value))->chars);
                case OBJ_LIST: {
                    ObjList* list = (ObjList*)AS_OBJ(value);
                    int offset = snprintf(buffer, buf_size, "【");
                    for (int i = 0; i < list->count; i++) {
                        if (i > 0) offset += snprintf(buffer + offset, buf_size - offset, "、");
                        offset += value_to_string_buf(list->items[i], buffer + offset, buf_size - offset);
                    }
                    offset += snprintf(buffer + offset, buf_size - offset, "】");
                    return offset;
                }
                case OBJ_DICT: {
                    ObjDict* dict = (ObjDict*)AS_OBJ(value);
                    int offset = snprintf(buffer, buf_size, "《");
                    DictStringContext ctx = { .buffer = buffer, .buf_size = buf_size, .offset = offset, .first = true };
                    table_iterate(dict->items, dict_string_entry, &ctx);
                    offset = ctx.offset;
                    offset += snprintf(buffer + offset, buf_size - offset, "》");
                    return offset;
                }
                default:
                    return snprintf(buffer, buf_size, "オブジェクト");
            }
    }
    return snprintf(buffer, buf_size, "オブジェクト");
}

static Value builtin_to_string(int argCount, Value* args) {
    if (argCount < 1) return OBJ_VAL(copy_string_value("", 0));

    Value value = args[0];

    
    if (IS_OBJ(value) && AS_OBJ(value)->type == OBJ_STRING) {
        return value;
    }

    
    char stack_buf[4096];
    int needed = value_to_string_buf(value, stack_buf, sizeof(stack_buf));
    if (needed < (int)sizeof(stack_buf)) {
        return OBJ_VAL(copy_string_value(stack_buf, strlen(stack_buf)));
    }
    
    int big_size = needed + 256;
    char* heap_buf = malloc(big_size);
    value_to_string_buf(value, heap_buf, big_size);
    ObjString* result = copy_string_value(heap_buf, strlen(heap_buf));
    free(heap_buf);
    return OBJ_VAL(result);
}

static Value builtin_to_number(int argCount, Value* args) {
    if (argCount < 1) return INT_VAL(0);
    Value v = args[0];
    if (IS_INT(v)) return v;
    if (IS_FLOAT(v)) return v;
    if (IS_OBJ(v)) {
        if (AS_OBJ(v)->type == OBJ_STRING) {
            char* chars = ((ObjString*)AS_OBJ(v))->chars;
            char* endptr = NULL;
            if (strchr(chars, '.')) {
                double d = strtod(chars, &endptr);
                
                if (endptr == chars) return NULL_VAL;
                return FLOAT_VAL(d);
            }
            long long ll = strtoll(chars, &endptr, 10);
            if (endptr == chars) return NULL_VAL;
            return INT_VAL(ll);
        }
    }
    return INT_VAL(0);
}

static Value builtin_length(int argCount, Value* args) {
    if (argCount < 1) return INT_VAL(0);
    Value v = args[0];
    if (IS_OBJ(v)) {
        if (AS_OBJ(v)->type == OBJ_STRING) {
            return INT_VAL(((ObjString*)AS_OBJ(v))->length);
        }
        if (AS_OBJ(v)->type == OBJ_LIST) {
            return INT_VAL(((ObjList*)AS_OBJ(v))->count);
        }
    }
    return INT_VAL(0);
}

static Value builtin_random(int argCount, Value* args) {
    (void)argCount; (void)args;
    return FLOAT_VAL((double)rand() / (double)RAND_MAX);
}


static Value builtin_random_range(int argCount, Value* args) {
    if (argCount < 2) return INT_VAL(0);
    long long min_val = 0, max_val = 0;
    if (IS_INT(args[0])) min_val = AS_INT(args[0]);
    else if (IS_FLOAT(args[0])) min_val = (long long)AS_FLOAT(args[0]);
    if (IS_INT(args[1])) max_val = AS_INT(args[1]);
    else if (IS_FLOAT(args[1])) max_val = (long long)AS_FLOAT(args[1]);
    if (min_val > max_val) { long long tmp = min_val; min_val = max_val; max_val = tmp; }
    long long range = max_val - min_val + 1;
    if (range <= 0) return INT_VAL(min_val);
    return INT_VAL(min_val + (rand() % range));
}


static Value builtin_contains(int argCount, Value* args) {
    if (argCount < 2) return BOOL_VAL(false);
    if (!IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_STRING) return BOOL_VAL(false);
    if (!IS_OBJ(args[1]) || AS_OBJ(args[1])->type != OBJ_STRING) return BOOL_VAL(false);
    char* haystack = ((ObjString*)AS_OBJ(args[0]))->chars;
    char* needle = ((ObjString*)AS_OBJ(args[1]))->chars;
    return BOOL_VAL(strstr(haystack, needle) != NULL);
}


static Value builtin_floor(int argCount, Value* args) {
    if (argCount < 1) return INT_VAL(0);
    Value v = args[0];
    if (IS_INT(v)) return v;
    if (IS_FLOAT(v)) return FLOAT_VAL(floor(AS_FLOAT(v)));
    return INT_VAL(0);
}

static Value builtin_ceil(int argCount, Value* args) {
    if (argCount < 1) return INT_VAL(0);
    Value v = args[0];
    if (IS_INT(v)) return v;
    if (IS_FLOAT(v)) return FLOAT_VAL(ceil(AS_FLOAT(v)));
    return INT_VAL(0);
}

static Value builtin_round(int argCount, Value* args) {
    if (argCount < 1) return INT_VAL(0);
    Value v = args[0];
    if (IS_INT(v)) return v;
    if (IS_FLOAT(v)) return FLOAT_VAL(round(AS_FLOAT(v)));
    return INT_VAL(0);
}

static Value builtin_abs(int argCount, Value* args) {
    if (argCount < 1) return INT_VAL(0);
    Value v = args[0];
    if (IS_INT(v)) return INT_VAL(llabs(AS_INT(v)));
    if (IS_FLOAT(v)) return FLOAT_VAL(fabs(AS_FLOAT(v)));
    return INT_VAL(0);
}

static Value builtin_max(int argCount, Value* args) {
    if (argCount < 2) return NULL_VAL;
    Value a = args[0], b = args[1];
    if (IS_INT(a) && IS_INT(b)) return AS_INT(a) >= AS_INT(b) ? a : b;
    double da = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
    double db = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
    return da >= db ? a : b;
}

static Value builtin_min(int argCount, Value* args) {
    if (argCount < 2) return NULL_VAL;
    Value a = args[0], b = args[1];
    if (IS_INT(a) && IS_INT(b)) return AS_INT(a) <= AS_INT(b) ? a : b;
    double da = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
    double db = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
    return da <= db ? a : b;
}

static Value builtin_sqrt(int argCount, Value* args) {
    if (argCount < 1) return FLOAT_VAL(0.0);
    Value v = args[0];
    if (IS_INT(v)) return FLOAT_VAL(sqrt((double)AS_INT(v)));
    if (IS_FLOAT(v)) return FLOAT_VAL(sqrt(AS_FLOAT(v)));
    return FLOAT_VAL(0.0);
}


static Value builtin_clear_screen(int argCount, Value* args) {
    (void)argCount; (void)args;
    printf("\033[2J\033[H");
    fflush(stdout);
    return NULL_VAL;
}


static Value builtin_cursor_up(int argCount, Value* args) {
    int n = 1;
    if (argCount > 0 && IS_INT(args[0])) n = (int)AS_INT(args[0]);
    if (n < 1) n = 1;
    printf("\033[%dA", n);
    fflush(stdout);
    return NULL_VAL;
}


static Value builtin_clear_line(int argCount, Value* args) {
    (void)argCount; (void)args;
    printf("\033[2K\r");
    fflush(stdout);
    return NULL_VAL;
}


static Value builtin_cursor_move(int argCount, Value* args) {
    int row = 1, col = 1;
    if (argCount > 0 && IS_INT(args[0])) row = (int)AS_INT(args[0]);
    if (argCount > 1 && IS_INT(args[1])) col = (int)AS_INT(args[1]);
    if (row < 1) row = 1;
    if (col < 1) col = 1;
    printf("\033[%d;%dH", row, col);
    fflush(stdout);
    return NULL_VAL;
}


static Value builtin_sleep(int argCount, Value* args) {
    if (argCount < 1 || !IS_INT(args[0])) return NULL_VAL;
    int ms = (int)AS_INT(args[0]);
    if (ms < 0) ms = 0;
#ifdef _WIN32
    
    extern __declspec(dllimport) void __stdcall Sleep(unsigned long);
    Sleep((unsigned long)ms);
#else
    usleep(ms * 1000);
#endif
    return NULL_VAL;
}

static Value builtin_to_int(int argCount, Value* args) {
    if (argCount < 1) return INT_VAL(0);
    Value v = args[0];
    if (IS_INT(v)) return v;
    if (IS_FLOAT(v)) return INT_VAL((long long)AS_FLOAT(v));
    if (IS_OBJ(v) && AS_OBJ(v)->type == OBJ_STRING) {
        return INT_VAL(strtoll(((ObjString*)AS_OBJ(v))->chars, NULL, 10));
    }
    return INT_VAL(0);
}


static Value builtin_split(int argCount, Value* args) {
    if (argCount < 2) return OBJ_VAL(new_list());
    if (!IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_STRING) return OBJ_VAL(new_list());
    if (!IS_OBJ(args[1]) || AS_OBJ(args[1])->type != OBJ_STRING) return OBJ_VAL(new_list());
    char* str = ((ObjString*)AS_OBJ(args[0]))->chars;
    char* delim = ((ObjString*)AS_OBJ(args[1]))->chars;
    int delim_len = strlen(delim);
    ObjList* list = new_list();
    if (delim_len == 0) {
        
        int len = strlen(str);
        for (int i = 0; i < len; ) {
            unsigned char c = (unsigned char)str[i];
            int charlen = 1;
            if (c >= 0xC0 && c < 0xE0) charlen = 2;
            else if (c >= 0xE0 && c < 0xF0) charlen = 3;
            else if (c >= 0xF0) charlen = 4;
            ObjString* s = copy_string_value(str + i, charlen);
            if (list->count + 1 > list->capacity) {
                list->capacity = list->capacity < 8 ? 8 : list->capacity * 2;
                list->items = realloc(list->items, sizeof(Value) * list->capacity);
            }
            list->items[list->count++] = OBJ_VAL(s);
            i += charlen;
        }
        return OBJ_VAL(list);
    }
    char* p = str;
    while (1) {
        char* found = strstr(p, delim);
        int seg_len = found ? (int)(found - p) : (int)strlen(p);
        ObjString* s = copy_string_value(p, seg_len);
        if (list->count + 1 > list->capacity) {
            list->capacity = list->capacity < 8 ? 8 : list->capacity * 2;
            list->items = realloc(list->items, sizeof(Value) * list->capacity);
        }
        list->items[list->count++] = OBJ_VAL(s);
        if (!found) break;
        p = found + delim_len;
    }
    return OBJ_VAL(list);
}


static Value builtin_join(int argCount, Value* args) {
    if (argCount < 1) return OBJ_VAL(copy_string_value("", 0));
    if (!IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_LIST) return OBJ_VAL(copy_string_value("", 0));
    ObjList* list = (ObjList*)AS_OBJ(args[0]);
    char* delim = "";
    if (argCount >= 2 && IS_OBJ(args[1]) && AS_OBJ(args[1])->type == OBJ_STRING) {
        delim = ((ObjString*)AS_OBJ(args[1]))->chars;
    }
    int total = 0;
    int delim_len = strlen(delim);
    
    char buf[64];
    for (int i = 0; i < list->count; i++) {
        if (i > 0) total += delim_len;
        Value v = list->items[i];
        if (IS_OBJ(v) && AS_OBJ(v)->type == OBJ_STRING) total += ((ObjString*)AS_OBJ(v))->length;
        else { snprintf(buf, sizeof(buf), "%lld", IS_INT(v) ? AS_INT(v) : 0LL); total += strlen(buf); }
    }
    char* result = malloc(total + 1);
    result[0] = '\0';
    int offset = 0;
    for (int i = 0; i < list->count; i++) {
        if (i > 0) { memcpy(result + offset, delim, delim_len); offset += delim_len; }
        Value v = list->items[i];
        if (IS_OBJ(v) && AS_OBJ(v)->type == OBJ_STRING) {
            ObjString* s = (ObjString*)AS_OBJ(v);
            memcpy(result + offset, s->chars, s->length);
            offset += s->length;
        } else {
            int n = snprintf(result + offset, total + 1 - offset, "%lld", IS_INT(v) ? AS_INT(v) : 0LL);
            offset += n;
        }
    }
    result[offset] = '\0';
    ObjString* str = copy_string_value(result, offset);
    free(result);
    return OBJ_VAL(str);
}


static Value builtin_substring(int argCount, Value* args) {
    if (argCount < 2) return NULL_VAL;
    if (!IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_STRING) return NULL_VAL;
    ObjString* str = (ObjString*)AS_OBJ(args[0]);
    long long start = IS_INT(args[1]) ? AS_INT(args[1]) : 0;
    long long end = (argCount >= 3 && IS_INT(args[2])) ? AS_INT(args[2]) : str->length;
    if (start < 0) start = 0;
    if (end > str->length) end = str->length;
    if (start >= end) return OBJ_VAL(copy_string_value("", 0));
    return OBJ_VAL(copy_string_value(str->chars + start, (int)(end - start)));
}


static Value builtin_replace(int argCount, Value* args) {
    if (argCount < 3) return NULL_VAL;
    if (!IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_STRING) return NULL_VAL;
    if (!IS_OBJ(args[1]) || AS_OBJ(args[1])->type != OBJ_STRING) return NULL_VAL;
    if (!IS_OBJ(args[2]) || AS_OBJ(args[2])->type != OBJ_STRING) return NULL_VAL;
    char* src = ((ObjString*)AS_OBJ(args[0]))->chars;
    char* search = ((ObjString*)AS_OBJ(args[1]))->chars;
    char* replace = ((ObjString*)AS_OBJ(args[2]))->chars;
    int search_len = strlen(search);
    int replace_len = strlen(replace);
    if (search_len == 0) return args[0];
    
    int count = 0;
    char* p = src;
    while ((p = strstr(p, search)) != NULL) { count++; p += search_len; }
    int new_len = strlen(src) + count * (replace_len - search_len);
    char* result = malloc(new_len + 1);
    char* dst = result;
    p = src;
    while (1) {
        char* found = strstr(p, search);
        if (!found) { strcpy(dst, p); break; }
        memcpy(dst, p, found - p);
        dst += found - p;
        memcpy(dst, replace, replace_len);
        dst += replace_len;
        p = found + search_len;
    }
    ObjString* str = copy_string_value(result, strlen(result));
    free(result);
    return OBJ_VAL(str);
}


static Value builtin_trim(int argCount, Value* args) {
    if (argCount < 1 || !IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_STRING) return NULL_VAL;
    char* str = ((ObjString*)AS_OBJ(args[0]))->chars;
    int len = ((ObjString*)AS_OBJ(args[0]))->length;
    int start = 0, end = len;
    while (start < end && (str[start] == ' ' || str[start] == '\t' || str[start] == '\n' || str[start] == '\r')) start++;
    while (end > start && (str[end-1] == ' ' || str[end-1] == '\t' || str[end-1] == '\n' || str[end-1] == '\r')) end--;
    return OBJ_VAL(copy_string_value(str + start, end - start));
}


static Value builtin_upper(int argCount, Value* args) {
    if (argCount < 1 || !IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_STRING) return NULL_VAL;
    ObjString* s = (ObjString*)AS_OBJ(args[0]);
    char* result = malloc(s->length + 1);
    for (int i = 0; i < s->length; i++) {
        char c = s->chars[i];
        result[i] = (c >= 'a' && c <= 'z') ? c - 32 : c;
    }
    result[s->length] = '\0';
    ObjString* r = copy_string_value(result, s->length);
    free(result);
    return OBJ_VAL(r);
}


static Value builtin_lower(int argCount, Value* args) {
    if (argCount < 1 || !IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_STRING) return NULL_VAL;
    ObjString* s = (ObjString*)AS_OBJ(args[0]);
    char* result = malloc(s->length + 1);
    for (int i = 0; i < s->length; i++) {
        char c = s->chars[i];
        result[i] = (c >= 'A' && c <= 'Z') ? c + 32 : c;
    }
    result[s->length] = '\0';
    ObjString* r = copy_string_value(result, s->length);
    free(result);
    return OBJ_VAL(r);
}


static Value builtin_index_of(int argCount, Value* args) {
    if (argCount < 2) return INT_VAL(-1);
    if (!IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_STRING) return INT_VAL(-1);
    if (!IS_OBJ(args[1]) || AS_OBJ(args[1])->type != OBJ_STRING) return INT_VAL(-1);
    char* haystack = ((ObjString*)AS_OBJ(args[0]))->chars;
    char* needle = ((ObjString*)AS_OBJ(args[1]))->chars;
    char* found = strstr(haystack, needle);
    if (!found) return INT_VAL(-1);
    return INT_VAL((long long)(found - haystack));
}


static Value builtin_starts_with(int argCount, Value* args) {
    if (argCount < 2) return BOOL_VAL(false);
    if (!IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_STRING) return BOOL_VAL(false);
    if (!IS_OBJ(args[1]) || AS_OBJ(args[1])->type != OBJ_STRING) return BOOL_VAL(false);
    char* str = ((ObjString*)AS_OBJ(args[0]))->chars;
    char* prefix = ((ObjString*)AS_OBJ(args[1]))->chars;
    return BOOL_VAL(strncmp(str, prefix, strlen(prefix)) == 0);
}


static Value builtin_ends_with(int argCount, Value* args) {
    if (argCount < 2) return BOOL_VAL(false);
    if (!IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_STRING) return BOOL_VAL(false);
    if (!IS_OBJ(args[1]) || AS_OBJ(args[1])->type != OBJ_STRING) return BOOL_VAL(false);
    char* str = ((ObjString*)AS_OBJ(args[0]))->chars;
    char* suffix = ((ObjString*)AS_OBJ(args[1]))->chars;
    int slen = strlen(str), suflen = strlen(suffix);
    if (suflen > slen) return BOOL_VAL(false);
    return BOOL_VAL(strcmp(str + slen - suflen, suffix) == 0);
}


static Value builtin_repeat(int argCount, Value* args) {
    if (argCount < 2) return NULL_VAL;
    if (!IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_STRING) return NULL_VAL;
    ObjString* s = (ObjString*)AS_OBJ(args[0]);
    long long count = IS_INT(args[1]) ? AS_INT(args[1]) : 0;
    if (count <= 0) return OBJ_VAL(copy_string_value("", 0));
    if (count > 10000) count = 10000; 
    int total = s->length * (int)count;
    char* result = malloc(total + 1);
    for (int i = 0; i < (int)count; i++) {
        memcpy(result + i * s->length, s->chars, s->length);
    }
    result[total] = '\0';
    ObjString* r = copy_string_value(result, total);
    free(result);
    return OBJ_VAL(r);
}


static Value builtin_char_code_at(int argCount, Value* args) {
    if (argCount < 2) return INT_VAL(0);
    if (!IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_STRING) return INT_VAL(0);
    ObjString* s = (ObjString*)AS_OBJ(args[0]);
    long long idx = IS_INT(args[1]) ? AS_INT(args[1]) : 0;
    if (idx < 0 || idx >= s->length) return INT_VAL(0);
    return INT_VAL((long long)(unsigned char)s->chars[idx]);
}


static Value builtin_pop(int argCount, Value* args) {
    if (argCount < 1 || !IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_LIST) return NULL_VAL;
    ObjList* list = (ObjList*)AS_OBJ(args[0]);
    if (list->count == 0) return NULL_VAL;
    return list->items[--list->count];
}


static Value builtin_shift(int argCount, Value* args) {
    if (argCount < 1 || !IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_LIST) return NULL_VAL;
    ObjList* list = (ObjList*)AS_OBJ(args[0]);
    if (list->count == 0) return NULL_VAL;
    Value first = list->items[0];
    memmove(list->items, list->items + 1, sizeof(Value) * (list->count - 1));
    list->count--;
    return first;
}


static Value builtin_slice(int argCount, Value* args) {
    if (argCount < 2 || !IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_LIST) return OBJ_VAL(new_list());
    ObjList* list = (ObjList*)AS_OBJ(args[0]);
    long long start = IS_INT(args[1]) ? AS_INT(args[1]) : 0;
    long long end = (argCount >= 3 && IS_INT(args[2])) ? AS_INT(args[2]) : list->count;
    if (start < 0) start = 0;
    if (end > list->count) end = list->count;
    if (start >= end) return OBJ_VAL(new_list());
    ObjList* result = new_list();
    int count = (int)(end - start);
    result->items = malloc(sizeof(Value) * count);
    result->capacity = count;
    result->count = count;
    memcpy(result->items, list->items + start, sizeof(Value) * count);
    return OBJ_VAL(result);
}


static int sort_compare(const void* a, const void* b) {
    Value va = *(const Value*)a;
    Value vb = *(const Value*)b;
    if (IS_INT(va) && IS_INT(vb)) return (AS_INT(va) > AS_INT(vb)) - (AS_INT(va) < AS_INT(vb));
    double da = IS_INT(va) ? (double)AS_INT(va) : (IS_FLOAT(va) ? AS_FLOAT(va) : 0.0);
    double db = IS_INT(vb) ? (double)AS_INT(vb) : (IS_FLOAT(vb) ? AS_FLOAT(vb) : 0.0);
    return (da > db) - (da < db);
}


static Value builtin_sort(int argCount, Value* args) {
    if (argCount < 1 || !IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_LIST) return NULL_VAL;
    ObjList* list = (ObjList*)AS_OBJ(args[0]);
    if (list->count > 1) qsort(list->items, list->count, sizeof(Value), sort_compare);
    return args[0];
}


static Value builtin_reverse(int argCount, Value* args) {
    if (argCount < 1 || !IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_LIST) return NULL_VAL;
    ObjList* list = (ObjList*)AS_OBJ(args[0]);
    for (int i = 0; i < list->count / 2; i++) {
        Value tmp = list->items[i];
        list->items[i] = list->items[list->count - 1 - i];
        list->items[list->count - 1 - i] = tmp;
    }
    return args[0];
}


static Value builtin_array_index_of(int argCount, Value* args) {
    if (argCount < 2 || !IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_LIST) return INT_VAL(-1);
    ObjList* list = (ObjList*)AS_OBJ(args[0]);
    for (int i = 0; i < list->count; i++) {
        if (value_equal(list->items[i], args[1])) return INT_VAL(i);
    }
    return INT_VAL(-1);
}


static Value builtin_remove(int argCount, Value* args) {
    if (argCount < 2 || !IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_LIST) return NULL_VAL;
    ObjList* list = (ObjList*)AS_OBJ(args[0]);
    long long idx = IS_INT(args[1]) ? AS_INT(args[1]) : -1;
    if (idx < 0 || idx >= list->count) return NULL_VAL;
    Value removed = list->items[idx];
    memmove(list->items + idx, list->items + idx + 1, sizeof(Value) * (list->count - idx - 1));
    list->count--;
    return removed;
}


typedef struct { ObjList* list; } KeysCtx;
static void keys_callback(const char* key, void* val, void* userdata) {
    (void)val;
    KeysCtx* ctx = (KeysCtx*)userdata;
    ObjString* s = copy_string_value(key, strlen(key));
    if (ctx->list->count + 1 > ctx->list->capacity) {
        ctx->list->capacity = ctx->list->capacity < 8 ? 8 : ctx->list->capacity * 2;
        ctx->list->items = realloc(ctx->list->items, sizeof(Value) * ctx->list->capacity);
    }
    ctx->list->items[ctx->list->count++] = OBJ_VAL(s);
}

static Value builtin_keys(int argCount, Value* args) {
    if (argCount < 1 || !IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_DICT) return OBJ_VAL(new_list());
    ObjDict* dict = (ObjDict*)AS_OBJ(args[0]);
    ObjList* list = new_list();
    KeysCtx ctx = { .list = list };
    table_iterate(dict->items, keys_callback, &ctx);
    return OBJ_VAL(list);
}


typedef struct { ObjList* list; } ValuesCtx;
static void values_callback(const char* key, void* val, void* userdata) {
    (void)key;
    ValuesCtx* ctx = (ValuesCtx*)userdata;
    if (ctx->list->count + 1 > ctx->list->capacity) {
        ctx->list->capacity = ctx->list->capacity < 8 ? 8 : ctx->list->capacity * 2;
        ctx->list->items = realloc(ctx->list->items, sizeof(Value) * ctx->list->capacity);
    }
    ctx->list->items[ctx->list->count++] = *(Value*)val;
}

static Value builtin_values(int argCount, Value* args) {
    if (argCount < 1 || !IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_DICT) return OBJ_VAL(new_list());
    ObjDict* dict = (ObjDict*)AS_OBJ(args[0]);
    ObjList* list = new_list();
    ValuesCtx ctx = { .list = list };
    table_iterate(dict->items, values_callback, &ctx);
    return OBJ_VAL(list);
}


static Value builtin_has_key(int argCount, Value* args) {
    if (argCount < 2 || !IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_DICT) return BOOL_VAL(false);
    if (!IS_OBJ(args[1]) || AS_OBJ(args[1])->type != OBJ_STRING) return BOOL_VAL(false);
    ObjDict* dict = (ObjDict*)AS_OBJ(args[0]);
    char* key = ((ObjString*)AS_OBJ(args[1]))->chars;
    void* val;
    return BOOL_VAL(table_get(dict->items, key, &val));
}


static Value builtin_dict_delete(int argCount, Value* args) {
    if (argCount < 2 || !IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_DICT) return BOOL_VAL(false);
    if (!IS_OBJ(args[1]) || AS_OBJ(args[1])->type != OBJ_STRING) return BOOL_VAL(false);
    ObjDict* dict = (ObjDict*)AS_OBJ(args[0]);
    char* key = ((ObjString*)AS_OBJ(args[1]))->chars;
    return BOOL_VAL(table_delete(dict->items, key));
}


typedef struct { ObjDict* dst; } MergeCtx;
static void merge_callback(const char* key, void* val, void* userdata) {
    MergeCtx* ctx = (MergeCtx*)userdata;
    Value* vPtr = malloc(sizeof(Value));
    *vPtr = *(Value*)val;
    table_set(ctx->dst->items, key, vPtr);
}

static Value builtin_merge(int argCount, Value* args) {
    if (argCount < 2) return OBJ_VAL(new_dict());
    if (!IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_DICT) return OBJ_VAL(new_dict());
    if (!IS_OBJ(args[1]) || AS_OBJ(args[1])->type != OBJ_DICT) return args[0];
    ObjDict* result = new_dict();
    MergeCtx ctx1 = { .dst = result };
    table_iterate(((ObjDict*)AS_OBJ(args[0]))->items, merge_callback, &ctx1);
    MergeCtx ctx2 = { .dst = result };
    table_iterate(((ObjDict*)AS_OBJ(args[1]))->items, merge_callback, &ctx2);
    return OBJ_VAL(result);
}


static Value builtin_http_get(int argCount, Value* args) {
#ifdef _WIN32
    if (argCount < 1 || !IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_STRING) return NULL_VAL;
    char* url = ((ObjString*)AS_OBJ(args[0]))->chars;
    char* body = winhttp_request("GET", url, NULL, 0, NULL, NULL, NULL);
    if (!body) return NULL_VAL;
    ObjString* result = copy_string_value(body, strlen(body));
    free(body);
    return OBJ_VAL(result);
#else
    (void)argCount; (void)args;
    return NULL_VAL;
#endif
}


static Value builtin_http_post(int argCount, Value* args) {
#ifdef _WIN32
    if (argCount < 2) return NULL_VAL;
    if (!IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_STRING) return NULL_VAL;
    if (!IS_OBJ(args[1]) || AS_OBJ(args[1])->type != OBJ_STRING) return NULL_VAL;
    char* url = ((ObjString*)AS_OBJ(args[0]))->chars;
    char* req_body = ((ObjString*)AS_OBJ(args[1]))->chars;
    char* resp = winhttp_request("POST", url, req_body, (int)strlen(req_body),
                                  "Content-Type: application/json\r\n", NULL, NULL);
    if (!resp) return NULL_VAL;
    ObjString* result = copy_string_value(resp, strlen(resp));
    free(resp);
    return OBJ_VAL(result);
#else
    (void)argCount; (void)args;
    return NULL_VAL;
#endif
}


static Value builtin_http_request(int argCount, Value* args) {
#ifdef _WIN32
    if (argCount < 2) return NULL_VAL;
    
    if (!IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_STRING) return NULL_VAL;
    char* method = ((ObjString*)AS_OBJ(args[0]))->chars;
    
    if (!IS_OBJ(args[1]) || AS_OBJ(args[1])->type != OBJ_STRING) return NULL_VAL;
    char* url = ((ObjString*)AS_OBJ(args[1]))->chars;
    
    char* req_body = NULL;
    int req_body_len = 0;
    if (argCount >= 3 && IS_OBJ(args[2]) && AS_OBJ(args[2])->type == OBJ_STRING) {
        req_body = ((ObjString*)AS_OBJ(args[2]))->chars;
        req_body_len = ((ObjString*)AS_OBJ(args[2]))->length;
    }
    
    char* extra_headers = NULL;
    if (argCount >= 4 && IS_OBJ(args[3]) && AS_OBJ(args[3])->type == OBJ_DICT) {
        ObjDict* hdr_dict = (ObjDict*)AS_OBJ(args[3]);
        
        ObjList* keys = new_list();
        KeysCtx kctx = { .list = keys };
        table_iterate(hdr_dict->items, keys_callback, &kctx);
        
        int hdr_cap = 256;
        int hdr_len = 0;
        extra_headers = malloc(hdr_cap);
        extra_headers[0] = '\0';
        for (int i = 0; i < keys->count; i++) {
            char* key = ((ObjString*)AS_OBJ(keys->items[i]))->chars;
            void* valPtr;
            if (table_get(hdr_dict->items, key, &valPtr)) {
                Value v = *(Value*)valPtr;
                if (IS_OBJ(v) && AS_OBJ(v)->type == OBJ_STRING) {
                    char* val = ((ObjString*)AS_OBJ(v))->chars;
                    int need = hdr_len + (int)strlen(key) + 2 + (int)strlen(val) + 3;
                    if (need > hdr_cap) {
                        hdr_cap = need + 128;
                        extra_headers = realloc(extra_headers, hdr_cap);
                    }
                    hdr_len += sprintf(extra_headers + hdr_len, "%s: %s\r\n", key, val);
                }
            }
        }
    }

    int status = 0;
    char* resp_headers = NULL;
    char* resp_body = winhttp_request(method, url, req_body, req_body_len,
                                       extra_headers, &status, &resp_headers);
    if (extra_headers) free(extra_headers);

    
    ObjDict* result = new_dict();

    
    Value* sPtr = malloc(sizeof(Value));
    *sPtr = INT_VAL(status);
    table_set(result->items, "status", sPtr);

    
    Value* bPtr = malloc(sizeof(Value));
    if (resp_body) {
        *bPtr = OBJ_VAL(copy_string_value(resp_body, strlen(resp_body)));
        free(resp_body);
    } else {
        *bPtr = OBJ_VAL(copy_string_value("", 0));
    }
    table_set(result->items, "body", bPtr);

    
    Value* hPtr = malloc(sizeof(Value));
    if (resp_headers) {
        *hPtr = OBJ_VAL(copy_string_value(resp_headers, strlen(resp_headers)));
        free(resp_headers);
    } else {
        *hPtr = OBJ_VAL(copy_string_value("", 0));
    }
    table_set(result->items, "headers", hPtr);

    return OBJ_VAL(result);
#else
    (void)argCount; (void)args;
    return NULL_VAL;
#endif
}

void register_builtins(Environment* env) {
    env_define(env, "時計チャン", OBJ_VAL(new_native(builtin_clock)));

    
    env_define(env, "型を教えてヨ😃", OBJ_VAL(new_native(builtin_type)));
    env_define(env, "文字にしてネ😘", OBJ_VAL(new_native(builtin_to_string)));
    env_define(env, "数字にしてネ😘", OBJ_VAL(new_native(builtin_to_number)));
    env_define(env, "長さを教えてヨ😃", OBJ_VAL(new_native(builtin_length)));
    env_define(env, "ランダムチャン😃", OBJ_VAL(new_native(builtin_random)));
    env_define(env, "チョット教えてヨ😃", OBJ_VAL(new_native(builtin_input)));

    
    env_define(env, "切り捨てチャン😃", OBJ_VAL(new_native(builtin_floor)));
    env_define(env, "切り上げチャン😃", OBJ_VAL(new_native(builtin_ceil)));
    env_define(env, "四捨五入チャン😃", OBJ_VAL(new_native(builtin_round)));
    env_define(env, "絶対値チャン😃", OBJ_VAL(new_native(builtin_abs)));
    env_define(env, "最大チャン😃", OBJ_VAL(new_native(builtin_max)));
    env_define(env, "最小チャン😃", OBJ_VAL(new_native(builtin_min)));
    env_define(env, "ルートチャン😃", OBJ_VAL(new_native(builtin_sqrt)));
    env_define(env, "整数にしてネ😘", OBJ_VAL(new_native(builtin_to_int)));
    env_define(env, "表示チャン😃", OBJ_VAL(new_native(builtin_print)));
    env_define(env, "ランダム範囲", OBJ_VAL(new_native(builtin_random_range)));
    env_define(env, "含むカナ", OBJ_VAL(new_native(builtin_contains)));

    
    env_define(env, "画面クリア", OBJ_VAL(new_native(builtin_clear_screen)));
    env_define(env, "カーソル上", OBJ_VAL(new_native(builtin_cursor_up)));
    env_define(env, "行クリア", OBJ_VAL(new_native(builtin_clear_line)));
    env_define(env, "カーソル移動", OBJ_VAL(new_native(builtin_cursor_move)));
    env_define(env, "ちょっと待って", OBJ_VAL(new_native(builtin_sleep)));

    
    env_define(env, "分けてネ😘", OBJ_VAL(new_native(builtin_split)));
    env_define(env, "くっつけてネ😘", OBJ_VAL(new_native(builtin_join)));
    env_define(env, "切り取ってネ😘", OBJ_VAL(new_native(builtin_substring)));
    env_define(env, "置き換えてネ😘", OBJ_VAL(new_native(builtin_replace)));
    env_define(env, "スッキリさせてネ😘", OBJ_VAL(new_native(builtin_trim)));
    env_define(env, "デカくしてネ😘", OBJ_VAL(new_native(builtin_upper)));
    env_define(env, "ちいさくしてネ😘", OBJ_VAL(new_native(builtin_lower)));
    env_define(env, "どこにあるノ😃", OBJ_VAL(new_native(builtin_index_of)));
    env_define(env, "先頭合ってるヨ😃", OBJ_VAL(new_native(builtin_starts_with)));
    env_define(env, "末尾合ってるヨ😃", OBJ_VAL(new_native(builtin_ends_with)));
    env_define(env, "繰り返してネ😘", OBJ_VAL(new_native(builtin_repeat)));
    env_define(env, "文字コード教えてヨ😃", OBJ_VAL(new_native(builtin_char_code_at)));

    
    env_define(env, "最後を取ってネ😘", OBJ_VAL(new_native(builtin_pop)));
    env_define(env, "最初を取ってネ😘", OBJ_VAL(new_native(builtin_shift)));
    env_define(env, "切り出してネ😘", OBJ_VAL(new_native(builtin_slice)));
    env_define(env, "並べ替えてネ😘", OBJ_VAL(new_native(builtin_sort)));
    env_define(env, "逆にしてネ😘", OBJ_VAL(new_native(builtin_reverse)));
    env_define(env, "どこにいるノ😃", OBJ_VAL(new_native(builtin_array_index_of)));
    env_define(env, "消してネ😘", OBJ_VAL(new_native(builtin_remove)));

    
    env_define(env, "キー一覧ダヨ😎", OBJ_VAL(new_native(builtin_keys)));
    env_define(env, "値一覧ダヨ😎", OBJ_VAL(new_native(builtin_values)));
    env_define(env, "持ってるヨ😃", OBJ_VAL(new_native(builtin_has_key)));
    env_define(env, "消しちゃうネ😘", OBJ_VAL(new_native(builtin_dict_delete)));
    env_define(env, "合体させてネ😘", OBJ_VAL(new_native(builtin_merge)));

    
    env_define(env, "取ってきてネ😘", OBJ_VAL(new_native(builtin_http_get)));
    env_define(env, "送っちゃうネ😘", OBJ_VAL(new_native(builtin_http_post)));
    env_define(env, "お届けモノだヨ😃", OBJ_VAL(new_native(builtin_http_request)));

    
    srand(time(NULL));
}
