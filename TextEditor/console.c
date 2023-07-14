#include "texteditor.h"

#include <windowsx.h>
#include <tchar.h>

#include <stdlib.h>
#include <assert.h>

#define OUTPUT_BUFFER_LEN ((size_t)0x2000)
#define INPUT_BUFFER_LEN ((size_t)0x400)

// Pipe ends to communicate with cmd.exe
static HANDLE input_write, output_read;

static struct ring_buffer {
	int end; // exclusive
	bool full;
	/* we only use the first half of buffer, but when we need a contiguous string (i.e. the buffer is full),
	   we can use the second half to copy the first half over */
	char buffer[2 * OUTPUT_BUFFER_LEN];
} output_rb = { 0 };

static struct input_buffer {
	static_assert(INT_MAX >= INPUT_BUFFER_LEN, "all input buffer locations must be addressable");

	int length;
	char buffer[INPUT_BUFFER_LEN];
} input_buf = { 0 };

static HWND console_window;
static HFONT console_font;
static HANDLE console_thread;

static LRESULT CALLBACK console_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static DWORD console_read_task(void* argument);
static int text_height_px(HDC hdc);
static int text_width_px(HDC hdc);
static int buffer_height_px(HDC hdc);
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

		HANDLE input_read, output_write;

		SECURITY_ATTRIBUTES sa;
		memset(&sa, 0, sizeof sa);

		sa.nLength = sizeof sa;
		sa.lpSecurityDescriptor = NULL;
		sa.bInheritHandle = true;

		#define PIPE_SIZE 1024

		if (!CreatePipe(&input_read, &input_write, &sa, PIPE_SIZE))	  FATALW("Pipe input");
		if (!CreatePipe(&output_read, &output_write, &sa, PIPE_SIZE)) FATALW("Pipe stdout");

		STARTUPINFO si;
		memset(&si, 0, sizeof si);
		si.cb = sizeof si;
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdInput = input_read;
		si.hStdOutput = output_write;
		si.hStdError = output_write;

		PROCESS_INFORMATION pi;
		memset(&pi, 0, sizeof pi);

		TCHAR cmdline[MAX_PATH] = { 0 };
		_tcsncpy(cmdline, TEXT("C:\\Windows\\System32\\cmd.exe"), MAX_PATH);

		if (!CreateProcess(
				NULL, cmdline,
				NULL, NULL, // process, thread attributes
				true, // inherit handles
				CREATE_NO_WINDOW, // creation flags
				NULL, // string of environment variables
				NULL, // working directory, NULL means inherit from this process
				&si, // [in] startup info
				&pi)) // [out] process info
			FATALW("Create cmd.exe");

		// Let us create a thread that spends its time blocking, waiting to read from cmd.exe
		if (NULL == (console_thread = CreateThread(
				NULL, // security attributes
				0, // stack size - 0 = default
				console_read_task, // procedure
				NULL, // procedure argument
				0, // flags
				NULL))) // thread id
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
	{
		if (!TerminateThread(console_thread, 0)) FATALW("Terminate console thread");

		PostQuitMessage(0);
		break;
	}
	case WM_CHAR:
	{
		char char_param = (char)wParam;
		switch (char_param) {
		case '\b':
			input_buf.length = max(input_buf.length - 1, 0);
			break;
		case '\r':
			strncpy(&input_buf.buffer[input_buf.length], "\r\n", INPUT_BUFFER_LEN - input_buf.length);
			write_to_stdin(input_buf.buffer, INPUT_BUFFER_LEN);
			memset(&input_buf, 0, sizeof input_buf);
			break;
		default:
			if (input_buf.length < INPUT_BUFFER_LEN)
				input_buf.buffer[input_buf.length++] = char_param;
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
		RECT client;
		GetClientRect(hwnd, &client);

		COLORREF background_color = RGB(0, 0, 0);
		SetBkColor(hdc, background_color);
		HBRUSH background_brush = CreateSolidBrush(background_color);
		FillRect(hdc, &client, background_brush);
		DeleteObject(background_brush);

		SetTextColor(hdc, RGB(255, 255, 255));
		SelectFont(hdc, console_font);

		// draw console output
		{
			RECT text_area = {
				.top = client.bottom - buffer_height_px(hdc), .bottom = client.bottom,
				.left = client.left, .right = client.right,
			};

			if (output_rb.full) {
				memset(&output_rb.buffer[OUTPUT_BUFFER_LEN], 0, OUTPUT_BUFFER_LEN);
				memcpy(&output_rb.buffer[OUTPUT_BUFFER_LEN], output_rb.buffer, output_rb.end);
				DrawTextA(hdc, &output_rb.buffer[output_rb.end], OUTPUT_BUFFER_LEN, &text_area, DT_BOTTOM | DT_LEFT);
			}
			else
				DrawTextA(hdc, output_rb.buffer, output_rb.end, &text_area, DT_BOTTOM | DT_LEFT);
		}

		// draw console input
		{
			const int height = text_height_px(hdc);
			const int width = text_width_px(hdc);

			size_t last_line_len = 0;
			
			for (int i = output_rb.end - 1;
					i != output_rb.end && output_rb.buffer[i] != '\n';
					i = (i - 1) % OUTPUT_BUFFER_LEN)
				++last_line_len;

			const RECT text_area = {
				.top = client.bottom - height, .bottom = client.bottom,
				.left = client.left + last_line_len * width, .right = client.right,
			};

			DrawTextA(hdc, input_buf.buffer, input_buf.length, &text_area, DT_BOTTOM | DT_LEFT);

			// draw the cursor

			const RECT cursor = {
				.left = (last_line_len + input_buf.length) * width,
				.right = (1 + last_line_len + input_buf.length) * width,
				.bottom = client.bottom, .top = client.bottom - height,
			};

			HBRUSH white = CreateSolidBrush(RGB(255, 255, 255));
			FillRect(hdc, &cursor, white);
			DeleteObject(white);
		}

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

	char local_buffer[OUTPUT_BUFFER_LEN];

	while (true) {
		DWORD bytes_read;

		if (!ReadFile(output_read, local_buffer, OUTPUT_BUFFER_LEN, &bytes_read, NULL))
			FATALW("ConsoleReadThread read console stdout");

		if (output_rb.end + bytes_read >= OUTPUT_BUFFER_LEN) {
			const size_t first_half = OUTPUT_BUFFER_LEN - output_rb.end;
			const size_t second_half = bytes_read - first_half;
			memcpy(&output_rb.buffer[output_rb.end], local_buffer, first_half);
			memcpy(output_rb.buffer, &local_buffer[first_half], second_half);
			output_rb.full = true;
		}
		else {
			memcpy(&output_rb.buffer[output_rb.end], local_buffer, bytes_read);
		}

		output_rb.end = (output_rb.end + bytes_read) % OUTPUT_BUFFER_LEN;

		InvalidateRect(console_window, NULL, true);
	}

	return EXIT_SUCCESS;
}

static int text_height_px(HDC hdc)
{
	const wchar_t full_block = L'\x2588';

	SIZE size;

	if (!GetTextExtentPoint32(
			hdc,
			&full_block,
			1,
			&size))
		FATALW("Calculate text height");

	return size.cy;
}

static int text_width_px(HDC hdc)
{
	const wchar_t full_block = L'\x2588';

	SIZE size;

	if (!GetTextExtentPoint32(
		hdc,
		&full_block,
		1,
		&size))
		FATALW("Calculate text width");

	return size.cx;
}

static int buffer_height_px(HDC hdc)
{
	size_t lines = 1;

	for (int i = 0; i < OUTPUT_BUFFER_LEN; i++)
		lines += output_rb.buffer[i] == '\n';

	return lines * text_height_px(hdc);
}

static void write_to_stdin(const char* string, size_t n)
{
	DWORD bytes_written;

	const size_t length = strnlen(string, n);

	if (!WriteFile(input_write, string, length, &bytes_written, NULL))
		FATALW("Send console input to stdin WriteFile");
}
