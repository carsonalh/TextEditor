#include "texteditor.h"

#include <tchar.h>

#define ERROR_PORTION_LEN ((size_t)64)
#define MESSAGE_LEN ((size_t)1024)

char min_c(char a, char b)
{
    return a < b ? a : b;
}

short min_h(short a, short b)
{
    return a < b ? a : b;
}

int min_i(int a, int b)
{
    return a < b ? a : b;
}

long min_l(long a, long b)
{
    return a < b ? a : b;
}

long long min_ll(long long a, long long b)
{
    return a < b ? a : b;
}

unsigned char min_uc(char a, char b)
{
    return a < b ? a : b;
}

unsigned short min_uh(short a, short b)
{
    return a < b ? a : b;
}

unsigned int min_ui(int a, int b)
{
    return a < b ? a : b;
}

unsigned long min_ul(long a, long b)
{
    return a < b ? a : b;
}

unsigned long long min_ull(long long a, long long b)
{
    return a < b ? a : b;
}

char max_c(char a, char b)
{
    return a > b ? a : b;
}

short max_h(short a, short b)
{
    return a > b ? a : b;
}

int max_i(int a, int b)
{
    return a > b ? a : b;
}

long max_l(long a, long b)
{
    return a > b ? a : b;
}

long long max_ll(long long a, long long b)
{
    return a > b ? a : b;
}

unsigned char max_uc(char a, char b)
{
    return a > b ? a : b;
}

unsigned short max_uh(short a, short b)
{
    return a > b ? a : b;
}

unsigned int max_ui(int a, int b)
{
    return a > b ? a : b;
}

unsigned long max_ul(long a, long b)
{
    return a > b ? a : b;
}

unsigned long long max_ull(long long a, long long b)
{
    return a > b ? a : b;
}

void tstr_to_cstr(char *dest, const TCHAR *source, size_t n)
{
    while (n-- && (*dest++ = (char)*source++))
        ;
}

void tstr_to_wcstr(wchar_t *dest, const TCHAR *source, size_t n)
{
    while (n-- && (*dest++ = (wchar_t)*source++))
        ;
}

void cstr_to_tstr(TCHAR *dest, const char *source, size_t n)
{
    while (n-- && (*dest++ = (TCHAR)*source++))
        ;
}

void wcstr_to_tstr(TCHAR *dest, const wchar_t *source, size_t n)
{
    while (n-- && (*dest++ = (TCHAR)*source++))
        ;
}

bool _Fatal(const TCHAR* where)
{
    DWORD error = GetLastError();

    TCHAR message[MESSAGE_LEN];
    memset(message, 0, sizeof message);

    C_ASSERT(ERROR_PORTION_LEN < MESSAGE_LEN);
    _sntprintf(message, ERROR_PORTION_LEN, TEXT("Received system error code 0x%.4X (%d):\n"), error, error);

    LPTSTR systemMessage;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        0, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&systemMessage, 0, NULL
    );

    _tcsncat(message, systemMessage, MESSAGE_LEN);
    _tcsncat(message, TEXT("\nWould you like to debug?"), MESSAGE_LEN);

    if (NULL == where)
        where = TEXT("Fatal Error Ocurred");

    int chosen = MessageBox(NULL, message, where, MB_YESNO | MB_ICONERROR | MB_SYSTEMMODAL);

    LocalFree(systemMessage);

    return IDYES == chosen;
}
