#include "texteditor.h"

#include <windowsx.h>

#include <stdlib.h>

#define STDOUT_BUFFER_SIZE ((size_t)0x2000)
#define INPUT_LEN ((size_t)256)

// Pipe ends to communicate with cmd.exe
static HANDLE stdin_write, stdout_read, stderr_read;

static char stdout_ring_buffer[STDOUT_BUFFER_SIZE];

static char console_input[INPUT_LEN];
static size_t console_input_len = 0;

static HWND console_window;
static HFONT console_font;

static LRESULT CALLBACK console_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static DWORD console_read_task(void* argument);
static void get_cursor_console_position(HDC hdc, int cursorIndex, int* x, int* y, int* textHeight);
static void write_to_stdin(const char *string, size_t n);

void register_console_window_class(void)
{
	WNDCLASS console_window_class;
	memset(&console_window_class, 0, sizeof console_window_class);

	console_window_class.lpfnWndProc = console_window_proc;
	console_window_class.lpszClassName = CONSOLE_WINDOW_CLASS;

	RegisterClass(&console_window_class);
}

static LRESULT CALLBACK console_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_CREATE:
	{
		console_window = hwnd;
		memset(stdout_ring_buffer, 0, sizeof stdout_ring_buffer);

		HANDLE stdin_read, stdout_write, stderr_write;

		SECURITY_ATTRIBUTES sa;
		memset(&sa, 0, sizeof sa);

		sa.nLength = sizeof sa;
		sa.lpSecurityDescriptor = NULL;
		sa.bInheritHandle = true;

		#define PIPE_SIZE 1024

		if (!CreatePipe(&stdin_read, &stdin_write, &sa, PIPE_SIZE)) FATALW("Pipe stdin");
		if (!CreatePipe(&stdout_read, &stdout_write, &sa, PIPE_SIZE)) FATALW("Pipe stdout");
		if (!CreatePipe(&stderr_read, &stderr_write, &sa, PIPE_SIZE)) FATALW("Pipe stderr");

		STARTUPINFO si;
		memset(&si, 0, sizeof si);
		si.cb = sizeof si;
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdInput = stdin_read;
		si.hStdOutput = stdout_write;
		si.hStdError = stderr_write;

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

		// Let us create a thread that spends its time blocking, waiting to read from cmd.exe
		HANDLE consoleThread;
		if (NULL == (consoleThread = CreateThread(
			NULL, // security attributes
			0, // stack size - 0 = default
			console_read_task, // procedure
			NULL, // procedure argument
			0, // flags
			NULL // thread id
		)))
			FATALW("Create console thread");

		console_font = CreateFont(
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
	}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_CHAR:
	{
		char char_param = (char)wParam;
		switch (char_param) {
		case '\b':
			console_input_len--;
			break;
		case '\r':
			strncpy(&console_input[console_input_len], "\r\n", INPUT_LEN - console_input_len);
			write_to_stdin(console_input, INPUT_LEN);
			memset(console_input, 0, sizeof console_input);
			console_input_len = 0;
			break;
		default:
			if (console_input_len < INPUT_LEN)
				console_input[console_input_len++] = char_param;
		}

		InvalidateRect(hwnd, NULL, true);

		break;
	}
	case WM_LBUTTONDOWN:
	{
		SetFocus(hwnd);
		break;
	}
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);

		// we'll try reading from the cmd.exe stdout buffer
		RECT rect;
		GetClientRect(hwnd, &rect);

		COLORREF background_color = RGB(0, 0, 0);
		SetBkColor(hdc, background_color);
		HBRUSH background_brush = CreateSolidBrush(background_color);
		FillRect(hdc, &rect, background_brush);

		SetTextColor(hdc, RGB(255, 255, 255));
		SelectFont(hdc, console_font);
		DrawTextA(hdc, stdout_ring_buffer, -1, &rect, DT_TOP | DT_LEFT);

		int x, y, height;
		get_cursor_console_position(hdc, strlen(stdout_ring_buffer), &x, &y, &height);
		rect.left = x;
		rect.top = y - height;
		DrawTextA(hdc, console_input, console_input_len, &rect, DT_TOP | DT_LEFT);

		EndPaint(hwnd, &ps);

		break;
	}
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	return 0;
}

static DWORD console_read_task(void* argument)
{
	if (NULL != argument)
		FATALW("ConsoleReadThread expected argument to be NULL");

	char buffer[STDOUT_BUFFER_SIZE];
	DWORD bytesRead;

	while (true) {
		if (!ReadFile(stdout_read, buffer, STDOUT_BUFFER_SIZE, &bytesRead, NULL))
			FATALW("ConsoleReadThread read console stdout");

		size_t console_output_length = strlen(stdout_ring_buffer);
		memcpy(&stdout_ring_buffer[console_output_length], buffer, bytesRead);

		InvalidateRect(console_window, NULL, true);
	}

	return EXIT_SUCCESS;
}

static void get_cursor_console_position(HDC hdc, int cursorIndex, int* x, int* y, int* textHeight)
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

	for (int i = 0; i <= strlen(stdout_ring_buffer); i++) {
		if (i > 0 && stdout_ring_buffer[i - 1] == L'\n') {
			line_start = i;
			cursor_line_idx++;
		}

		if (i == cursorIndex)
			break;
	}

	int cursor_column_idx = cursorIndex - line_start;

	*x = cursor_column_idx * size.cx;
	*y = (cursor_line_idx + 1) * size.cy;
	*textHeight = size.cy;
}

static void write_to_stdin(const char* string, size_t n)
{
	DWORD bytes_written;

	const size_t length = strnlen(string, n);

	if (!WriteFile(stdin_write, string, length, &bytes_written, NULL))
		FATALW("Send console input to stdin WriteFile");
}
