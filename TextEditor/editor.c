#include "texteditor.h"

#include <windowsx.h>

#define CURSOR_WIDTH_PX 1

static bool handle_key_press(HWND hwnd, WPARAM virtualKeyCode, LPARAM otherState);

/* xy is baseline xy at which to draw the cursor.
 */
static void get_cursor_position(HDC hdc, int cursorIndex, int* x, int* y, int* textHeight);

static DWORD editor_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static HFONT editor_font;

#define MAX_FILE 0x8000
static char file_contents[MAX_FILE] = { 0 };
static size_t file_len = 0;

/*Cursor in between this index of file_contents and the char before it.*/
static int cursor = 0;
static int selection = -1;

void register_editor_window_class(void)
{
	WNDCLASS editor_window_class;
	memset(&editor_window_class, 0, sizeof editor_window_class);

	editor_window_class.lpfnWndProc = editor_window_proc;
	editor_window_class.lpszClassName = EDITOR_WINDOW_CLASS;

	RegisterClass(&editor_window_class);
}

static DWORD editor_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
		break;
	}
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
			get_cursor_position(hdc, cursor, &x, &y, &textHeight);
			RECT cursorRect;

			cursorRect.left = x;
			cursorRect.right = x + CURSOR_WIDTH_PX;
			cursorRect.bottom = y;
			cursorRect.top = y - textHeight;

			FillRect(hdc, &cursorRect, brush);
		}

		if (selection >= 0) {
			HBRUSH selectBrush = CreateSolidBrush(RGB(0, 0, 255));

			int x, y, textHeight;
			get_cursor_position(hdc, selection, &x, &y, &textHeight);
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

		break;
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
			if (cursor <= 0) {
				break;
			}

			char* delete_end, *delete_start;

			if (selection >= 0) {
				delete_start = &file_contents[min(selection, cursor)];
				delete_end = &file_contents[max(selection, cursor)];
			}
			else {
				delete_end = &file_contents[cursor];
				delete_start = CharPrevA(file_contents, &file_contents[cursor]);
			}

			size_t shifted = &file_contents[file_len] - delete_end;

			memmove(delete_start, delete_end, shifted);
			cursor = min(selection, cursor);
			selection = -1;
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
				file_contents[cursor++] = '\n';
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
			file_contents[cursor++] = (char)(wchar_t)wParam;
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
static bool handle_key_press(HWND hwnd, WPARAM virtualKeyCode, LPARAM otherState)
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
			if (selection < 0) {
				selection = max(0, cursor - 1);
			}
			else {
				selection = max(0, selection - 1);
			}
		}
		else {
			if (selection < 0) {
				cursor = max(0, cursor - 1);
			}
			else {
				cursor = min(cursor, selection);
				selection = -1;
			}
		}
		return true;
	}
	case VK_RIGHT:
	{
		bool shiftPressed = GetKeyState(VK_SHIFT) & KEYDOWN_MASK;
		if (shiftPressed) {
			if (selection < 0) {
				selection = min(cursor + 1, (int)file_len);
			}
			else {
				selection = min(selection + 1, (int)file_len);
			}
		}
		else {
			if (selection < 0) {
				cursor = min(cursor + 1, (int)file_len);
			}
			else {
				cursor = max(cursor, selection);
				selection = -1;
			}
		}
		return true;
	}
	case VK_C:
	{
		bool controlPressed = GetKeyState(VK_CONTROL) & KEYDOWN_MASK;
		if (controlPressed) {
			// Ctrl+C: copy!
			if (selection < 0) {
				return false; // without selection, copy does nothing
			}

			int selectionStart = min(cursor, selection);
			int selectionEnd = max(cursor, selection);

			HGLOBAL global = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, selectionEnd - selectionStart + 1);
			char* buffer = (char*)GlobalLock(global);
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
				const char* text = (const char*)GlobalLock(hGlobal);

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

static void get_cursor_position(HDC hdc, int cursorIndex, int* x, int* y, int* textHeight)
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


