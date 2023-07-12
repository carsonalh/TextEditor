#include "texteditor.h"

#include <windowsx.h>
#include <tchar.h>
#include <pathcch.h>

#include <stdlib.h>
#include <assert.h>

#define LISTING_HEIGHT_PX (32)
#define DIRECTORY_MAX_DEPTH ((size_t)64)

typedef struct directory_list_item directory_list_item;
typedef struct directory_listing directory_listing;

struct directory_list_item {
	char name[MAX_PATH];
	enum { DL_FILE, DL_DIRECTORY } type;
	bool open;                   // only valid if type == DL_DIRECTORY
	directory_listing* children; // only valid if type == DL_DIRECTORY
};

struct directory_listing {
	size_t length;
	size_t capacity;
	directory_list_item items[];
};

static LRESULT CALLBACK file_tree_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void paint_file_tree(HWND hwnd, HDC hdc);
static directory_listing *list_directory_recursive(const TCHAR* dirname);
static directory_list_item *locate_list_item(
	const directory_listing *listing /* file tree to look through (visually) */,
	int n /* the nth 0-based listing from the top that the user clicked on */);

#define ROOT_LISTING_INIT_LEN ((size_t)64)
directory_listing *root_listing;

void register_file_tree_window_class(void)
{
	WNDCLASS wclass = { 0 };

	wclass.lpszClassName = FILE_TREE_WINDOW_CLASS;
	wclass.lpfnWndProc = file_tree_window_proc;

	RegisterClass(&wclass);
}

static LRESULT CALLBACK file_tree_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
	{
		TCHAR current_dir[MAX_PATH];
		GetCurrentDirectory(MAX_PATH, current_dir);
		root_listing = list_directory_recursive(current_dir);
		
		break;
	}
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		paint_file_tree(hwnd, hdc);
		EndPaint(hwnd, &ps);
		break;
	}
	case WM_LBUTTONDOWN:
	{
		const int y = GET_Y_LPARAM(lParam);
		const int n = y / LISTING_HEIGHT_PX;
		
		directory_list_item *li = locate_list_item(root_listing, n);

		if (NULL == li)
			MessageBox(hwnd, TEXT("GOING TO IGNORE THAT"), NULL, MB_OK);
		else if (DL_FILE == li->type)
			MessageBox(hwnd, TEXT("OPEN A FILE"), NULL, MB_OK);
		else if (DL_DIRECTORY == li->type) {
			li->open = !li->open;
			RECT client;
			GetClientRect(hwnd, &client);
			InvalidateRect(hwnd, &client, true);
		}
		else
			FATALW("Click on filetree item that is neither file nor directory");

		break;
	}
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	return 0;
}

static void paint_file_tree(HWND hwnd, HDC hdc)
{
	RECT client;
	GetClientRect(hwnd, &client);

	RECT fill_area = {
		.left = client.left, .right = client.right,
		.top = 0, .bottom = LISTING_HEIGHT_PX,
	};

	// stack of indices for the current traversal
	struct {
		int idx;
		const directory_listing *l;
	} stack[DIRECTORY_MAX_DEPTH] = {
		{ 0, root_listing },
	};

	HBRUSH dir_brush, file_brush, bg_brush;
	dir_brush = CreateSolidBrush(RGB(255, 90, 90));
	file_brush = CreateSolidBrush(RGB(90, 90, 255));
	bg_brush = CreateSolidBrush(RGB(255, 255, 255));

	FillRect(hdc, &client, bg_brush);

	DeleteObject(bg_brush);

	TCHAR text[MAX_PATH];

	int top = 0;
	while (top >= 0) {
	traverse_layer:

		if (top > DIRECTORY_MAX_DEPTH)
			FATALW("File tree traverse too far");

		while (stack[top].idx < stack[top].l->length) {
			const int current_idx = stack[top].idx;

			if (DL_DIRECTORY == stack[top].l->items[current_idx].type)
				FillRect(hdc, &fill_area, dir_brush);
			else if (DL_FILE == stack[top].l->items[current_idx].type)
				FillRect(hdc, &fill_area, file_brush);
			else
				FATALW("Paint listing that is neither directory nor file");

			size_t name_len = strlen(stack[top].l->items[current_idx].name);
			cstr_to_tstr(text, stack[top].l->items[current_idx].name, MAX_PATH);
			DrawText(
				hdc, text, name_len, &fill_area,
				DT_LEFT | DT_SINGLELINE | DT_VCENTER);

			fill_area.bottom += LISTING_HEIGHT_PX;
			fill_area.top += LISTING_HEIGHT_PX;

			const directory_listing* inner_listing = stack[top].l->items[current_idx].children;
			bool should_go_deeper = DL_DIRECTORY == stack[top].l->items[current_idx].type &&
				stack[top].l->items[current_idx].open;

			if (DL_DIRECTORY == stack[top].l->items[current_idx].type &&
				stack[top].l->items[current_idx].open &&
				NULL == stack[top].l->items[current_idx].children)
				FATALW("Illegal state in file tree");

			stack[top].idx++;

			if (should_go_deeper) {
				top++;
				stack[top].idx = 0;
				stack[top].l = inner_listing;
				goto traverse_layer;
			}
		}

		top--;
	}

	DeleteObject(dir_brush);
	DeleteObject(file_brush);
}

static directory_listing *list_directory_recursive(const TCHAR *dirname)
{
	TCHAR path[MAX_PATH];
	WIN32_FIND_DATA find_data = { 0 };

	const size_t initial_size = sizeof (directory_listing) +
		ROOT_LISTING_INIT_LEN * sizeof (directory_list_item);

	directory_listing *listing = (directory_listing*)malloc(initial_size);
	memset(listing, 0, initial_size);
	listing->length = 0;
	listing->capacity = ROOT_LISTING_INIT_LEN;

	_tcsncpy(path, dirname, MAX_PATH);
	PathCchAppend(path, MAX_PATH, TEXT("*"));

	HANDLE search = FindFirstFile(path, &find_data);

	if (INVALID_HANDLE_VALUE == search)
		FATALW("FindFirstFile start file search");

	directory_list_item *li = &listing->items[0];

	do {
		if (
			0 == _tcsncmp(find_data.cFileName, TEXT("."), MAX_PATH) ||
			0 == _tcsncmp(find_data.cFileName, TEXT(".."), MAX_PATH))
			continue;

		if (listing->length > listing->capacity) {
			const int c = 2 * listing->capacity;
			const size_t new_size = sizeof *listing + c * sizeof listing->items[0];
			listing = realloc(listing, new_size);
		}

		tstr_to_cstr(
			li->name,
			find_data.cFileName,
			sizeof find_data.cFileName / sizeof find_data.cFileName[0]);

		li->type =
			find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY
				? DL_DIRECTORY
				: DL_FILE;

		if (DL_DIRECTORY == li->type) {
			_tcsncpy(path, dirname, MAX_PATH);
			PathCchAppend(path, MAX_PATH, find_data.cFileName);

			li->children = list_directory_recursive(path);
		}

		listing->length++;
		li++;
	} while (FindNextFile(search, &find_data));

	if (ERROR_NO_MORE_FILES != GetLastError())
		FATALW("List files");

	return listing;
}

static directory_list_item *locate_list_item_impl(const directory_listing *listing, int *n)
{
	for (int i = 0; i < listing->length; i++) {
		if (0 == *n)
			return &listing->items[i];
		--*n;

		directory_list_item *li;
		if (DL_DIRECTORY == listing->items[i].type && listing->items[i].open)
			if ((li = locate_list_item_impl(listing->items[i].children, n)) != NULL)
				return li;
	}

	return NULL;
}

static directory_list_item *locate_list_item(
	const directory_listing *listing /* file tree to look through (visually) */,
	int n /* the nth 0-based listing from the top that the user clicked on */)
{
	return locate_list_item_impl(listing, &n);
}