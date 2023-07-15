#ifndef TEXT_EDITOR_H
#define TEXT_EDITOR_H

#ifndef UNICODE
#define UNICODE
#endif

#define __STDC__ 1
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <stdbool.h>

#define FATALW(_where) do { if ( _Fatal(TEXT(_where)) ) DebugBreak(); exit(1); } while (0)
#define FATAL()        do { if ( _Fatal(NULL)         ) DebugBreak(); exit(1); } while (0)

#ifdef __cplusplus
#define CFUNCTION extern "C"
#define CPPFUNCTION __stdcall
#else
#define CFUNCTION
#define CPPFUNCTION
#endif // __cplusplus


/* Used to crash the program. Will print out result of GetLastError() and give a (somewhat) meaningful message. */
bool _Fatal(const TCHAR*);

#define CONSOLE_WINDOW_CLASS TEXT("ConsoleWindow")
void register_console_window_class();

#define FILE_TREE_WINDOW_CLASS TEXT("FileTreeWindow")
void register_file_tree_window_class(void);

#define EDITOR_WINDOW_CLASS TEXT("EditorWindow")
void register_editor_window_class(void);

void tstr_to_cstr(char* dest, const TCHAR * source, size_t n);
void tstr_to_wcstr(wchar_t* dest, const TCHAR * source, size_t n);
void cstr_to_tstr(TCHAR * dest, const char* source, size_t n);
void wcstr_to_tstr(TCHAR * dest, const wchar_t* source, size_t n);

#define min(a, b) \
    _Generic(a, \
        char : min_c, \
        short : min_h, \
        int : min_i, \
        long : min_l, \
        long long : min_ll, \
        unsigned char : min_uc, \
        unsigned short : min_uh, \
        unsigned int : min_ui, \
        unsigned long : min_ul, \
        unsigned long long : min_ull \
        )(a, b)

#define max(a, b) \
    _Generic(a, \
        char : max_c, \
        short : max_h, \
        int : max_i, \
        long : max_l, \
        long long : max_ll, \
        unsigned char : max_uc, \
        unsigned short : max_uh, \
        unsigned int : max_ui, \
        unsigned long : max_ul, \
        unsigned long long : max_ull \
        )(a, b)

char min_c(char a, char b);
short min_h(short a, short b);
int min_i(int a, int b);
long min_l(long a, long b);
long long min_ll(long long a, long long b);
unsigned char min_uc(char a, char b);
unsigned short min_uh(short a, short b);
unsigned int min_ui(int a, int b);
unsigned long min_ul(long a, long b);
unsigned long long min_ull(long long a, long long b);

char max_c(char a, char b);
short max_h(short a, short b);
int max_i(int a, int b);
long max_l(long a, long b);
long long max_ll(long long a, long long b);
unsigned char max_uc(char a, char b);
unsigned short max_uh(short a, short b);
unsigned int max_ui(int a, int b);
unsigned long max_ul(long a, long b);
unsigned long long max_ull(long long a, long long b);

#endif // TEXT_EDITOR_H