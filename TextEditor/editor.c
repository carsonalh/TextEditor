#include "texteditor.h"

#include <windowsx.h>

#include <assert.h>

#define MAX_FILE ((size_t)0x8000)
#define CURSOR_WIDTH_PX 1

static bool handle_key_press(HWND hwnd, WPARAM virtualKeyCode, LPARAM otherState);

/* xy is baseline xy at which to draw the cursor.
 */
static void get_cursor_position(HDC hdc, int cursorIndex, int* x, int* y, int* textHeight);

static DWORD editor_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void remove_buffer_portion(int start, int end);
static void char_size_px(HDC hdc, int *width, int *height);
static int mouse_coords_to_buffer_idx(POINT point);

static HFONT editor_font;

static struct file_buffer {
	/* since the cursor can be at the start or end of a selection, we say that if there is a
	   selection, it begins at min(selection, cursor) and ends at max(selection, cursor) */

	int cursor;
	int cursor_colidx; // which column the cursor "wants" to be on when jumping up/down
	int selection; // negative value denotes that no text is selected
	size_t length;
	char contents[MAX_FILE];
} file_buf = { .selection = -1 };

static struct mouse_state {
	bool dragging;
} mouse = { 0 };

static int char_width_px = -1, char_height_px = -1;

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

			DrawTextA(hdc, file_buf.contents, file_buf.length, &rect, DT_TOP | DT_LEFT);
		}

		{ // draw cursor
			int x, y, textHeight;
			get_cursor_position(hdc, file_buf.cursor, &x, &y, &textHeight);
			RECT cursorRect;

			cursorRect.left = x;
			cursorRect.right = x + CURSOR_WIDTH_PX;
			cursorRect.bottom = y;
			cursorRect.top = y - textHeight;

			FillRect(hdc, &cursorRect, brush);
		}

		if (file_buf.selection >= 0) {
			HBRUSH selectBrush = CreateSolidBrush(RGB(0, 0, 255));

			int x, y, textHeight;
			get_cursor_position(hdc, file_buf.selection, &x, &y, &textHeight);
			RECT selectionRect;

			selectionRect.left = x;
			selectionRect.right = x + CURSOR_WIDTH_PX;
			selectionRect.bottom = y;
			selectionRect.top = y - textHeight;

			FillRect(hdc, &selectionRect, selectBrush);
			DeleteObject(selectBrush);
		}

		// cache the character width and height for future operations (mouse related)
		if (char_width_px < 0 || char_height_px < 0)
			char_size_px(hdc, &char_width_px, &char_height_px);

		DeleteObject(brush);
		EndPaint(hwnd, &ps);

		break;
	}
	case WM_KEYDOWN:
	{
		bool shouldRedraw = handle_key_press(hwnd, wParam, lParam);

		if (shouldRedraw) {
			InvalidateRect(hwnd, NULL, false);
		}

		return 0;
	}
	case WM_LBUTTONDOWN:
	{
		SetFocus(hwnd);


		int buffer_idx = mouse_coords_to_buffer_idx((POINT) { .x = LOWORD(lParam), .y = HIWORD(lParam) });

		if (DragDetect(hwnd, (POINT) { .x = LOWORD(lParam), .y = HIWORD(lParam) })) {
			// start a text highlight
			mouse.dragging = true;
			SetCapture(hwnd);
		}
		else {
			mouse.dragging = false;
			file_buf.selection = -1;
			file_buf.cursor_colidx = -1;
		}

		file_buf.cursor = buffer_idx;

		InvalidateRect(hwnd, NULL, false);

		break;
	}
	case WM_LBUTTONUP:
	{
		mouse.dragging = false;
		ReleaseCapture(hwnd);
		// end a text highlight
		break;
	}
	case WM_MOUSEMOVE:
	{
		if (mouse.dragging) {
			int buffer_idx = mouse_coords_to_buffer_idx((POINT) { .x = LOWORD(lParam), .y = HIWORD(lParam) });
			file_buf.selection = file_buf.cursor == buffer_idx ? -1 : buffer_idx;
		}
		InvalidateRect(hwnd, NULL, false);
		break;
	}
	case WM_ERASEBKGND:
		return 1;
	case WM_CHAR:
	{
		#define ESCAPE 0x1B
		switch (wParam)
		{
		case '\b':
		{
			int delete_start, delete_end;

			if (file_buf.selection >= 0) {
				delete_start = min(file_buf.selection, file_buf.cursor);
				delete_end = max(file_buf.selection, file_buf.cursor);
			}
			else {
				delete_end = file_buf.cursor;
				// CharPrev stops us from going behind the start of the buffer
				delete_start = CharPrevA(file_buf.contents, &file_buf.contents[file_buf.cursor]) - file_buf.contents;
			}
			
			remove_buffer_portion(delete_start, delete_end);
			file_buf.cursor = delete_start;
			file_buf.selection = -1;
			file_buf.length -= delete_end - delete_start;
		}
		case '\t':
			// handle a tab insertion
			break;
		case ESCAPE:
			break;
		case '\r':
			wParam = (WPARAM)'\n';
			// no break on purpose
		default:
		{
			if (wParam != '\n' && !isprint((char)wParam)) {
				break;
			}

			if (file_buf.selection >= 0) {
				int delete_start = min(file_buf.selection, file_buf.cursor);
				int delete_end = max(file_buf.selection, file_buf.cursor);
				file_buf.contents[delete_start] = (char)wParam;
				// any selection will contain at least one character so this is safe
				remove_buffer_portion(delete_start + 1, delete_end);
				file_buf.selection = -1;
				file_buf.cursor = delete_start + 1;
				file_buf.length -= delete_end - (delete_start + 1);
				break;
			}

			if (file_buf.length < MAX_FILE) {
				memmove(
					&file_buf.contents[file_buf.cursor + 1],
					&file_buf.contents[file_buf.cursor],
					file_buf.length - file_buf.cursor);
				file_buf.contents[file_buf.cursor++] = (char)wParam;
				file_buf.length++;
				break;
			}

			FATALW("Could not insert text into the file");
		}
		}
		InvalidateRect(hwnd, NULL, false);
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

	bool shift_pressed = GetKeyState(VK_SHIFT) & KEYDOWN_MASK;

	switch (virtualKeyCode) {
	case VK_LEFT:
	{
		file_buf.cursor_colidx = -1;
		if (shift_pressed) {
			if (file_buf.selection < 0) {
				file_buf.selection = max(0, file_buf.cursor - 1);
			}
			else {
				file_buf.selection = max(0, file_buf.selection - 1);
			}
		}
		else {
			if (file_buf.selection < 0) {
				file_buf.cursor = max(0, file_buf.cursor - 1);
			}
			else {
				file_buf.cursor = min(file_buf.cursor, file_buf.selection);
				file_buf.selection = -1;
			}
		}
		return true;
	}
	case VK_RIGHT:
	{
		file_buf.cursor_colidx = -1;
		if (shift_pressed) {
			if (file_buf.selection < 0) {
				file_buf.selection = min(file_buf.cursor + 1, (int)file_buf.length);
			}
			else {
				file_buf.selection = min(file_buf.selection + 1, (int)file_buf.length);
			}
		}
		else {
			if (file_buf.selection < 0) {
				file_buf.cursor = min(file_buf.cursor + 1, (int)file_buf.length);
			}
			else {
				file_buf.cursor = max(file_buf.cursor, file_buf.selection);
				file_buf.selection = -1;
			}
		}
		return true;
	}
	case VK_UP:
	{
		int i;
		int current_column = 0;

		for (i = file_buf.cursor; i > 0; current_column++, i--)
			if (file_buf.contents[i - 1] == '\n')
				break;

		if (file_buf.cursor_colidx < 0)
			file_buf.cursor_colidx = current_column;

		// i is at the beginning of the current line

		const bool on_first_line = i == 0;

		if (on_first_line) {
			file_buf.cursor = 0;
			file_buf.cursor_colidx = -1;
		}
		else {
			i--;

			for (; i > 0; i--) {
				if (file_buf.contents[i - 1] == '\n')
					break;
			}

			const int prevline_begin = i;

			for (; i < file_buf.length && i - prevline_begin < file_buf.cursor_colidx; i++)
				if (file_buf.contents[i] == '\n')
					break;

			file_buf.cursor = i;
		}

		InvalidateRect(hwnd, NULL, false);

		break;
	}
	case VK_DOWN:
	{
		if (file_buf.cursor_colidx < 0) {
			int current_column = 0;

			for (int i = file_buf.cursor - 1; i >= 0; current_column++, i--)
				if (file_buf.contents[i] == '\n')
					break;

			file_buf.cursor_colidx = current_column;
		}

		int i;

		for (i = file_buf.cursor; i < file_buf.length; i++)
			if (file_buf.contents[i] == '\n') {
				i++;
				break;
			}

		const int next_line_begin = i;
		// at the beginning of the next line, let us traverse to the column in question

		while (i < file_buf.length && i - next_line_begin < file_buf.cursor_colidx) {
			if (file_buf.contents[i] == '\n')
				break;
			else
				i++;
		}

		file_buf.cursor = i;

		InvalidateRect(hwnd, NULL, false);

		break;
	}
	case VK_C:
	{
		bool controlPressed = GetKeyState(VK_CONTROL) & KEYDOWN_MASK;
		if (controlPressed) {
			// Ctrl+C: copy!
			if (file_buf.selection < 0) {
				return false; // without selection, copy does nothing
			}

			int selectionStart = min(file_buf.cursor, file_buf.selection);
			int selectionEnd = max(file_buf.cursor, file_buf.selection);

			HGLOBAL global = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, selectionEnd - selectionStart + 1);
			char* buffer = (char*)GlobalLock(global);
			strncpy(buffer, &file_buf.contents[selectionStart], selectionEnd - selectionStart);
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

	for (int i = 0; i <= file_buf.length; i++) {
		if (i > 0 && file_buf.contents[i - 1] == L'\n') {
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

static void remove_buffer_portion(int start, int end)
{
	assert(0 <= start && start < MAX_FILE);
	assert(0 <= end && end < MAX_FILE);
	assert(start <= end && end <= file_buf.length);

	size_t to_move = file_buf.length - end;

	memmove(&file_buf.contents[start], &file_buf.contents[end], to_move);
}

static void char_size_px(HDC hdc, int *width, int *height)
{
	const wchar_t full_block = L'\x2588';

	SIZE size;

	if (!GetTextExtentPoint32W(
		hdc,
		&full_block,
		1,
		&size))
		FATALW("Calculate text height");

	*width = size.cx;
	*height = size.cy;
}

static int mouse_coords_to_buffer_idx(POINT point)
{
	// get the line and column

	const int column = point.x / char_width_px;
	const int line = point.y / char_height_px;

	// convert the line and column to an index of the buffer

	int i = 0;
	for (int linei = 0; linei < line; linei++) {
		while (i < file_buf.length && file_buf.contents[i] != '\n')
			i++;

		if (i >= file_buf.length)
			goto done;
		else
			i++;
	}

	const int begin_line_i = i;

	while (i < file_buf.length && file_buf.contents[i] != '\n' && i < begin_line_i + column)
		i++;

	done:
	return i;
}
