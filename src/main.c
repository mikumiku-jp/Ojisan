#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "lexer.h"
#include "parser.h"
#include "eval.h"
#include "utf8.h"
#include "builtins.h"
#include "gc.h"

#ifdef _WIN32
#include <io.h>

typedef void* HANDLE;
typedef unsigned long DWORD;
typedef int BOOL;
#define STD_INPUT_HANDLE ((DWORD)-10)
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define INVALID_HANDLE_VALUE ((HANDLE)(long long)-1)
#define CP_UTF8 65001
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
__declspec(dllimport) int __stdcall SetConsoleOutputCP(unsigned int);
__declspec(dllimport) int __stdcall SetConsoleCP(unsigned int);
__declspec(dllimport) HANDLE __stdcall GetStdHandle(DWORD);
__declspec(dllimport) BOOL __stdcall ReadConsoleW(HANDLE, void*, DWORD, DWORD*, void*);
__declspec(dllimport) BOOL __stdcall GetConsoleMode(HANDLE, DWORD*);
__declspec(dllimport) BOOL __stdcall SetConsoleMode(HANDLE, DWORD);
__declspec(dllimport) int __stdcall WideCharToMultiByte(unsigned int, DWORD, const wchar_t*, int, char*, int, const char*, int*);
#endif


void run_file(const char* path);
void run_repl();

int main(int argc, char* argv[]) {
    
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    setvbuf(stdout, NULL, _IONBF, 0);
    
    {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
            SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
#endif
    
    if (argc == 1) {
        run_repl();
    } else if (argc == 2) {
        run_file(argv[1]);
    } else {
        fprintf(stderr, "使い方だヨ😘: ojisan [ファイル]\n");
        return 64;
    }
    return 0;
}

static char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "ファイルが開けないヨ😅💦 \"%s\"\n", path);
        exit(74);
    }
    
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);
    
    char* buffer = (char*)malloc(fileSize + 1);
    if (buffer == NULL) {
        fprintf(stderr, "メモリが足りないヨ😱💦\n");
        exit(74);
    }
    
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    buffer[bytesRead] = '\0';
    
    fclose(file);
    return buffer;
}

void run_file(const char* path) {
    const char* dot = strrchr(path, '.');
    if (!dot || (strcmp(dot, ".ojs") != 0 && strcmp(dot, ".oji") != 0)) {
        fprintf(stderr, "おじさんは「.ojs」か「.oji」ファイルしか読めないヨ😅💦: \"%s\"\n", path);
        exit(65);
    }
    char* source = read_file(path);
    interpret(source);
    free(source);
}

void run_repl() {
    char line[1024];
    printf("🍺 Ojisan言語 v1.0.0 🍺\n");
    printf("オッハー❗😃 おじさんに話しかけてヨ😘（「ジャアネ😘👋」で終了ダヨ）\n");

    gc_init();
    Environment* global = env_new(NULL);
    register_builtins(global);
    gc_set_root(global); 

    for (;;) {
        printf("おじさん😃> ");
        fflush(stdout);
#ifdef _WIN32
        {
            HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
            if (hIn != INVALID_HANDLE_VALUE && _isatty(_fileno(stdin))) {
                wchar_t wbuf[512];
                DWORD read_count = 0;
                if (!ReadConsoleW(hIn, wbuf, 511, &read_count, NULL) || read_count == 0) {
                    printf("\n");
                    break;
                }
                wbuf[read_count] = L'\0';
                if (read_count > 0 && wbuf[read_count - 1] == L'\n') wbuf[--read_count] = L'\0';
                if (read_count > 0 && wbuf[read_count - 1] == L'\r') wbuf[--read_count] = L'\0';
                int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wbuf, (int)read_count, NULL, 0, NULL, NULL);
                if (utf8_len >= (int)sizeof(line)) utf8_len = sizeof(line) - 1;
                WideCharToMultiByte(CP_UTF8, 0, wbuf, (int)read_count, line, utf8_len, NULL, NULL);
                line[utf8_len] = '\0';
            } else {
                if (!fgets(line, sizeof(line), stdin)) { printf("\n"); break; }
                int len = strlen(line);
                if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
            }
        }
#else
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        
        int len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
#endif

        if (strcmp(line, "ジャアネ😘👋") == 0) {
            printf("ジャアネ😘👋 また飲みに行こうヨ🍺🍻\n");
            break;
        }
        
        AstNode* prog = parse_program(line);
        if (prog) {
            evaluate(prog, global);
            ast_free(prog); 
        }
    }
    env_release(global);
}
