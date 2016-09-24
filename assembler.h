#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <stdio.h>
#include <stdint.h>
#include "assembler_lex.h"

#define TMPFILE_NAME ".SPYRE_TEMP_FILE"

typedef struct Assembler Assembler;
typedef struct AssemblerFile AssemblerFile;
typedef struct AssemblerLabel AssemblerLabel;
typedef struct AssemblerConstant AssemblerConstant;
typedef struct AssemblerInstruction AssemblerInstruction;
typedef enum AssemblerOperand AssemblerOperand;

enum AssemblerOperand {
	NO_OPERAND = 0,
	_INT64,
	_INT32,
	_FLOAT64
};

struct Assembler {
	AssemblerToken*		tokens;
	AssemblerLabel*		labels;
	AssemblerConstant*	constants;
};

struct AssemblerFile {
	FILE*				handle;
	unsigned long long	length;
	char*				contents;
};

struct AssemblerLabel {
	char*				identifier;
	uint32_t			index;
	AssemblerLabel*		next;
};

struct AssemblerConstant {
	char*				identifier;
	uint32_t			index;
	AssemblerConstant*	next;
};

struct AssemblerInstruction {
	char*				name;
	uint8_t				opcode;
	AssemblerOperand	operands[4];
};

extern const AssemblerInstruction instructions[0xFF];

void Assembler_generateBytecodeFile(const char*);
static void	Assembler_die(Assembler*, const char*, ...);
static void Assembler_appendLabel(Assembler*, const char*, uint32_t);
static void Assembler_appendConstant(Assembler*, const char*, uint32_t);
static const AssemblerInstruction* Assembler_validateInstruction(Assembler*, const char*);
static int strcmp_lower(const char*, const char*);

#endif
