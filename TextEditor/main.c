// Without this mess, our app will look like it's from Windows 95
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include "texteditor.h"

#include <winbase.h>
#include <winuser.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <tchar.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define CURSOR_WIDTH_PX 1

LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

bool handle_key_press(HWND hwnd, WPARAM virtualKeyCode, LPARAM otherState);

/* xy is baseline xy at which to draw the cursor.
 */
void get_cursor_position(HDC hdc, int cursorIndex, int* x, int* y, int* textHeight);

#define MAX_FILE 0x8000
char file_contents[MAX_FILE] = { 0 };
size_t file_len = 0;

/*Cursor in between this index of g_fileContents and the char before it.*/
int g_cursor = 0;
int g_selection = -1;

HFONT editor_font;

enum ButtonControlIdentifier {
    SAVE_BUTTON,
    OPEN_BUTTON,
};

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    // Register the window class.
    WNDCLASS wc = { 0 };

    wc.lpfnWndProc = window_proc;
    wc.hInstance = hInstance;
    wc.lpszClassName = TEXT("MainWindow");

    RegisterClass(&wc);

    register_console_window_class();
    register_file_tree_window_class();

    INITCOMMONCONTROLSEX commonControls = { 0 };
    commonControls.dwSize = sizeof commonControls;
    commonControls.dwICC = (DWORD)ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&commonControls);

    // Create the window.

    HWND master_window = CreateWindow(
        TEXT("MainWindow"),                     // Window class
        L"My jankey text editor",    // Window text
        WS_OVERLAPPEDWINDOW,            // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

        NULL,       // Parent window    
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );

    ShowWindow(master_window, nCmdShow);

    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        editor_font = CreateFont(
            0, 0, // height, width
            0, 0, // escapement, orientation
            FW_DONTCARE,
            false, false, false, // italic, underline, strikeout
            ANSI_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY,
            FF_DONTCARE,
            L"Consolas"
        );

        HWND save_file_button = CreateWindow(
            TEXT("BUTTON"),
            TEXT("Save File..."),
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            10, 400,
            80, 30,
            hwnd,
            (HMENU)SAVE_BUTTON,
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL
        );

        HWND open_file_button = CreateWindow(
            L"BUTTON",
            L"Open File...",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            100, 400,
            80, 30,
            hwnd,
            (HMENU)OPEN_BUTTON,
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL
        );

        HWND _console_window = CreateWindow(
            CONSOLE_WINDOW_CLASS,
            TEXT("Console Window"),
            WS_VISIBLE | WS_CHILD,
            0, 430,
            800, 300,
            hwnd,
            NULL,
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL
        );

        HWND _file_tree_window = CreateWindow(
            FILE_TREE_WINDOW_CLASS,
            TEXT("File tree window"),
            WS_VISIBLE | WS_CHILD,
            0, 0, // x y
            800, 600, // width height
            hwnd,
            NULL,
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL
        );

        SendMessage(open_file_button, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), true);
        SendMessage(save_file_button, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), true);

        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
    {

        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        HBRUSH brush = CreateSolidBrush(RGB(255, 0x0, 0x0));

        FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));

        { // draw text
            RECT rect;

            GetWindowRect(hwnd, &rect);
            int windowWidth = rect.right - rect.left;
            int windowHeight = rect.bottom - rect.top;

            rect.left = rect.top = 0;
            rect.right = windowWidth;
            rect.bottom = windowHeight;

            SelectFont(hdc, editor_font);

            DrawTextA(hdc, file_contents, file_len, &rect, DT_TOP | DT_LEFT);
        }

        { // draw cursor
            int x, y, textHeight;
            get_cursor_position(hdc, g_cursor, &x, &y, &textHeight);
            RECT cursorRect;

            cursorRect.left = x;
            cursorRect.right = x + CURSOR_WIDTH_PX;
            cursorRect.bottom = y;
            cursorRect.top = y - textHeight;

            FillRect(hdc, &cursorRect, brush);
        }

        if (g_selection >= 0) {
            HBRUSH selectBrush = CreateSolidBrush(RGB(0, 0, 255));

            int x, y, textHeight;
            get_cursor_position(hdc, g_selection, &x, &y, &textHeight);
            RECT selectionRect;

            selectionRect.left = x;
            selectionRect.right = x + CURSOR_WIDTH_PX;
            selectionRect.bottom = y;
            selectionRect.top = y - textHeight;

            FillRect(hdc, &selectionRect, selectBrush);
            DeleteObject(selectBrush);
        }

        DeleteObject(brush);
        EndPaint(hwnd, &ps);

        return 0;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED) {
            char selectedFile[MAX_PATH];
            memset(selectedFile, 0, sizeof selectedFile);

            OPENFILENAMEA ofn;
            memset(&ofn, 0, sizeof ofn);

            ofn.lStructSize = sizeof ofn;
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = "All Files\0*.*\0";
            ofn.lpstrFile = selectedFile;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (LOWORD(wParam) == SAVE_BUTTON) {
                if (GetSaveFileNameA(&ofn)) {
                    FILE *f = fopen(selectedFile, "w");

                    size_t written = fwrite(file_contents, sizeof file_contents[0], file_len, f);
                    assert(written == file_len);

                    fclose(f);

                    (void)SetFocus(hwnd);
                }
            }
            else if (LOWORD(wParam) == OPEN_BUTTON) {
                if (GetOpenFileNameA(&ofn)) {
                    FILE* f;
                    fopen_s(&f, selectedFile, "r");

                    size_t size_bytes;
                    fseek(f, 0, SEEK_END);
                    size_bytes = ftell(f);
                    fseek(f, 0, SEEK_SET);

                    assert(size_bytes <= MAX_FILE);

                    size_t read = fread(file_contents, sizeof file_contents[0], MAX_FILE, f);

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
        bool shouldRedraw = handle_key_press(hwnd, wParam, lParam);

        if (shouldRedraw) {
            InvalidateRect(hwnd, NULL, true);
        }

        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        SetFocus(hwnd);
        break;
    }

    case WM_CHAR:
    {
        #define ESCAPE 0x1B
        switch (wParam)
        {
        case '\b':
        {
            if (g_cursor <= 0) {
                break;
            }

            char *delete_end, *delete_start;

            if (g_selection >= 0) {
                delete_start = &file_contents[min(g_selection, g_cursor)];
                delete_end = &file_contents[max(g_selection, g_cursor)];
            }
            else {
                delete_end = &file_contents[g_cursor];
                delete_start = CharPrevA(file_contents, &file_contents[g_cursor]);
            }

            size_t shifted = &file_contents[file_len] - delete_end;

            memmove(delete_start, delete_end, shifted);
            g_cursor = min(g_selection, g_cursor);
            g_selection = -1;
            file_len -= delete_end - delete_start;
        }
        case '\t':
            // handle a tab insertion
            break;
        case '\r':
        case '\n':
        {
            if (file_len < MAX_FILE) {
                file_len++;
                file_contents[g_cursor++] = '\n';
            }
            break;
        }
        case ESCAPE:
            break;
        default:
        {
            if (!iswprint((wchar_t)wParam)) {
                break;
            }
            // TODO utf16 to utf8 conversion
            file_contents[g_cursor++] = (char)(wchar_t)wParam;
            file_len++;
            break;
        }
        }
        InvalidateRect(hwnd, NULL, true);
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}

/*Returns true if should redraw the window.
 */
bool handle_key_press(HWND hwnd, WPARAM virtualKeyCode, LPARAM otherState)
{
    #define KEYDOWN_MASK ((short)0x8000)
    #define VK_A ((int)0x41)
    #define VK_C ((int)(('c' - 'a') + VK_A))
    #define VK_V ((int)(('v' - 'a') + VK_A))

    const int repeatCount = 0xffff & otherState;
    const int scanCode = (0xff0000 & otherState) >> 16;
    const bool isExtended = (0x1000000 & otherState) >> 24;
    const int previousState = (0x80000000 & otherState) >> 31;

    switch (virtualKeyCode) {
    case VK_LEFT:
    {
        bool shiftPressed = GetKeyState(VK_SHIFT) & KEYDOWN_MASK;
        if (shiftPressed) {
            if (g_selection < 0) {
                g_selection = max(0, g_cursor - 1);
            }
            else {
                g_selection = max(0, g_selection - 1);
            }
        }
        else {
            if (g_selection < 0) {
                g_cursor = max(0, g_cursor - 1);
            }
            else {
                g_cursor = min(g_cursor, g_selection);
                g_selection = -1;
            }
        }
        return true;
    }
    case VK_RIGHT:
    {
        bool shiftPressed = GetKeyState(VK_SHIFT) & KEYDOWN_MASK;
        if (shiftPressed) {
            if (g_selection < 0) {
                g_selection = min(g_cursor + 1, (int)file_len);
            }
            else {
                g_selection = min(g_selection + 1, (int)file_len);
            }
        }
        else {
            if (g_selection < 0) {
                g_cursor = min(g_cursor + 1, (int)file_len);
            }
            else {
                g_cursor = max(g_cursor, g_selection);
                g_selection = -1;
            }
        }
        return true;
    }
    case VK_C:
    {
        bool controlPressed = GetKeyState(VK_CONTROL) & KEYDOWN_MASK;
        if (controlPressed) {
            // Ctrl+C: copy!
            if (g_selection < 0) {
                return false; // without selection, copy does nothing
            }

            int selectionStart = min(g_cursor, g_selection);
            int selectionEnd = max(g_cursor, g_selection);

            HGLOBAL global = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, selectionEnd - selectionStart + 1);
            char* buffer = (char *)GlobalLock(global);
            strncpy(buffer, &file_contents[selectionStart], selectionEnd - selectionStart);
            GlobalUnlock(global);

            OpenClipboard(hwnd);
            EmptyClipboard();
            SetClipboardData(CF_TEXT, global);
            CloseClipboard();

            return false;
        }
    }
    case VK_V:
    {
        bool controlPressed = GetKeyState(VK_CONTROL) & KEYDOWN_MASK;
        if (controlPressed) {
            // Ctrl+V: paste!

            OpenClipboard(hwnd);
            if (IsClipboardFormatAvailable(CF_TEXT)) {
                HGLOBAL hGlobal = GetClipboardData(CF_TEXT);
                const char *text = (const char *)GlobalLock(hGlobal);

                if (NULL != text) {
                    // paste (not implemented)

                    GlobalUnlock(hGlobal);
                }
            }
            CloseClipboard();
        }
    }
    }

    return false;
}

void get_cursor_position(HDC hdc, int cursorIndex, int* x, int* y, int *textHeight)
{
    const wchar_t* full_block = L"\x2588";

    SIZE size;

    if (!GetTextExtentPoint32(
            hdc,
            full_block,
            wcslen(full_block),
            &size)) {
        DebugBreak();
        abort();
    }

    // TODO test this following part of the function

    int cursor_line_idx = 0;
    int line_start = 0;

    for (int i = 0; i <= file_len; i++) {
        if (i > 0 && file_contents[i - 1] == L'\n') {
            line_start = i;
            cursor_line_idx++;
        }

        if (i == cursorIndex)
            break;
    }

    int cursorColumnIdx = cursorIndex - line_start;

    *x = cursorColumnIdx * size.cx;
    *y = (cursor_line_idx + 1) * size.cy;
    *textHeight = size.cy;
}
