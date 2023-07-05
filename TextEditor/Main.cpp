#ifndef UNICODE
#define UNICODE
#endif

// Without this mess, our app will look like it's from Windows 95
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>

#include <stdio.h>
#include <assert.h>

#include <string>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

bool HandleKeyPress(WPARAM virtualKeyCode, LPARAM otherState);
void AppendUtf16Char(const wchar_t* buffer, size_t length);

std::wstring g_fileContents;

enum ButtonControlIdentifier {
    SAVE_BUTTON,
    OPEN_BUTTON,
};

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    // Register the window class.
    const wchar_t CLASS_NAME[] = L"Sample Window Class";

    WNDCLASS wc = { };

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    INITCOMMONCONTROLSEX commonControls = {};
    commonControls.dwSize = sizeof commonControls;
    commonControls.dwICC = (DWORD)ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&commonControls);

    // Create the window.

    HWND hwnd = CreateWindowEx(
        0,                              // Optional window styles.
        CLASS_NAME,                     // Window class
        L"My jankey text editor",    // Window text
        WS_OVERLAPPEDWINDOW,            // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

        NULL,       // Parent window    
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );

    HWND saveFileButton = CreateWindow(
        L"BUTTON",
        L"Save File...",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        10, 400,
        80, 30,
        hwnd,
        (HMENU)SAVE_BUTTON,
        hInstance,
        NULL
    );

    HWND openFileButton = CreateWindow(
        L"BUTTON",
        L"Open File...",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        100, 400,
        80, 30,
        hwnd,
        (HMENU)OPEN_BUTTON,
        hInstance,
        NULL
    );

    SendMessage(openFileButton, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), true);
    SendMessage(saveFileButton, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), true);

    if (hwnd == NULL)
    {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    // Run the message loop.

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HBRUSH brush = CreateSolidBrush(RGB(0xff, 0x0, 0x0));
        HDC hdc = BeginPaint(hwnd, &ps);

        FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));

        SelectFont(hdc, (HGDIOBJ)GetStockObject(DEFAULT_GUI_FONT));

        {
            RECT rect;

            GetWindowRect(hwnd, &rect);
            int windowWidth = rect.right - rect.left;
            int windowHeight = rect.bottom - rect.top;

            rect.left = rect.top = 0;
            rect.right = windowWidth;
            rect.bottom = windowHeight;

            DrawText(hdc, g_fileContents.c_str(), -1, &rect, DT_TOP | DT_LEFT);
        }

        EndPaint(hwnd, &ps);
        DeleteObject(brush);

        return 0;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED) {
            constexpr size_t FILE_MAX_LEN = 256;
            char selectedFile[FILE_MAX_LEN];
            memset(selectedFile, 0, sizeof selectedFile);

            OPENFILENAMEA ofn;
            memset(&ofn, 0, sizeof ofn);

            ofn.lStructSize = sizeof ofn;
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = "All Files\0*.*\0";
            ofn.lpstrFile = selectedFile;
            ofn.nMaxFile = FILE_MAX_LEN;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (LOWORD(wParam) == SAVE_BUTTON) {
                if (GetSaveFileNameA(&ofn)) {
                    FILE* f;
                    fopen_s(&f, selectedFile, "w");

                    size_t written = fwrite(g_fileContents.c_str(), sizeof g_fileContents[0], g_fileContents.length(), f);
                    assert(written == g_fileContents.length());

                    fclose(f);

                    (void)SetFocus(hwnd);
                }
            }
            else if (LOWORD(wParam) == OPEN_BUTTON) {
                if (GetOpenFileNameA(&ofn)) {
                    FILE* f;
                    fopen_s(&f, selectedFile, "r");

                    size_t sizeBytes;
                    fseek(f, 0, SEEK_END);
                    sizeBytes = ftell(f);
                    fseek(f, 0, SEEK_SET);

                    const size_t newSize = 1 + sizeBytes / 2;

                    g_fileContents.resize(newSize, 0);

                    size_t read = fread(&g_fileContents[0], sizeof g_fileContents[0], g_fileContents.length(), f);
                    assert(read % 2 == 0 && "file must be utf 16");

                    size_t actualLength = wcsnlen(g_fileContents.c_str(), newSize);
                    g_fileContents.resize(actualLength, 0);

                    fclose(f);

                    (void)SetFocus(hwnd);
                    InvalidateRect(hwnd, NULL, true);
                }
            }
            else {
                FatalExit(1);
            }

            return 0;
        }
    }

    case WM_KEYDOWN:
    {
        bool shouldRedraw = HandleKeyPress(wParam, lParam);

        if (shouldRedraw) {
            InvalidateRect(hwnd, NULL, true);
        }

        return 0;
    }

    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

/*Returns true if should redraw the window.
 */
bool HandleKeyPress(WPARAM virtualKeyCode, LPARAM otherState)
{
    constexpr size_t KEYBOARD_STATE_SIZE = 256;
    uint8_t keyboardState[KEYBOARD_STATE_SIZE];

    if (!GetKeyboardState(keyboardState)) {
        DebugBreak();
        abort();
    }

    const int repeatCount = 0xffff & otherState;
    const int scanCode = (0xff0000 & otherState) >> 16;
    const bool isExtended = (0x1000000 & otherState) >> 24;
    const int previousState = (0x80000000 & otherState) >> 31;

    constexpr size_t MAX_UTF16_CHARACTER = 2;
    wchar_t outChar[MAX_UTF16_CHARACTER];

    int value;

    value = ToUnicode(virtualKeyCode, scanCode, keyboardState, outChar, MAX_UTF16_CHARACTER, 0);

    if (0 == value) {
        // no op
        return false;
    }
    else if (value > 0) {
        AppendUtf16Char(outChar, value);
        return true;
    }
    else if (value < 0) {
        return false;
    }
}

void AppendUtf16Char(const wchar_t* buffer, size_t length)
{
    constexpr wchar_t BACKSPACE = L'\x0008';

    if (length == 1 && *buffer == BACKSPACE) {
        if (g_fileContents.empty()) {
            return;
        }

        const wchar_t *end = g_fileContents.c_str() + g_fileContents.size();
        const wchar_t *newEnd = CharPrevW(
            g_fileContents.c_str(),
            g_fileContents.c_str() + g_fileContents.size()
        );

        int removed = end - newEnd;
        int newEndIndex = newEnd - g_fileContents.c_str();

        g_fileContents.erase(newEndIndex, removed);
    }
    else {
        g_fileContents.insert(g_fileContents.size(), buffer, length);
    }
}