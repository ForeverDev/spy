#ifndef PARSE_H
#define PARSE_H

#include <stdint.h>
#include "lex.h"

#define MOD_CONST		(0x1 << 0)
#define MOD_VOLATILE	(0x1 << 1)
#define MOD_UNSIGNED	(0x1 << 2)
#define MOD_SIGNED		(0x1 << 3)
#define MOD_STATIC		(0x1 << 4)

typedef struct TreeBlock TreeBlock;
typedef struct TreeNode TreeNode;
typedef struct TreeIf TreeIf;
typedef struct TreeWhile TreeWhile;
typedef struct TreeFunc TreeFunc;
typedef struct TreeRoot TreeRoot;
typedef struct TreeAssign TreeAssign;
typedef struct TreeDecl TreeDecl;
typedef struct TreeDatatype TreeDatatype;
typedef struct ParseState ParseState;
typedef enum NodeType NodeType;

enum NodeType {
	NODE_IF,
	NODE_WHILE,
	NODE_FUNC,
	NODE_ASSIGN,
	NODE_ROOT
};

struct TreeDatatype {
	char* type_name;
	uint32_t modifier;
};

struct TreeDecl {
	char* identifier;
	TreeDatatype* datatype;
	TreeDecl* next;
};

struct TreeAssign {
	Token* lhs;
	Token* rhs;
};

struct TreeIf {
	Token* condition;
	TreeBlock* block;
};

struct TreeWhile {
	Token* condition;
	TreeBlock* block;
};

struct TreeFunc {
	char* identifier;
	TreeDatatype* return_type;
	TreeDecl* arguments;
	TreeBlock* block;
};

struct TreeBlock {
	TreeNode* parent_node;
	TreeNode* children;
};

struct TreeRoot {
	TreeBlock* block;
};

struct TreeNode {
	NodeType type;
	TreeNode* next;
	TreeNode* prev;
	TreeBlock* parent_block;
	union {
		TreeIf* pif;
		TreeWhile* pwhile;
		TreeFunc* pfunc;
		TreeAssign* pass;
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
