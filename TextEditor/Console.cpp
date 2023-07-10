#include "TextEditor.h"

#include <windowsx.h>

#include <string>
#include <stdlib.h>

// Pipe ends to communicate with cmd.exe
static HANDLE g_stdinWrite, g_stdoutRead, g_stderrRead;

static constexpr size_t STDOUT_BUFFER_SIZE = 0x2000;
static char g_stdoutRingBuffer[STDOUT_BUFFER_SIZE];

static std::string g_consoleInput;

static HWND g_consoleWindow;
static HFONT g_consoleFont;

static LRESULT CALLBACK ConsoleWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static DWORD ConsoleReadTask(void* argument);
static void GetCursorConsolePosition(HDC hdc, int cursorIndex, int* x, int* y, int* textHeight);
static void WriteStringToStdin(const std::string* string);

void RegisterConsoleWindowClass()
{
	WNDCLASS consoleWindowClass;
	memset(&consoleWindowClass, 0, sizeof consoleWindowClass);

	consoleWindowClass.lpfnWndProc = ConsoleWindowProc;
	consoleWindowClass.lpszClassName = CONSOLE_WINDOW_CLASS;

	RegisterClass(&consoleWindowClass);
}

static LRESULT CALLBACK ConsoleWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_CREATE:
	{
		g_consoleWindow = hwnd;
		memset(g_stdoutRingBuffer, 0, sizeof g_stdoutRingBuffer);

		HANDLE stdinRead;
		HANDLE stdoutWrite;
		HANDLE stderrWrite;

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

		// Let us create a thread that spends its time blocking, waiting to read from cmd.exe
		HANDLE consoleThread;
		if (NULL == (consoleThread = CreateThread(
			NULL, // security attributes
			0, // stack size - 0 = default
			ConsoleReadTask, // procedure
			NULL, // procedure argument
			0, // flags
			NULL // thread id
		)))
			FATALW("Create console thread");

		g_consoleFont = CreateFont(
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
		char charParam = (char)wParam;
		switch (charParam) {
		case '\b':
			g_consoleInput.pop_back();
			break;
		case '\r':
			g_consoleInput.append("\r\n");
			WriteStringToStdin(&g_consoleInput);
			g_consoleInput.clear();
			break;
		default:
			g_consoleInput.push_back(charParam);
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

		COLORREF backgroundColor = RGB(0, 0, 0);
		SetBkColor(hdc, backgroundColor);
		HBRUSH backgroundBrush = CreateSolidBrush(backgroundColor);
		FillRect(hdc, &rect, backgroundBrush);

		SetTextColor(hdc, RGB(255, 255, 255));
		SelectFont(hdc, g_consoleFont);
		DrawTextA(hdc, g_stdoutRingBuffer, -1, &rect, DT_TOP | DT_LEFT);

		int x, y, height;
		GetCursorConsolePosition(hdc, strlen(g_stdoutRingBuffer), &x, &y, &height);
		rect.left = x;
		rect.top = y - height;
		DrawTextA(hdc, g_consoleInput.c_str(), -1, &rect, DT_TOP | DT_LEFT);

		EndPaint(hwnd, &ps);

		break;
	}
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	return 0;
}

static DWORD ConsoleReadTask(void* argument)
{
	if (NULL != argument)
		FATALW("ConsoleReadThread expected argument to be NULL");

	char localReadBuffer[STDOUT_BUFFER_SIZE];
	DWORD bytesRead;

	while (true) {
		if (!ReadFile(g_stdoutRead, localReadBuffer, STDOUT_BUFFER_SIZE, &bytesRead, NULL))
			FATALW("ConsoleReadThread read console stdout");

		size_t consoleOutputLength = strlen(g_stdoutRingBuffer);
		memcpy(&g_stdoutRingBuffer[consoleOutputLength], localReadBuffer, bytesRead);

		InvalidateRect(g_consoleWindow, NULL, true);
	}

	return EXIT_SUCCESS;
}

static void GetCursorConsolePosition(HDC hdc, int cursorIndex, int* x, int* y, int* textHeight)
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

	for (int i = 0; i <= strlen(g_stdoutRingBuffer); i++) {
		if (i > 0 && g_stdoutRingBuffer[i - 1] == L'\n') {
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

static void WriteStringToStdin(const std::string *string)
{
	DWORD bytesWritten;

	if (!WriteFile(g_stdinWrite, string->c_str(), string->length(), &bytesWritten, NULL))
		FATALW("Send console input to stdin WriteFile");
}
