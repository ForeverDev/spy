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
	TreeNode* root;		
	TreeNode* focus;
	InsStack* ins_stack;
	unsigned int depth;
	unsigned int label_count;
	FILE* fout;
};

void generate_bytecode(TreeNode*, const char*);

#endif
