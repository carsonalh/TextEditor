#ifndef TEXT_EDITOR_H
#define TEXT_EDITOR_H

#ifndef UNICODE
#define UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#define FATALW(_where) do { if ( _Fatal(TEXT(_where)) ) DebugBreak(); exit(1); } while (0)
#define FATAL()        do { if ( _Fatal(NULL)         ) DebugBreak(); exit(1); } while (0)

/* Used to crash the program. Will print out result of GetLastError() and give a (somewhat) meaningful message. */
bool _Fatal(const TCHAR*);

#define CONSOLE_WINDOW_CLASS TEXT("ConsoleWindow")

void RegisterConsoleWindowClass();

#endif // TEXT_EDITOR_H