#ifndef TEXT_EDITOR_H
#define TEXT_EDITOR_H

#ifndef UNICODE
#define UNICODE
#endif

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
CFUNCTION bool _Fatal(const TCHAR*);

#define CONSOLE_WINDOW_CLASS TEXT("ConsoleWindow")

CFUNCTION void register_console_window_class();

#define FILE_TREE_WINDOW_CLASS TEXT("FileTreeWindow")

CFUNCTION void register_file_tree_window_class(void);

CFUNCTION void tstr_to_cstr(char* dest, const TCHAR * source, size_t n);
CFUNCTION void tstr_to_wcstr(wchar_t* dest, const TCHAR * source, size_t n);
CFUNCTION void cstr_to_tstr(TCHAR * dest, const char* source, size_t n);
CFUNCTION void wcstr_to_tstr(TCHAR * dest, const wchar_t* source, size_t n);

#endif // TEXT_EDITOR_H