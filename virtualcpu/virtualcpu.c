#include <Windows.h>

char *code = 	"/* code */";
int  code_len = /* code_len */;
char *key = 	"/* key */";

#define REG_DP				11
#define REG_BP				12
#define REG_SP				13
#define REG_IP				14
#define REG_CF				15

#define MEMORY_SIZE 		0x4000
#define STACK_SIZE			0x1000

#define INST_NOP			0
#define INST_MOV			1
#define INST_ADD			2
#define INST_SUB			3
#define INST_JMP			4
#define INST_PUSH			5
#define INST_POP			6
#define INST_CALL			7
#define INST_RET			8
#define INST_ENTER			9
#define INST_LEAVE			10
#define INST_CMP			11
#define INST_JE				12
#define INST_JNE			13
#define INST_JG				14
#define INST_JL				15
#define INST_JGE			16
#define	INST_JLE			17
#define INST_JZ				18
#define INST_JNZ			19
#define INST_MUL			20
#define INST_DIV			21
#define INST_MOD			22
#define INST_SHR			23
#define INST_SHL			24
#define INST_AND			25
#define INST_OR				26
#define INST_XOR			27
#define INST_EIP			28

#define OPERAND_NA			0
#define OPERAND_REG			1
#define OPERAND_REG_8		2
#define OPERAND_VAL			3
#define OPERAND_VAL_8		4
#define OPERAND_ADDR		5
#define OPERAND_ADDR_REG	6
#define OPERAND_LABEL		7
#define OPERAND_ARG			8
#define OPERAND_ADDR_REG_8	9

typedef struct MEMORY {
	int reg[0x10];
	char *code;
	int code_len;
	char *key;
	int *GetModuleHandle;
	int *GetProcAddress;
	char mem[MEMORY_SIZE + STACK_SIZE * sizeof(int)];
} MEMORY;

void write(char *message) {
	if (message == 0)
		return;
	int charsWritten;
	WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), message, lstrlen(message), &charsWritten, NULL);
}

void write_int(unsigned int value) {
	char str[18];
	int i, v;
	str[17] = 0;
	str[16] = 0;
	if (value == 0) {
		str[15] = '0';
		str[16] = '\t';
		write(&str[15]);
		return;
	}
	for (i = 15; i > 0 && value != 0; i--) {
		v = value & 0xf;
		if (v > 9) {
			v += 'a' - 10;
		} else {
			v += '0';
		}
		str[i] = v;
		value >>= 4;
	}
	if (i > 7) {
		str[16] = '\t';
	}
	write(&str[i + 1]);
}

void setcolor(short color) {
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

char code_read_8(MEMORY *memory, char *address) {
	return address[0]; //^ memory->key[((int)address - (int)memory->code) & 0x1f];
}

int code_read_32(MEMORY *memory, int *address) {
	//int k = memory->key[(int)address & 0xf];
	//k += memory->key[(int)address & 0xf] << 8;
	//k += memory->key[(int)address & 0xf] << 16;
	//k += memory->key[(int)address & 0xf] << 24;
	return address[0]; // ^ k;
}

void reg_write(MEMORY *memory, int index, int value) {
	memory->reg[index] = value ^ ((int *)memory->key)[index & 7];
}

int reg_read(MEMORY *memory, int index) {
	return memory->reg[index] ^ ((int *)memory->key)[index & 7];
}

void stack_push(MEMORY *memory, int value) {
	int sp = reg_read(memory, REG_SP);
	((int *)sp)[0] = value ^ ((int *)memory->key)[(sp >> 2) & 7];
	reg_write(memory, REG_SP, sp - 4);
}

int stack_pop(MEMORY *memory) {
	int sp = reg_read(memory, REG_SP) + 4;
	reg_write(memory, REG_SP, sp);
	return ((int *)sp)[0] ^ ((int *)memory->key)[(sp >> 2) & 7];
}

int load_operand(MEMORY *memory, int *pc, char type, int *act) {
	int i;
	switch (type) {
	case OPERAND_NA:
		return 0;
	case OPERAND_VAL:
		*pc += 4;
		return code_read_32(memory,  (int *)(*pc - 4));
	case OPERAND_VAL_8:
		*pc += 1;
		return code_read_8(memory,  (char *)(*pc - 1)) & 0xff;
	case OPERAND_REG:
		*act = code_read_8(memory, (char *)(*pc));
		*pc += 1;
		return reg_read(memory, *act);
	case OPERAND_LABEL:
		*pc += 4;
		return reg_read(memory, REG_IP) + code_read_32(memory, (int *)((*pc - 4)));
	case OPERAND_ADDR_REG:
		*act = code_read_8(memory, (char *)(*pc));
		*pc += 1;
		return ((int *)reg_read(memory, *act))[0];
	case OPERAND_ARG:
		i = reg_read(memory, REG_BP) - (code_read_8(memory, (char *)(*pc)) * 4) + 12;
		*pc += 1;
		return ((int *)i)[0] ^ ((int *)memory->key)[(i >> 2) & 7];
	case OPERAND_ADDR_REG_8:
		*act = reg_read(memory, code_read_8(memory, (char *)(*pc)));
		*pc += 1;
		return ((int)((char *)*act)[0]) & 0xff;
	}
}

int _start() {
	int pc, a, b, r, next;
	unsigned char op, type;
	MEMORY *memory = (MEMORY *)VirtualAlloc(0, sizeof(MEMORY), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (memory == NULL) {
		return 0;
	}
	setcolor(7);
	write("vcpu.exe:\n\n");
	setcolor(0x80);
	write("addr\t\top\t\ttype\t\ta\t\tb\t\tr\t\t\n");
	setcolor(7);
	memory->code = code;
	memory->code_len = code_len;
	memory->key = key;
	reg_write(memory, REG_IP, (int)memory->code);
	reg_write(memory, REG_DP, (int)memory->mem);
	reg_write(memory, REG_SP, ((int)memory->mem) + MEMORY_SIZE + STACK_SIZE * sizeof(int) - 4);
	reg_write(memory, REG_BP, 0);
	stack_push(memory, 0);
	int i = 1;
	int act;
	while (TRUE) {
		pc = reg_read(memory, REG_IP);
		if (pc == 0) {
			write("Code Exit\n");
			ExitProcess(0);
		}
		write_int(pc);
		write("\t");
		op = code_read_8(memory, (char *)pc);
		type = code_read_8(memory, (char *)(pc + 1));
		

		pc += 2;
		
		a = load_operand(memory, &pc, (type >> 4) & 0xf, &act);
		b = load_operand(memory, &pc, type & 0xf, &r);
		r = 0;
		next = 1;
		switch (op) {
		case INST_NOP:
			r = 0;
			break;
		case INST_MOV:
			r = b;
			next = 0;
			break;
		case INST_ADD:
			r = a + b;
			next = 0;
			break;
		case INST_SUB:
			r = a - b;
			next = 0;
			break;
		case INST_JMP:
			pc = a;
			break;
		case INST_PUSH:
			stack_push(memory, a);
			break;
		case INST_POP:
			r = stack_pop(memory);
			next = 0;
			break;
		case INST_CALL:
			stack_push(memory, pc);
			pc = a;
			break;
		case INST_RET:
			pc = stack_pop(memory);
			reg_write(memory, REG_SP, reg_read(memory, REG_SP) + a);
			break;
		case INST_ENTER:
			stack_push(memory, reg_read(memory, REG_BP));
			reg_write(memory, REG_BP, reg_read(memory, REG_SP));
			break;
		case INST_LEAVE:
			reg_write(memory, REG_SP, reg_read(memory, REG_BP));
			reg_write(memory, REG_BP, stack_pop(memory));
			pc = stack_pop(memory);
			reg_write(memory, REG_SP, reg_read(memory, REG_SP) + a);
			break;
		case INST_CMP:
			r = a - b;
			reg_write(memory, REG_CF, r);
			break;
		case INST_JE:
			if (reg_read(memory, REG_CF) == 0)
				pc = a;
			break;
		case INST_JNE:
			if (reg_read(memory, REG_CF) != 0)
				pc = a;
			break;
		case INST_JG:
			if (reg_read(memory, REG_CF) > 0)
				pc = a;
			break;
		case INST_JL:
			if (reg_read(memory, REG_CF) < 0)
				pc = a;
			break;
		case INST_JGE:
			if (reg_read(memory, REG_CF) >= 0)
				pc = a;
			break;
		case INST_JLE:
			if (reg_read(memory, REG_CF) <= 0)
				pc = a;
			break;
		case INST_JZ:
			if (a == 0)
				pc = a;
			break;
		case INST_JNZ:
			if (a == 0)
				pc = a;
			break;
		case INST_MUL:
			r = a * b;
			next = 0;
			break;
		case INST_DIV:
			r = a / b;
			next = 0;
			break;
		case INST_MOD:
			r = a % b;
			next = 0;
			break;
		case INST_SHR:
			r = (unsigned int)a >> (unsigned int)b;
			next = 0;
			break;
		case INST_SHL:
			r = (unsigned int)a << (unsigned int)b;
			next = 0;
			break;
		case INST_AND:
			r = a & b;
			next = 0;
			break;
		case INST_OR:
			r = a | b;
			next = 0;
			break;
		case INST_XOR:
			r = a ^ b;
			next = 0;
			break;
		case INST_EIP:
			__asm__ __volatile__(
				"call *%0;\n"
				:
				:"r" (a)
			);
		}
		if (next == 0) {
			switch (type  >> 4) {
			case OPERAND_REG:
				reg_write(memory, act, r);
				break;
			case OPERAND_REG_8:
				reg_write(memory, act, r & 0xff);
				break;
			case OPERAND_ADDR_REG:
				((int *)act)[0] = r;
			case OPERAND_ADDR_REG_8:
				((char *)act)[0] = r;
			}
		}
		reg_write(memory, REG_IP, pc);
		i++;
		write_int(op);
		write("\t");
		write_int(type);
		write("\t");
		write_int(a);
		write("\t");
		write_int(b);
		write("\t");
		write_int(r);
		write("\t\n");
	}
	return 0;
}