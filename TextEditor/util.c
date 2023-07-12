#include "texteditor.h"

#include <tchar.h>

#define ERROR_PORTION_LEN ((size_t)64)
#define MESSAGE_LEN ((size_t)1024)

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
