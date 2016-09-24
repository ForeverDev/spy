#ifndef PARSE_H
#define PARSE_H

#include "lex.h"

typedef struct TreeBlock TreeBlock;
typedef struct TreeNode TreeNode;
typedef struct TreeIf TreeIf;
typedef struct TreeWhile TreeWhile;
typedef struct TreeFunc TreeFunc;
typedef struct TreeRoot TreeRoot;
typedef struct ParseState ParseState;
typedef enum NodeType NodeType;

enum NodeType {
	NODE_IF,
	NODE_WHILE,
	NODE_FUNC,
	NODE_ROOT
};

struct TreeIf {
	Token* condition;
	TreeBlock* block;
};

struct TreeWhile {
	Token* condition;
	TreeBlock* block;
};

struct TreeBlock {
	TreeNode* parent;
	TreeNode* children;
};

struct TreeRoot {
	TreeBlock* block;
};

struct TreeNode {
	NodeType type;
	TreeNode* next;
	TreeNode* prev;
	TreeBlock* parent;
	union {
		TreeIf* pif;
		TreeWhile* pwhile;
		TreeFunc* pfunc;
		TreeRoot* proot;
	};
};

struct ParseState {
	TreeNode* root;
	TreeBlock* block;
	Token* token;
};

TreeNode* generate_tree(Token*);

#endif
