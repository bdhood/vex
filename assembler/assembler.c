#include <Windows.h>

#define OPERAND_NA			0
#define OPERAND_REG			1
#define OPERAND_VAL			3
#define OPERAND_VAL_8		4
#define OPERAND_ADDR		5
#define OPERAND_ADDR_REG	6
#define OPERAND_LABEL		7
#define OPERAND_ARG			8
#define OPERAND_ADDR_REG_8	9

#define RED		0x0c
#define WHITE 	0x07
#define BLUE	0x09
#define GREEN	0x0A

typedef LPWSTR *(CALLBACK* _CommandLineToArgvW)(LPCWSTR lpCmdLine, int *pNumArgs);

typedef struct INST {
	char *op;
	char *a;
	char *b;
} INST;

typedef struct LABEL {
	char *name;
	int offset;
} LABEL;

void setcolumn(short column) {
	CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
		SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), (int)(csbi.dwCursorPosition.Y *0x10000 + column));
		
	}
}

void setcolor(short color) {
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
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

void string_to_lower(char *str) {
	for (int i = 0; str[i] != 0; i++) {
		if (str[i] >= 'A' && str[i] <= 'Z') {
			str[i] = str[i] - ('A' - 'a');
		}
	}
}

void error_line(int line) {
	write("[", WHITE);
	write("ERROR", RED);
	write("] line ", WHITE);
	write_int(line + 1, WHITE);
	write(": ", WHITE);
}

int get_operator_value(char *operator) {
	if (operator == 0)
		return -3;
	if (lstrcmp("private", operator) == 0) { 	return -2; }
	if (lstrcmp("public", operator) == 0) {		return -2; }
	if (lstrcmp("nop", operator) == 0) {		return 0; }
	if (lstrcmp("mov", operator) == 0) {		return 1; }
	if (lstrcmp("add", operator) == 0) {		return 2; }
	if (lstrcmp("sub", operator) == 0) {		return 3; }
	if (lstrcmp("jmp", operator) == 0) {		return 4; }
	if (lstrcmp("push", operator) == 0) {		return 5; }
	if (lstrcmp("pop", operator) == 0) {		return 6; }
	if (lstrcmp("call", operator) == 0) {		return 7; }
	if (lstrcmp("ret", operator) == 0) {		return 8; }
	if (lstrcmp("enter", operator) == 0) {		return 9; }
	if (lstrcmp("leave", operator) == 0) {		return 10; }
	if (lstrcmp("cmp", operator) == 0) {	 	return 11; }
	if (lstrcmp("je", operator) == 0) {	 		return 12; }
	if (lstrcmp("jne", operator) == 0) {	 	return 13; }
	if (lstrcmp("jg", operator) == 0) {	 		return 14; }
	if (lstrcmp("jl", operator) == 0) {	 		return 15; }
	if (lstrcmp("jge", operator) == 0) {	 	return 16; }
	if (lstrcmp("jle", operator) == 0) {	 	return 17; }
	if (lstrcmp("jz", operator) == 0) {	 		return 18; }
	if (lstrcmp("jnz", operator) == 0) {	 	return 19; }
	if (lstrcmp("mul", operator) == 0) {	 	return 20; }
	if (lstrcmp("div", operator) == 0) {	 	return 21; }
	if (lstrcmp("mod", operator) == 0) {	 	return 22; }
	if (lstrcmp("shr", operator) == 0) {	 	return 23; }
	if (lstrcmp("shl", operator) == 0) {	 	return 24; }
	if (lstrcmp("and", operator) == 0) {	 	return 25; }
	if (lstrcmp("or", operator) == 0) {	 		return 26; }
	if (lstrcmp("xor", operator) == 0) {	 	return 27; }
	if (lstrcmp("eip", operator) == 0) {		return 28; }
	return -1;
}

int get_register_value(char *operand) {
	if (lstrlen(operand) > 3)
		return -1;
	char reg[4];
	char reg2[4];
	reg2[0] = 'r';
	reg2[2] = 0;
	lstrcpy(reg, operand);
	string_to_lower(reg);
	if (reg[0] == 'r') {
		if (reg[1] >= '0' && reg[1] <= '9')
			return reg[1] - '0';
	}
	if (lstrcmp(reg, "err") == 0) { return 10; }
	if (lstrcmp(reg, "dp") == 0) { 	return 11; }
	if (lstrcmp(reg, "bp") == 0) { 	return 12; }
	if (lstrcmp(reg, "sp") == 0) { 	return 13; }
	if (lstrcmp(reg, "ip") == 0) { 	return 14; }
	if (lstrcmp(reg, "cf") == 0) {	return 15; }
	return -1;
}

LABEL *table_init(int max_size) {
	LABEL *tbl= (LABEL *)VirtualAlloc(NULL, max_size * sizeof(LABEL), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (tbl == 0) {
		die("Out of memory 7\n", WHITE);
	}
	for (int i = 0; i < max_size; i++) {
		tbl[i].name = NULL;
		tbl[i].offset = 0;
	}
	return tbl;
}

int table_add(LABEL *tbl, char *name, int offset, int strict, int line) {
	for (int i = 0; i < 0x4000; i++) {
		if (tbl[i].name == 0) {
			tbl[i].name = name;
			tbl[i].offset = offset;
			return 1;
		}
		if (lstrcmp(tbl[i].name, name) == 0 && strict) {
			error_line(line);
			write("Label redefinition '", WHITE);
			write(name, WHITE);
			die("'\n", WHITE);
		}
	}
	error_line(line);
	die("Maximum label count is 16384\n", WHITE);
}

char *table_write(LABEL *tbl, int *size) {
	*size = 4;
	for (int i = 0; tbl[i].name != 0; i++) {
		*size += lstrlen(tbl[i].name);
		*size += 5;
	}
	char *output = (char *)VirtualAlloc(NULL, *size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (output == 0) {
		error_line(0);
		die("Out of memory 6\n", WHITE);
	}
	int offset = 4;
	((int *)output)[0] = *size;
	for (int i = 0; tbl[i].name != 0; i++) {
		((int *)&output[offset])[0] = tbl[i].offset;
		offset += 4;
		lstrcpy(&output[offset], tbl[i].name);
		offset += lstrlen(tbl[i].name) + 1;
		output[offset - 1] = 0;
	}
	return output;
}

LABEL *get_export_table(INST *code, int line_count) {
	LABEL *tbl = table_init(0x4000);
	int offset = 0;
	if (tbl == 0) {
		error_line(0);
		die("Out of memory 5\n", WHITE);
	}
	for (int i = 0; i < line_count; i++) {
		if (code[i].op == 0)
			continue;
		if (code[i].op[lstrlen(code[i].op) - 1] == ':') {
			code[i].op[lstrlen(code[i].op) - 1] = 0;
			table_add(tbl, code[i].op, offset, 1, i);
		}
		if (code[i].a != 0) {
			if ((lstrcmp("private", code[i].op) == 0) || (lstrcmp("public", code[i].op) == 0)) {
				if (code[i].a[lstrlen(code[i].a) - 1] == ':') {
					code[i].a[lstrlen(code[i].a) - 1] = 0;
					table_add(tbl, code[i].a, offset, 1, i);
				} else {
					error_line(i);
					die("Missing ':' after function name\n", WHITE);
				}
			} else if (lstrcmp("public", code[i].op) == 0) {
				if (code[i].a[lstrlen(code[i].a) - 1] == ':') {
					code[i].a[lstrlen(code[i].a) - 1] = 0;
					table_add(tbl, code[i].a, offset, 1, i);
				} else {
					error_line(i);
					die("Missing ':' after function name\n", WHITE);
				}
			}
		}
		if (get_operator_value(code[i].op) < 0)
			continue;
		offset += 2;
		offset += get_type_size(get_operand_type(code[i].a, i));
		offset += get_type_size(get_operand_type(code[i].b, i));
	}
	return tbl;
}

LABEL *get_import_table(INST *code, int line_count) {
	LABEL *tbl = table_init(0x4000);
	int offset = 0;
	if (tbl == 0) {
		error_line(0);
		die("Out of memory 4\n", WHITE);
	}
	for (int i = 0; i < line_count; i++) {
		if (code[i].op == 0)
			continue;
		if (lstrcmp(code[i].op, "public") == 0) 
			continue;
		if (lstrcmp(code[i].op, "private") == 0)
			continue;
		if (code[i].a != 0) {
			if (get_operand_type(code[i].a, i) == OPERAND_LABEL) {
				table_add(tbl, &code[i].a[1], offset + 2, 0, i);
			}
		}
		if (code[i].b != 0) {
			if (get_operand_type(code[i].b, i) == OPERAND_LABEL) {
				table_add(tbl, &code[i].b[1], offset + 2 + get_type_size(get_operand_type(code[i].a, i)), 0, i);
			}
		}
		if (get_operator_value(code[i].op) < 0)
			continue;
		offset += 2;
		offset += get_type_size(get_operand_type(code[i].a, i));
		offset += get_type_size(get_operand_type(code[i].b, i));
	}
	return tbl;
}

int get_type_size(int type) {
	switch (type) {
	case OPERAND_NA:
		return 0;
	case OPERAND_VAL:
		return 4;
	case OPERAND_VAL_8:
		return 1;
	case OPERAND_REG:
		return 1;
	case OPERAND_ADDR:
		return 4;
	case OPERAND_ADDR_REG:
		return 1;
	case OPERAND_LABEL:
		return 4;
	case OPERAND_ARG:
		return 1;
	case OPERAND_ADDR_REG_8:
		return 1;
	}
	return 0;
}

int get_integer_value(char *operand, int *value) {
	int i;
	*value = 0;
	if (operand[0] == '0' && operand[1] == 'x') {		// hex
		
		for (i = 2; (operand[i] >= '0' && operand[i] <= '9') || (operand[i] >= 'a' && operand[i] <= 'f'); i++) {
			*value <<= 4;
			*value += char_to_int(operand[i]);
		}
		if (operand[i] == 0 && i > 2)
			return 1;
	} else {											// dec
	
		for (i = 0; operand[i] >= '0' && operand[i] <= '9'; i++) {
			*value *= 10;
			*value += char_to_int(operand[i]);
		}
		if (operand[i] == 0 && i > 0)
			return 1;
	}
	return 0;
}

int get_operand_type(char *operand, int line) {
	int i;
	if (operand == 0)
		return OPERAND_NA;
	if (operand[0] == '*') {
		if (get_register_value(&operand[1]) != -1) {
			return OPERAND_ADDR_REG;
		} else if (get_integer_value(&operand[1], &i) == 1) {
			return OPERAND_ADDR;
		}
	} else if (get_register_value(operand) != -1) {
		return OPERAND_REG;
	} else if (get_integer_value(operand, &i) == 1) {
		if (i < 0x100)
			return OPERAND_VAL_8;
		return OPERAND_VAL;
	} else if (operand[0] == '$') {
		return OPERAND_LABEL;
	} else if (operand[0] == 'a' && operand[1] == 'r' && operand[2] == 'g') {
		if (get_integer_value(&operand[3], &i) == 1) {
			if (i >= 0 && i <= 9) {
				return OPERAND_ARG;
			}
		}
	} else if (operand[0] == '^') {
		if (get_register_value(&operand[1]) != -1) {
			return OPERAND_ADDR_REG_8;
		}
	}
	error_line(line);
	write("Unknown operand '", WHITE);
	write(operand, WHITE);
	die("'\n", WHITE);
}

char *read_file(wchar_t *filename, int *fileSize) {
	HANDLE hFile = CreateFileW(filename, GENERIC_READ, 1, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		error_line(0);
		write("Could not open '", WHITE);
		writew(filename, WHITE);
		die("', CreateFileW failed\n", WHITE);
	}
	*fileSize = GetFileSize(hFile, NULL) + 1;
	char *lpFile = (char *)VirtualAlloc(0, *fileSize + 1, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (lpFile == NULL) {
		error_line(0);
		die("Out of memory 3\n", WHITE);
	}
	if (ReadFile(hFile, lpFile, *fileSize, NULL, NULL) == FALSE) {
		error_line(0);
		die("Could not read, ReadFile() == FALSE\n", WHITE);
	}
	CloseHandle(hFile);
	lpFile[*fileSize - 1] = '\n';
	return lpFile;
}

void write_file(wchar_t *filename, char *buffer, int size) {
	DeleteFileW(filename);
	HANDLE hFile = CreateFileW(filename, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		error_line(0);
		write("Could not open '", WHITE);
		writew(filename, WHITE);
		die("', CreateFileW failed\n", WHITE);
	}
	if (WriteFile(hFile, buffer, size, NULL, NULL) == FALSE) {
		error_line(0);
		die("Could not write data, WriteFile() == FALSE\n", WHITE);
	}
	CloseHandle(hFile);
}

int char_to_int(char c) {
	c -= '0';
	if (c < 10 && c >= 0)
		return c;
	c += '0';
	c -= 'a';
	if (c < 6 && c >= 0)
		return c + 10;
	return 0xee;
}

int next_new_line(char *str, int start, int max) {
	int i, j;
	for (i = start; i < max; i++) {
		if (str[i] == 0) {
			return i + 1;
		}
	}
	return -1;
}

INST *parse_code(char *inFile, int inFileSize, int *line_count) {
	*line_count = 0;
	for (int i = 0; i < inFileSize; i++) {
		if (inFile[i] == '\n') {
			*line_count += 1;
		}
	}
	
	INST *lines = (INST *)VirtualAlloc(NULL, *line_count * sizeof(INST), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	
	if (lines == NULL) {
		error_line(0);
		die("Out of memory 2\n", WHITE);
	}
	
	int line_index = 0;
	int select = 0;
	int trip;
	for (int i = 0; i < inFileSize; i++) {
		if (inFile[i] == ';') {
			inFile[i] = 0;
			while (inFile[i] != '\n' && i < inFileSize) { i++; }
		}
		if (inFile[i] == '\n') {
			line_index++;
			lines[line_index].op = 0;
			lines[line_index].a = 0;
			lines[line_index].b = 0;
			inFile[i] = 0;
			trip = 0;
			select = 0;
		} else if (inFile[i] == ' ' || inFile[i] == '\t' || inFile[i] == ',' || inFile[i] == '\r') {
			inFile[i] = 0;
			if (trip == 1) {
				trip = 0;
				select++;
			}
		} else if (trip == 0) {
			trip = 1;
			switch (select) {
			case 0:
				lines[line_index].op = &inFile[i];
				break;
			case 1:
				lines[line_index].a = &inFile[i];
				break;
			case 2:
				lines[line_index].b = &inFile[i];
				break;
			}		
		}
	}
	return lines;
}

int get_operand_value(char *operand, int *size, int line) {
	int i;
	*size = 0;
	int type = get_operand_type(operand, line);
	switch (type) {
	case OPERAND_NA:
		return 0;
	case OPERAND_VAL:
		*size = 4;
		if (get_integer_value(operand, &i) == 1)
			return i;
		break;
	case OPERAND_VAL_8:
		*size = 1;
		if (get_integer_value(operand, &i) == 1)
			return i;
		break;
	case OPERAND_REG:
		*size = 1;
		i = get_register_value(operand);
		if (i != -1)
			return i;
		break;
	case OPERAND_ADDR:
		*size =  4;
		if (get_integer_value(&operand[1], &i) == 1)
			return i;
		break;
	case OPERAND_ADDR_REG:
		*size = 1;
		i = get_register_value(&operand[1]);
		if (i != -1)
			return i;
		break;
	case OPERAND_LABEL:
		*size = 4;
		return 0;
	case OPERAND_ARG:
		*size = 1;
		if (operand[0] == 'a' && operand[1] == 'r' && operand[2] == 'g') {
			if (get_integer_value(&operand[3], &i) == 1) {
				return i;
			}
		}
		break;
	case OPERAND_ADDR_REG_8:
		*size = 1;
		if (operand[0] == '^') {
			i = get_register_value(&operand[1]);
			if (i != -1)
				return i;
		}
		break;
	}
	*size = 0;
	return 0;
}

char *assemble(INST *code, int line_count, int *offset) {
	*offset = 0;

	for (int i = 0; i < line_count; i++) {
		if (get_operator_value(code[i].op) < 0) {
			continue;
		}
		((int)*offset) += 2;
		((int)*offset) += get_type_size(get_operand_type(code[i].a, i));
		((int)*offset) += get_type_size(get_operand_type(code[i].b, i));
	}
	char *obj = (char *)VirtualAlloc(NULL, *offset, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (obj == 0) {
		die("Out of memory 1\n", WHITE);
	}
	*offset = 0;
	int op, type, sizeA, sizeB, x;
	for (int i = 0; i < line_count; i++) {
		op = get_operator_value(code[i].op);
		if (op == -1) {
			error_line(i);
			write("Unknown opcode '", WHITE);
			write(code[i].op, WHITE);
			die("'\n", WHITE);
		} else if (op == -2 || op == -3) {
			continue;
		}
		obj[*offset] = op;
		type = get_operand_type(code[i].a, i) * 16;			
		type += get_operand_type(code[i].b, i);
		obj[*offset + 1] = type;
		x = get_operand_value(code[i].a, &sizeA, i);
		if (sizeA == 1) {
			obj[*offset + 2] = x;
		} else if (sizeA == 4) {
			((int *)&obj[*offset + 2])[0] = x;
		}
		x = get_operand_value(code[i].b, &sizeB, i);
		if (sizeB == 1) {
			obj[*offset + 2 + sizeA] = x;
		} else if (sizeB == 4) {
			((int *)&obj[*offset + 2 + sizeA])[0] = x;
		}
		
		*offset += 2;
		*offset += get_type_size(get_operand_type(code[i].a, i));
		*offset += get_type_size(get_operand_type(code[i].b, i));
	}
	return obj;
}

void lmemcpy(char *a, char *b, int size) {
	for (int i = 0; i < size; i++) {
		a[i] = b[i];
	}
}

int _start() {
	int n_args, op;
	HMODULE hShell32 = GetModuleHandle("shell32.dll");
	if (hShell32 == NULL) {
		hShell32 = LoadLibrary("shell32.dll");
	}	
	LPWSTR *szArglist = ((_CommandLineToArgvW)GetProcAddress(hShell32, "CommandLineToArgvW"))(GetCommandLineW(), &n_args);
	if (n_args != 3) {
		die("Invalid arguments!\nassembler.exe example.asm example.o\n", WHITE);
	}
	write("Assembler:\t", WHITE);
	writew(szArglist[1], WHITE);
	write(": ", WHITE);
	int inFileSize, outFileSize, line_count, size;
	char *inFile = read_file(szArglist[1], &inFileSize);
	
	INST *code = parse_code(inFile, inFileSize, &line_count);

	LABEL *exp_tbl = get_export_table(code, line_count);
	LABEL *imp_tbl = get_import_table(code, line_count);
	
	char *bin = assemble(code, line_count, &size);
	
	for (int i = 0; exp_tbl[i].name != 0; i++) {
		for (int j = 0; imp_tbl[j].name != 0; j++) {
			if (lstrcmp(imp_tbl[j].name, exp_tbl[i].name) == 0) {
				((int *)&bin[imp_tbl[j].offset])[0] = exp_tbl[i].offset - imp_tbl[j].offset + 2;
				for (int k = j; imp_tbl[k].name != 0; k++) {
					imp_tbl[k].name = imp_tbl[k + 1].name;
					imp_tbl[k].offset = imp_tbl[k + 1].offset;
				}
				j--;
			}
		}
	}
	int offset = 0;
	exp_tbl = table_init(0x4000);
	for (int i = 0; i < line_count; i++) {	
		if (code[i].op != 0 && code[i].a != 0) {
			if (lstrcmp("public", code[i].op) == 0) {	
				table_add(exp_tbl, code[i].a, offset, 1, i);
			}
		}
		if (get_operator_value(code[i].op) < 0)
			continue;
		offset += 2;
		offset += get_type_size(get_operand_type(code[i].a, i));
		offset += get_type_size(get_operand_type(code[i].b, i));
	}

	int exp_size, imp_size;
	char *export = table_write(exp_tbl, &exp_size);
	char *import = table_write(imp_tbl, &imp_size);
	
	char *output = (char *)VirtualAlloc(NULL, exp_size + imp_size + size + 4,  MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (output == 0) {
		error_line(0);
		die("Out of memory 0\n", WHITE);
	}

	((int *)output)[0] = 0xFEFFFFFF;
	
	int i;
	lmemcpy(&output[4], (char *)export, exp_size);
	lmemcpy(&output[4 + exp_size], (char *)import, imp_size);
	lmemcpy(&output[4 + exp_size + imp_size], bin, size);

	write_file(szArglist[2], output, 4 + exp_size + imp_size + size);
	setcolumn(80);
	write("[", WHITE);
	write("OK", GREEN);
	write("]\n", WHITE);
	ExitProcess(0);
}