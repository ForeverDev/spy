#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "assembler.h"

const AssemblerInstruction instructions[0xFF] = {
	{"NOOP",	0x00, {NO_OPERAND}},
	{"IPUSH",	0x01, {_INT64}},
	{"IADD",	0x02, {NO_OPERAND}},
	{"ISUB",	0x03, {NO_OPERAND}},
	{"IMUL",	0x04, {NO_OPERAND}},
	{"IDIV",	0x05, {NO_OPERAND}},
	{"MOD",	0x06, {NO_OPERAND}},
	{"SHL",		0x07, {NO_OPERAND}},
	{"SHR",		0x08, {NO_OPERAND}},
	{"AND",		0x09, {NO_OPERAND}},
	{"OR",		0x0A, {NO_OPERAND}},
	{"XOR",		0x0B, {NO_OPERAND}},
	{"NOT",		0x0C, {NO_OPERAND}},
	{"NEG",		0x0D, {NO_OPERAND}},
	{"IGT",		0x0E, {NO_OPERAND}},
	{"IGE",		0x0F, {NO_OPERAND}},
	{"ILT",		0x10, {NO_OPERAND}},
	{"ILE",		0x11, {NO_OPERAND}},
	{"ICMP",	0x12, {NO_OPERAND}},
	{"JNZ",		0x13, {_INT32}},
	{"JZ",		0x14, {_INT32}},
	{"JMP",		0x15, {_INT32}},
	{"CALL",	0x16, {_INT32, _INT32}},
	{"IRET",	0x17, {NO_OPERAND}},
	{"CCALL",	0x18, {_INT32, _INT32}},
	{"FPUSH",	0x19, {_FLOAT64}},
	{"FADD",	0x1A, {NO_OPERAND}},
	{"FSUB",	0x1B, {NO_OPERAND}},
	{"FMUL",	0x1C, {NO_OPERAND}},
	{"FDIV",	0x1D, {NO_OPERAND}},
	{"FGT",		0x1E, {NO_OPERAND}},
	{"FGE",		0x1F, {NO_OPERAND}},
	{"FLT",		0x20, {NO_OPERAND}},
	{"FLE",		0x21, {NO_OPERAND}},
	{"FCMP",	0x22, {NO_OPERAND}},
	{"FRET",	0x23, {NO_OPERAND}},
	{"ILLOAD",	0x24, {_INT32}},
	{"ILSAVE",	0x25, {_INT32}},
	{"IARG",	0x26, {_INT32}},
	{"ILOAD",	0x27, {NO_OPERAND}},
	{"ISAVE",	0x28, {NO_OPERAND}},
	{"RES",		0x29, {_INT32}},
	{"ILEA",	0x2A, {_INT32}},
	{"IDER",	0x2B, {NO_OPERAND}},
	{"ICINC",	0x2C, {_INT64}},
	{"CDER",	0x2D, {NO_OPERAND}},
	{"LOR",		0x2E, {NO_OPERAND}},
	{"LAND",	0x2F, {NO_OPERAND}},
	{"PADD",	0x30, {NO_OPERAND}},
	{"PSUB",	0x31, {NO_OPERAND}},
	{"LOG",		0x32, {_INT32}},
	{"VRET",	0x33, {NO_OPERAND}},
	{"DBON",	0x34, {NO_OPERAND}},
	{"DBOFF",	0x35, {NO_OPERAND}},
	{"DBDS",	0x36, {NO_OPERAND}},
	{"CJNZ",	0x37, {NO_OPERAND}},
	{"CJZ",		0x38, {NO_OPERAND}},
	{"CJMP",	0x39, {NO_OPERAND}},
	{"ILNSAVE",	0x3A, {_INT32, _INT32}},
	{"ILNLOAD",	0x3B, {_INT32, _INT32}},
	{"FLLOAD",	0x3C, {_INT32}},
	{"FLSAVE",	0x3D, {_INT32}},
	{"FTOI",	0x3E, {_INT32}},
	{"ITOF",	0x3F, {_INT32}},
};

void
Assembler_generateBytecodeFile(const char* in_file_name) {
	Assembler A;
	A.labels = NULL;
	A.tokens = NULL;
	A.constants = NULL;

	AssemblerFile input;
	input.handle = fopen(in_file_name, "rb");
	if (!input.handle) {
		Assembler_die(&A, "Couldn't open source file '%s'", in_file_name);
	}
	fseek(input.handle, 0, SEEK_END);
	input.length = ftell(input.handle);
	fseek(input.handle, 0, SEEK_SET);
	input.contents = (char *)malloc(input.length + 1);
	fread(input.contents, 1, input.length, input.handle);
	input.contents[input.length] = 0;
	fclose(input.handle);

	AssemblerFile output;
	size_t name_len;
	char* out_file_name;
	name_len = strlen(in_file_name);
	out_file_name = malloc(name_len + 1);
	strcpy(out_file_name, in_file_name);
	out_file_name[name_len] = 0;
	out_file_name[name_len - 1] = 'b'; /* .spys -> .spyb */
	output.handle = fopen(out_file_name, "wb");
	if (!output.handle) {
		Assembler_die(&A, "Couldn't open output file for writing");
	}
	output.length = 0;
	output.contents = NULL;

	AssemblerFile tmp_output;
	if (!output.handle) {
		Assembler_die(&A, "Couldn't open tmp file for writing");
	}
	tmp_output.handle = fopen(TMPFILE_NAME, "wb");
	tmp_output.length = 0;
	tmp_output.contents = NULL;

	uint64_t index = 0;
	uint64_t rom_size = 0;
	const AssemblerInstruction* ins;
	AssemblerToken* head;
	
	if (!(A.tokens = head = AsmLexer_convertToAssemblerTokens(input.contents))) goto done;

	/* pass one, find all labels */
	while (A.tokens && A.tokens->next) {
		if (A.tokens->type == IDENTIFIER) {
			if ((A.tokens->next->type == PUNCT && (A.tokens->next->word[0] == ':'))) {
				if (A.tokens->next->word[0] == ':') {
					Assembler_appendLabel(&A, A.tokens->word, index);
				}
				if (A.tokens->prev) {
					AssemblerToken* save = A.tokens;
					A.tokens->prev->next = A.tokens->next->next;
					if (A.tokens->next->next) {
						A.tokens->next->next->prev = A.tokens->prev;
					}
					A.tokens = A.tokens->next->next;
					free(save->next);
					free(save);
					continue;
				} else if (A.tokens->next->next) {
					AssemblerToken* save = A.tokens;
					A.tokens->next->next->prev = NULL;
					A.tokens = A.tokens->next->next;
					free(save->next);
					free(save);
					continue;
				}
			} else if (!strcmp_lower(A.tokens->word, "let")) {
				size_t len;
				A.tokens = A.tokens->next;
				Assembler_appendConstant(&A, A.tokens->word, rom_size);
				A.tokens = A.tokens->next;
				len = strlen(A.tokens->word);
				fwrite(A.tokens->word, 1, len, tmp_output.handle);
				fputc(0, tmp_output.handle);
				rom_size += len + 1;
			} else if ((ins = Assembler_validateInstruction(&A, A.tokens->word))) {
				index++; /* instruction is one byte */
				for (int i = 0; i < 4; i++) {
					if (ins->operands[i] == NO_OPERAND) break;
					A.tokens = A.tokens->next;
					index += (
						ins->operands[i] == _INT64 ? 8 :
						ins->operands[i] == _INT32 ? 4 : 
						ins->operands[i] == _FLOAT64 ? 8 : 0
					);
				}
			}
		}
		A.tokens = A.tokens->next;
	}
	A.tokens = head;

	/* pass two, replace labels, insert static memory */
	while (A.tokens) {
		if (!strcmp_lower(A.tokens->word, "let")) {
			A.tokens = A.tokens->next->next;
		} else if (A.tokens->type == IDENTIFIER && strcmp_lower(A.tokens->word, "let") && !Assembler_validateInstruction(&A, A.tokens->word)) {
			if (A.tokens->next && A.tokens->next->word[0] != ':')  {
				for (const AssemblerLabel* i = A.labels; i; i = i->next) {
					if (!strcmp(i->identifier, A.tokens->word)) {
						free(A.tokens->word);
						sprintf((A.tokens->word = (char *)malloc(128)), "%u", i->index);
						goto safe;
					}
				}
			}
			if (A.tokens->prev) {
				for (const AssemblerConstant* i = A.constants; i; i = i->next) {
					if (!strcmp(i->identifier, A.tokens->word)) {
						free(A.tokens->word);
						sprintf((A.tokens->word = (char *)malloc(128)), "%u", i->index);
						goto safe;
					}
				}
			}
			printf("%s\n", A.tokens->next->word);
			Assembler_die(&A, "unexpected identifier '%s'", A.tokens->word);
			safe:;
		}
		A.tokens = A.tokens->next;
	}
	A.tokens = head;

	/* pass three, assemble */
	while (A.tokens) {
		switch (A.tokens->type) {
			case PUNCT:
				switch (A.tokens->word[0]) {
								
				}
				break;
			case IDENTIFIER:
			{
				if (!strcmp_lower(A.tokens->word, "let")) {
					A.tokens = A.tokens->next->next;
					continue;
				} else if (!(ins = Assembler_validateInstruction(&A, A.tokens->word))) {
					Assembler_die(&A, "unknown instruction '%s'", A.tokens->word);
				}
				fputc(ins->opcode, tmp_output.handle);
				/* go through the operands */
				for (int i = 0; i < 4; i++) {
					if (ins->operands[i] == NO_OPERAND) break;
					A.tokens = A.tokens->next;
					if (A.tokens && A.tokens->word[0] == ',') {
						A.tokens = A.tokens->next;
					}
					if (!A.tokens) {
						Assembler_die(&A, "expected operand(s)");
					}
					switch (ins->operands[i]) {
						case _INT64:
						{
							uint64_t n = A.tokens->word[1] == 'x' ? strtoll(&A.tokens->word[2], NULL, 16) : strtol(A.tokens->word, NULL, 10);
							fwrite(&n, 1, 8, tmp_output.handle);
							break;
						}
						case _INT32:
						{
							uint64_t n = A.tokens->word[1] == 'x' ? strtoll(&A.tokens->word[2], NULL, 16) : strtol(A.tokens->word, NULL, 10);
							fwrite(&n, 1, 4, tmp_output.handle);
							break;
						}
						case _FLOAT64:
						{
							double n = strtod(A.tokens->word, NULL);
							fwrite(&n, 1, 8, tmp_output.handle);
							break;
						}
						case NO_OPERAND:
							break;
					}
				}
				break;
			}
			case NUMBER:
				break;
			case NOTOK:
				break;
			default:
				break;
		}
		A.tokens = A.tokens->next;
	}
	
	/* close temporary write file and open for reading */
	AssemblerFile tmp_input;	
	fclose(tmp_output.handle);
	tmp_input.handle = fopen(TMPFILE_NAME, "rb");
	/* N/A */
	tmp_input.length = 0;
	tmp_input.contents = NULL;

	/* write the headers for the output file */
	const uint32_t magic = 0x5950535F;
	const uint32_t rom = sizeof(uint32_t) * 2;
	const uint32_t code = sizeof(uint32_t) * 3 + rom_size;
	fwrite(&magic, sizeof(uint32_t), 1, output.handle);
	fwrite(&rom, sizeof(uint32_t), 1, output.handle);
	fwrite(&code, sizeof(uint32_t), 1, output.handle);

	/* copy temporary file into output file */
	char c;
	while ((c = fgetc(tmp_input.handle)) != EOF) {
		fputc(c, output.handle);
	}

	done:
	if (tmp_output.handle) {
		fclose(tmp_output.handle);
	}
	fclose(output.handle);	
	free(A.tokens);
	free(input.contents);
}

static void
Assembler_die(Assembler* A, const char* format, ...) {
	va_list list;
	printf("\n*** Spyre assembler error (line %d) ***\n", A->tokens ? A->tokens->line : 0);
	va_start(list, format);
	vprintf(format, list);
	va_end(list);
	printf("\n\n");
	exit(1);
}

static void
Assembler_appendLabel(Assembler* A, const char* identifier, uint32_t index) {
	AssemblerLabel* label;
	size_t idlen;
	label = (AssemblerLabel *)malloc(sizeof(AssemblerLabel));
	idlen = strlen(identifier);
	label->identifier = (char *)malloc(idlen + 1);
	strcpy(label->identifier, identifier);
	label->identifier[idlen] = 0;
	label->index = index;
	label->next = NULL;
	if (!A->labels) {
		A->labels = label;
	} else {
		AssemblerLabel* at = A->labels;
		while (at->next) at = at->next;
		at->next = label;
	}
}

static void
Assembler_appendConstant(Assembler* A, const char* identifier, uint32_t index) {
	AssemblerConstant* constant;
	size_t idlen;
	constant = (AssemblerConstant *)malloc(sizeof(AssemblerConstant));
	idlen = strlen(identifier);
	constant->identifier = (char *)malloc(idlen + 1);
	strcpy(constant->identifier, identifier);
	constant->identifier[idlen] = 0;
	constant->index = index;
	constant->next = NULL;
	if (!A->constants) {
		A->constants = constant;
	} else {
		AssemblerConstant* at = A->constants;
		while (at->next) at = at->next;
		at->next = constant;
	}
}

/* 0 = not valid, 1 = valid */
static const AssemblerInstruction*
Assembler_validateInstruction(Assembler* A, const char* instruction) {
	for (int i = 0; i <= 0x3F; i++) {
		if (!strcmp_lower(instructions[i].name, instruction)) {
			return &instructions[i];	
		};
	}
	return NULL;
}

/* case insensitive strcmp (for various validations) */
/* 0 = strings are same (to match strcmp) */
static int 
strcmp_lower(const char* a, const char* b) {
	if (strlen(a) != strlen(b)) return 1;
	while (*a) {
		if (tolower(*a++) != tolower(*b++)) return 1; 
	}
	return 0;
}
