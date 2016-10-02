#ifndef GENERATE_H
#define GENERATE_H

#include "parse.h"

typedef struct CompileState CompileState;
typedef struct InsStack InsStack;
typedef struct StringList StringList;

struct StringList {
	char* str;
	StringList* next;
};

struct InsStack {
	InsStack* next;
	InsStack* prev;
	StringList* instruction;
	unsigned int depth;
};

struct CompileState {
	Token* token; /* so we don't need to pass Token* between calls */
	TreeNode* root;		
	TreeNode* focus;
	TreeFunction* func; /* current function being generated */
	TreeStruct* defined_types;
	InsStack* ins_stack;
	unsigned int depth;
	unsigned int label_count;
	unsigned int literal_count;
	unsigned int return_label;
	FILE* fout;
};

void generate_bytecode(ParseState*, const char*);

#endif
