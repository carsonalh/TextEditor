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

#define FILETREE_WIDTH_PX 300
#define CONSOLE_HEIGHT_PX 300

#define MASTER_WINDOW_CLASS TEXT("MasterWindow")

LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

enum ButtonControlIdentifier {
    SAVE_BUTTON,
    OPEN_BUTTON,
};

static HWND editor_window, file_tree_window, console_window;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    // Register the window class.
    WNDCLASS wc = { 0 };

    //wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = hInstance;
    wc.lpszClassName = MASTER_WINDOW_CLASS;

    RegisterClass(&wc);

    register_editor_window_class();
    register_console_window_class();
    register_file_tree_window_class();

    INITCOMMONCONTROLSEX commonControls = { 0 };
    commonControls.dwSize = sizeof commonControls;
    commonControls.dwICC = (DWORD)ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&commonControls);

    // Create the window.

    HWND master_window = CreateWindow(
        MASTER_WINDOW_CLASS,     // Window class
        TEXT("Text Editor.exe"), // Window text
        WS_OVERLAPPEDWINDOW,     // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

        NULL,       // Parent window    
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );

    if (NULL == master_window)
        FATALW("Create master window");

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
        RECT client;
        GetClientRect(hwnd, &client);

        const unsigned client_width_px = client.right - client.left;
        const unsigned client_height_px = client.bottom - client.top;

        file_tree_window = CreateWindow(
            FILE_TREE_WINDOW_CLASS,
            NULL,
            WS_VISIBLE | WS_CHILD,
            client.left, client.top, // x y
            FILETREE_WIDTH_PX, client_height_px, // width height
            hwnd,
            NULL,
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL
        );

        editor_window = CreateWindow(
            EDITOR_WINDOW_CLASS,
            NULL,
            WS_CHILD | WS_VISIBLE,
            FILETREE_WIDTH_PX, client.top, // x y
            client_width_px - FILETREE_WIDTH_PX, client_height_px - CONSOLE_HEIGHT_PX, // width height
            hwnd,
            NULL,
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL
        );

        console_window = CreateWindow(
            CONSOLE_WINDOW_CLASS,
            NULL,
            WS_VISIBLE | WS_CHILD,
            FILETREE_WIDTH_PX, client_height_px - CONSOLE_HEIGHT_PX, // x y
            client_width_px - FILETREE_WIDTH_PX, CONSOLE_HEIGHT_PX, // width height
            hwnd,
            NULL,
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL
        );

        //HWND save_file_button = CreateWindow(
        //    TEXT("BUTTON"),
        //    TEXT("Save File..."),
        //    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        //    10, 400,
        //    80, 30,
        //    hwnd,
        //    (HMENU)SAVE_BUTTON,
        //    (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        //    NULL
        //);

        //HWND open_file_button = CreateWindow(
        //    L"BUTTON",
        //    L"Open File...",
        //    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        //    100, 400,
        //    80, 30,
        //    hwnd,
        //    (HMENU)OPEN_BUTTON,
        //    (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        //    NULL
        //);

        //SendMessage(open_file_button, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), true);
        //SendMessage(save_file_button, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), true);

        break;
    }
    case WM_SIZE:
    {
        RECT client;
        GetClientRect(hwnd, &client);

        const unsigned client_width_px = client.right - client.left;
        const unsigned client_height_px = client.bottom - client.top;

        MoveWindow(file_tree_window,
            client.left, client.top, // x y
            FILETREE_WIDTH_PX, client_height_px, // width height
            true /* repaint */);

        MoveWindow(editor_window,
            FILETREE_WIDTH_PX, client.top, // x y
            client_width_px - FILETREE_WIDTH_PX, client_height_px - CONSOLE_HEIGHT_PX, // width height
            true /* repaint */);

        MoveWindow(console_window,
            FILETREE_WIDTH_PX, client_height_px - CONSOLE_HEIGHT_PX, // x y
            client_width_px - FILETREE_WIDTH_PX, CONSOLE_HEIGHT_PX, // width height
            true /* repaint */);

        ShowWindow(file_tree_window, SW_SHOW);
        ShowWindow(editor_window, SW_SHOW);
        ShowWindow(console_window, SW_SHOW);

        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_COMMAND:
    {
        /*if (HIWORD(wParam) == BN_CLICKED) {
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
        }*/
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}
