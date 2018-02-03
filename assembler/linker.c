#include <Windows.h>

typedef LPWSTR *(CALLBACK* _CommandLineToArgvW)(LPCWSTR lpCmdLine, int *pNumArgs);

#define MAX_LABELS	0x20000
#define RED			0x0c
#define WHITE 		0x07
#define BLUE		0x09
#define GREEN		0x0A

typedef struct LABEL {
	char *name;
	int offset;
	wchar_t *filename;
} LABEL;

void setcolor(short color) {
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}
void setcolumn(short column) {
	CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
		SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), (int)(csbi.dwCursorPosition.Y *0x10000 + column));
		
	}
}

void write(char *message, short color) {
	if (message == 0)
		return;
	int charsWritten;
	setcolor(color);
	WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), message, lstrlen(message), &charsWritten, NULL);
}

void writew(wchar_t *message, short color) {
	if (message == 0)
		return;
	int charsWritten;
	setcolor(color);
	WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), message, lstrlenW(message), &charsWritten, NULL);
}

void write_int(unsigned int value, short color) {
	char str[17];
	int i;
	str[16] = 0;
	if (value == 0) {
		str[15] = '0';
		write(&str[15], color);
		return;
	}
	for (i = 15; i > 0 && value != 0; i--) {
		str[i] = (value % 10) + '0';
		value /= 10;
	}
	write(&str[i + 1], color);
}

void die(char *message, short color) {
	write(message, color);
	setcolor(WHITE);
	ExitProcess(1);
}

LABEL *table_init() {
	LABEL *tbl= (LABEL *)VirtualAlloc(NULL, MAX_LABELS * sizeof(LABEL), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (tbl == 0)
		die("Error, out of memory", WHITE);
	for (int i = 0; i < MAX_LABELS; i++) {
		tbl[i].name = NULL;
		tbl[i].offset = 0;
		tbl[i].filename = 0;
	}
	return tbl;
}

int table_add(LABEL *tbl, char *name, wchar_t *filename, int offset, int strict) {
	for (int i = 0; i < MAX_LABELS; i++) {
		if (tbl[i].name == 0) {
			tbl[i].name = name;
			tbl[i].offset = offset;
			tbl[i].filename = filename;
			return 1;
		}
		if (lstrcmp(tbl[i].name, name) == 0 && strict == 1)
			die("Error: duplicate label\n", WHITE);
	}
	die("Error, too many labels\n", WHITE);
}

void table_read(LABEL *tbl, char *obj, int *len, int offset, wchar_t *filename, int strict) {
	*len = ((int *)obj)[0];
	for (int i = 4; i < *len; i += lstrlen(&obj[i + 4]) + 5) {
		int rva = ((int *)&obj[i])[0];
		char *name = &obj[i + 4];
		table_add(tbl, name, filename, rva + offset, strict);
	}
}

char *read_file(wchar_t *filename, int *fileSize) {
	HANDLE hFile = CreateFileW(filename, GENERIC_READ, 1, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		write("Error, could not open '", WHITE);
		writew(filename, WHITE);
		die("'\n", WHITE);
	}
	*fileSize = GetFileSize(hFile, NULL);
	char *lpFile = (char *)VirtualAlloc(0, *fileSize + 1, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (lpFile == NULL) {
		die("Error, ran out of memory\n", WHITE);
	}
	if (ReadFile(hFile, lpFile, *fileSize, NULL, NULL) == FALSE) {
		die("Error, could not read file\n", WHITE);
	}
	CloseHandle(hFile);
	return lpFile;
}

void write_file(wchar_t *filename, char *buffer, int size) {
	DeleteFileW(filename);
	HANDLE hFile = CreateFileW(filename, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		die("Error, could not open file to write", WHITE);
	}
	if (WriteFile(hFile, buffer, size, NULL, NULL) == FALSE) {
		die("Error, could not write file", WHITE);
	}
	CloseHandle(hFile);
}

void lmemcpy(char *a, char *b, int size) {
	for (int i = 0; i < size; i++) {
		a[i] = b[i];
	}
}

int _start() {
	int n_args, file_count = 0;
	HMODULE hShell32 = GetModuleHandle("shell32.dll");
	if (hShell32 == NULL) {
		hShell32 = LoadLibrary("shell32.dll");
	}	
	wchar_t **szArglist = (wchar_t **)((_CommandLineToArgvW)GetProcAddress(hShell32, "CommandLineToArgvW"))(GetCommandLineW(), &n_args);
	int state = 0, offset = 6, size, exp_size, imp_size, code_size;
	LABEL *exports = table_init();
	LABEL *imports = table_init();
	write("Linker:\t\t", WHITE);
	char *code = (char *)VirtualAlloc(NULL, 0x100000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (code == 0)
		die("Error, out of memory\n", WHITE);
	code[0] = 4;
	code[1] = 0x70;
	table_add(imports, "main", L"<entry>", 2, 0);
	wchar_t *output_file = 0;
	for (int i = 1; i < n_args; i++) {
		if (szArglist[i][0] == '-' && (szArglist[i][1] == 'o' || szArglist[i][1] == 'O')) {
			state = 1;
			continue;
		}
		if (state == 1) {
			output_file = szArglist[i];
			state = 0;
			continue;
		} 
		char *file_data = read_file(szArglist[i], &size);
		if (((int *)file_data)[0] != 0xfeffffff) {
			die("Error, invalid object file\n", WHITE);
		}
		table_read(exports, &file_data[4], &exp_size, offset, szArglist[i], 1);
		table_read(imports, &file_data[4 + exp_size], &imp_size, offset, szArglist[i], 0);
		code_size = size - (4 + exp_size + imp_size);
		lmemcpy(&code[offset], &file_data[4 + exp_size + imp_size], code_size);

		offset += code_size;
		file_count++;
		
	}
	int exp_c, imp_c;
	for (exp_c = 0; exports[exp_c].name != 0; exp_c++) {}
	for (imp_c = 0; imports[imp_c].name != 0; imp_c++) {}
	
	for (int i = 0; exports[i].name != 0; i++) {
		for (int j = 0; imports[j].name != 0; j++) {
			if (lstrcmp(exports[i].name, imports[j].name) == 0 && lstrlen(exports[i].name) == lstrlen(imports[i].name)) {
				((int *)&code[imports[j].offset])[0] = exports[i].offset - imports[j].offset + 2;
				for (int k = j; imports[k].name != 0; k++) {
					imports[k].name = imports[k + 1].name;
					imports[k].offset = imports[k + 1].offset;
				}
				j--;
			}
		}
	}
	for (int i = 0; imports[i].name != 0; i++) {
		write("Cannot resolve address '", WHITE);
		writew(imports[i].filename, WHITE);
		write(":", WHITE);
		write(imports[0].name, WHITE);
		die("'\n", WHITE);
	}
	if (output_file == 0)
		die("Error, No output file given\ntry using -o\n", WHITE);
	write_file(output_file, code, offset);
	write_int(exp_c, WHITE);
	write(" exports, ", WHITE);
	write_int(imp_c, WHITE);
	write(" imports: ", WHITE);
	setcolumn(80);
	write("[", WHITE);
	write("OK", GREEN);
	write("]\n", WHITE);
	ExitProcess(0);
}