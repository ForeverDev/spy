#ifndef GENERATE_H
#define GENERATE_H

#include "parse.h"

typedef struct CompileState CompileState;

struct CompileState {
	TreeNode* root;		
	TreeNode* focus;
	FILE* fout;
};

void generate_bytecode(TreeNode*, const char*);

#endif
