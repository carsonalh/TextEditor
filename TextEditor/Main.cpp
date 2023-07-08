#ifndef UNICODE
#define UNICODE
#endif

// Without this mess, our app will look like it's from Windows 95
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winbase.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <tchar.h>

#include <stdio.h>
#include <assert.h>

#include <string>
#include <utility>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

bool HandleKeyPress(HWND hwnd, WPARAM virtualKeyCode, LPARAM otherState);
void AppendUtf16Char(const wchar_t* buffer, size_t length);

/* xy is baseline xy at which to draw the cursor.
 */
void GetCursorPosition(HDC hdc, int cursorIndex, int* x, int* y, int* textHeight);

#define FATALW(_where) do { if ( _Fatal(TEXT(_where)) ) DebugBreak(); exit(1); } while (0)
#define FATAL()        do { if ( _Fatal(NULL)         ) DebugBreak(); exit(1); } while (0)

/* Used to crash the program. Will print out result of GetLastError() and give a (somewhat) meaningful message. */
bool _Fatal(const TCHAR *);

std::string g_fileContents;
/*Cursor in between this index of g_fileContents and the char before it.*/
int g_cursor = 0;
int g_selection = -1;

// Pipe ends to communicate with cmd.exe
HANDLE g_stdinWrite, g_stdoutRead, g_stderrRead;

HWND g_mainWindow;

constexpr size_t CONSOLE_OUTPUT_BUFFER_SIZE = 1024;
char g_consoleOutput[CONSOLE_OUTPUT_BUFFER_SIZE];

HFONT g_editorFont;

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

    g_mainWindow = CreateWindowEx(
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
        g_mainWindow,
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
        g_mainWindow,
        (HMENU)OPEN_BUTTON,
        hInstance,
        NULL
    );

    g_editorFont = CreateFont(
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

    SendMessage(openFileButton, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), true);
    SendMessage(saveFileButton, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), true);

    HANDLE stdinRead;
    HANDLE stdoutWrite;
    HANDLE stderrWrite;

    int couldCreate = -1;

    constexpr size_t PIPE_SIZE = 1024;

    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof sa);

    sa.nLength = sizeof sa;
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = true;

    if (!CreatePipe(&stdinRead, &g_stdinWrite, &sa, PIPE_SIZE)) FATALW("Pipe stdin");
    if (!CreatePipe(&g_stdoutRead, &stdoutWrite, &sa, PIPE_SIZE)) FATALW("Pipe stdout");
    if (!CreatePipe(&g_stderrRead, &stderrWrite, &sa, PIPE_SIZE)) FATALW("Pipe stderr");

    STARTUPINFO si;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = stdinRead;
    si.hStdOutput = stdoutWrite;
    si.hStdError = stderrWrite;

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof pi);

    if (!CreateProcess(
            TEXT("C:\\Windows\\System32\\cmd.exe"), NULL,
            NULL, NULL, // process, thread attributes
            true, // inherit handles
            CREATE_NO_WINDOW, // creation flags
            NULL, // string of environment variables
            NULL, // working directory, NULL means inherit from this process
            &si, // [in] startup info
            &pi // [out] process info
            ))
        FATALW("Create cmd.exe");

    // our code is too fast even for cmd.exe

    memset(g_consoleOutput, 0, CONSOLE_OUTPUT_BUFFER_SIZE);
    int bytesRead;
    // just get the console prompt to show to the screen for now
    if (!ReadFile(g_stdoutRead, (void*)g_consoleOutput, CONSOLE_OUTPUT_BUFFER_SIZE, (LPDWORD)&bytesRead, NULL)) {
        FATALW("Read from subprocess cmd.exe");
    }

    ShowWindow(g_mainWindow, nCmdShow);

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
        HDC hdc = BeginPaint(hwnd, &ps);

        // we'll try reading from the cmd.exe stdout buffer
        RECT rect;
        GetClientRect(hwnd, &rect);
        SelectFont(hdc, g_editorFont);
        DrawTextA(hdc, g_consoleOutput, -1, &rect, DT_TOP | DT_LEFT);

        EndPaint(hwnd, &ps);

        //PAINTSTRUCT ps;
        //HDC hdc = BeginPaint(hwnd, &ps);
        //HBRUSH brush = CreateSolidBrush(RGB(255, 0x0, 0x0));

        //FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));

        //{ // draw text
        //    RECT rect;

        //    GetWindowRect(hwnd, &rect);
        //    int windowWidth = rect.right - rect.left;
        //    int windowHeight = rect.bottom - rect.top;

        //    rect.left = rect.top = 0;
        //    rect.right = windowWidth;
        //    rect.bottom = windowHeight;

        //    SelectFont(hdc, g_editorFont);

        //    DrawTextA(hdc, g_fileContents.c_str(), -1, &rect, DT_TOP | DT_LEFT);
        //}

        //{ // draw cursor
        //    int x, y, textHeight;
        //    GetCursorPosition(hdc, g_cursor, &x, &y, &textHeight);
        //    constexpr int CURSOR_WIDTH_PX = 2;
        //    RECT cursorRect;

        //    cursorRect.left = x;
        //    cursorRect.right = x + CURSOR_WIDTH_PX;
        //    cursorRect.bottom = y;
        //    cursorRect.top = y - textHeight;

        //    FillRect(hdc, &cursorRect, brush);
        //}

        //if (g_selection >= 0) {
        //    HBRUSH selectBrush = CreateSolidBrush(RGB(0, 0, 255));

        //    int x, y, textHeight;
        //    GetCursorPosition(hdc, g_selection, &x, &y, &textHeight);
        //    constexpr int CURSOR_WIDTH_PX = 2;
        //    RECT selectionRect;

        //    selectionRect.left = x;
        //    selectionRect.right = x + CURSOR_WIDTH_PX;
        //    selectionRect.bottom = y;
        //    selectionRect.top = y - textHeight;

        //    FillRect(hdc, &selectionRect, selectBrush);
        //    DeleteObject(selectBrush);
        //}

        //DeleteObject(brush);
        //EndPaint(hwnd, &ps);

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
                    FILE *f = fopen(selectedFile, "w");

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

                    g_fileContents.resize(sizeBytes, 0);

                    size_t read = fread(&g_fileContents[0], sizeof g_fileContents[0], g_fileContents.length(), f);

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
        bool shouldRedraw = HandleKeyPress(hwnd, wParam, lParam);

        if (shouldRedraw) {
            InvalidateRect(hwnd, NULL, true);
        }

        return 0;
    }

    case WM_CHAR:
    {
        constexpr char ESCAPE = 0x1B;
        switch (wParam)
        {
        case '\b':
        {
            if (g_cursor <= 0) {
                break;
            }

            const char* deleteEnd = g_fileContents.c_str() + g_cursor;
            const char* deleteStart = CharPrevA(
                g_fileContents.c_str(),
                g_fileContents.c_str() + g_cursor
            );

            int removed = deleteEnd - deleteStart;
            int deleteStartIdx = deleteStart - g_fileContents.c_str();

            g_fileContents.erase(deleteStartIdx, removed);
            g_cursor--;
        }
        case '\t':
            // handle a tab insertion
            break;
        case '\r':
        case '\n':
        {
            // insert a new line
            const char lf = '\n';
            g_fileContents.insert(g_cursor, &lf, 1);
            g_cursor++;
            break;
        }
        case ESCAPE:
            break;
        default:
        {
            if (!iswprint((wchar_t)wParam)) {
                break;
            }
            g_fileContents.insert(g_cursor, (const char *)&wParam, 1);
            g_cursor++;
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
bool HandleKeyPress(HWND hwnd, WPARAM virtualKeyCode, LPARAM otherState)
{
    constexpr short KEYDOWN_MASK = 0x8000;

    constexpr int VK_A = 0x41;
    constexpr int VK_C = ('c' - 'a') + VK_A;
    constexpr int VK_V = ('v' - 'a') + VK_A;

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
                g_selection = std::max(0, g_cursor - 1);
            }
            else {
                g_selection = std::max(0, g_selection - 1);
            }
        }
        else {
            if (g_selection < 0) {
                g_cursor = std::max(0, g_cursor - 1);
            }
            else {
                g_cursor = std::min(g_cursor, g_selection);
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
                g_selection = std::min(g_cursor + 1, (int)g_fileContents.length());
            }
            else {
                g_selection = std::min(g_selection + 1, (int)g_fileContents.length());
            }
        }
        else {
            if (g_selection < 0) {
                g_cursor = std::min(g_cursor + 1, (int)g_fileContents.length());
            }
            else {
                g_cursor = std::max(g_cursor, g_selection);
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

            int selectionStart = std::min(g_cursor, g_selection);
            int selectionEnd = std::max(g_cursor, g_selection);

            HGLOBAL global = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, selectionEnd - selectionStart + 1);
            char* buffer = (char *)GlobalLock(global);
            strncpy(buffer, &g_fileContents[selectionStart], selectionEnd - selectionStart);
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
                    // Now we paste

                    if (g_selection >= 0) {
                        int selectionStart = std::min(g_cursor, g_selection);
                        int selectionEnd = std::max(g_cursor, g_selection);

                        g_fileContents.replace(
                            selectionStart, selectionEnd - selectionStart,
                            text, strlen(text));

                        g_cursor = std::max(g_cursor, g_selection);
                        g_selection = -1;
                    }
                    else {
                        g_fileContents.insert(g_cursor, text, strlen(text));
                        g_cursor += strlen(text);
                    }

                    GlobalUnlock(hGlobal);
                }
            }
            CloseClipboard();
        }
    }
    }

    return false;
}

void GetCursorPosition(HDC hdc, int cursorIndex, int* x, int* y, int *textHeight)
{
    const wchar_t* fullBlock = L"\x2588";

    SIZE size;

    if (!GetTextExtentPoint32(
            hdc,
            fullBlock,
            wcslen(fullBlock),
            &size)) {
        DebugBreak();
        abort();
    }

    // TODO test this following part of the function

    int cursorLineIdx = 0;
    int lineStart = 0;

    for (int i = 0; i <= g_fileContents.length(); i++) {
        if (i > 0 && g_fileContents[i - 1] == L'\n') {
            lineStart = i;
            cursorLineIdx++;
        }

        if (i == cursorIndex)
            break;
    }

    int cursorColumnIdx = cursorIndex - lineStart;

    *x = cursorColumnIdx * size.cx;
    *y = (cursorLineIdx + 1) * size.cy;
    *textHeight = size.cy;
}

bool _Fatal(const TCHAR *where)
{
    DWORD error = GetLastError();

    constexpr size_t MESSAGE_LEN = 1024;
    TCHAR message[MESSAGE_LEN];
    memset(message, 0, sizeof message);

    constexpr size_t ERROR_PORTION_LEN = 64;
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

    int chosen = MessageBox(g_mainWindow, message, where, MB_YESNO | MB_ICONERROR | MB_SYSTEMMODAL);

    LocalFree(systemMessage);

    return IDYES == chosen;
}