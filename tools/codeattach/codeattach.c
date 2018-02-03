#include <Windows.h>

#define SEED 	0xb53e194d
#define KEY     "\x44\x58\xd7\x89\x64\x10\x3d\x95\x88\x97\xd2\xe6\x5c\x43\xd9\x4a\x0f\xcf\xbd\x18\x0c\xd5\xcf\x15\x98\x94\x6a\x81\x16\xcc\xee\xdf"

#define RED			0x0c
#define WHITE 		0x07
#define BLUE		0x09
#define GREEN		0x0A

wchar_t lpInFile, lpOutFile;

typedef LPWSTR *(CALLBACK* _CommandLineToArgvW)(LPCWSTR lpCmdLine, int *pNumArgs);

char *message = "Invalid parameters.\n\nUsage:\ncodeattach.exe \"infile.bin\" \"sourcefile.c\" \"outfile.c\"";


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

int _strstr(char *haystack, char *needle) {
	int len = lstrlen(needle);
	int pass;
	for (int i = 0; i < lstrlen(haystack) - len; i++) {
		pass = 1;
		for (int j = 0; j < len; j++) {
			if (haystack[i + j] != needle[j]) {
				pass = 0;
				break;
			}
		}
		if (pass == 1) {
			return i;
		}
	}
	return -1;
}

char *read_file(wchar_t *filename, int *fileSize) {
	HANDLE hFile = CreateFileW(filename, GENERIC_READ, 1, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		die("Error, could not open file\n", RED);
	}
	*fileSize = GetFileSize(hFile, NULL) + 1;
	char *lpFile = (char *)VirtualAlloc(0, *fileSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (lpFile == NULL) {
		die("Error, ran out of memory\n", RED);
	}
	if (ReadFile(hFile, lpFile, *fileSize - 1, NULL, NULL) == FALSE) {
		die("Error, could not read file\n", RED);
	}
	lpFile[*fileSize - 1] = 0;
	CloseHandle(hFile);
	return lpFile;
}

void write_file(wchar_t *filename, char *buffer, int size) {
	DeleteFileW(filename);
	HANDLE hFile = CreateFileW(filename, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		die("Error, could not open file to write\n", RED);
	}
	if (WriteFile(hFile, buffer, size, NULL, NULL) == FALSE) {
		die("Error, could not write file\n", RED);
	}
	CloseHandle(hFile);
}

char int_to_hex_char(int x) {
	if (x < 10) {
		return x + '0';
	} else {
		return x + 'a' - 10;
	}
}
	
int _start() {
	int n_args, charsWritten;
	HMODULE hShell32 = GetModuleHandle("shell32.dll");
	if (hShell32 == NULL) {
		hShell32 = LoadLibrary("shell32.dll");
	}	
	LPWSTR *szArglist = ((_CommandLineToArgvW)GetProcAddress(hShell32, "CommandLineToArgvW"))(GetCommandLineW(), &n_args);
	HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

	if (n_args != 4) {
		die(message, WHITE);
	}
	int inFileSize, outFileSize;
	char *inFile = read_file(szArglist[1], &inFileSize);
	char *outFile = read_file(szArglist[2], &outFileSize);
	char *final = (char *)VirtualAlloc(0, inFileSize * 4 + outFileSize + 0x100, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	
	char *key = KEY;
	
	//for (int i = 0; i < inFileSize; i++) {
	//	inFile[i] ^= key[i & 0x1f];
	//}
	
	write("CodeAttach:\t", WHITE);
	int offset = _strstr(outFile, "/* code */");
	
	if (offset == -1) {
		die("Error, invalid code", RED);
	}
	for (int i = 0; i < offset; i++) {
		final[i] = outFile[i];
	}
	
	for (int i = 0; i < inFileSize; i++) {
		final[lstrlen(final)] = '\\';
		final[lstrlen(final)] = 'x';
		final[lstrlen(final)] = int_to_hex_char((inFile[i] >> 4) & 0xf);
		final[lstrlen(final)] = int_to_hex_char(inFile[i] & 0xf);
	}
	
	int offset2 = _strstr(outFile, "/* code_len */");
	if (offset2 == -1) {
		die("Error, invalid source code (*.c)", RED);
	}
	
	for (int i = offset + lstrlen("/* code */"); i < offset2; i++) {
		final[lstrlen(final)] = outFile[i];
	}
	
	final[lstrlen(final)] = '0';
	final[lstrlen(final)] = 'x';
	for (int i = 28; i >= 0; i -= 4) {
		final[lstrlen(final)] = int_to_hex_char((inFileSize >> i) & 0xf);
	}
	
	int offset3 = _strstr(outFile, "/* key */");
	for (int i = offset2 + lstrlen("/* code-len */"); i < offset3; i++) {
		final[lstrlen(final)] = outFile[i];
	}
	
	for (int i = 0; i < 0x20; i++) {
		final[lstrlen(final)] = '\\';
		final[lstrlen(final)] = 'x';
		final[lstrlen(final)] = int_to_hex_char((key[i] >> 8) & 0xf);
		final[lstrlen(final)] = int_to_hex_char(key[i] & 0xf);
	}
	
	for (int i = offset3 + lstrlen("/* key */"); i < lstrlen(outFile); i++) {
		final[lstrlen(final)] = outFile[i];	
	}
	
	write_file(szArglist[3], final, lstrlen(final));
	
	
	write_int(inFileSize, WHITE);
	write(" bytes loaded: ", WHITE);
	setcolumn(80);
	write("[", WHITE);
	write("OK", GREEN);
	write("]\n", WHITE);
	ExitProcess(0);
}