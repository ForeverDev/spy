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
typedef struct TreeFunction TreeFunction;
typedef struct TreeRoot TreeRoot;
typedef struct TreeAssign TreeAssign;
typedef struct TreeStatement TreeStatement;
typedef struct TreeDecl TreeDecl;
typedef struct TreeDatatype TreeDatatype;
typedef struct ParseState ParseState;
typedef enum NodeType NodeType;

enum NodeType {
	NODE_IF,
	NODE_WHILE,
	NODE_FUNCTION,
	NODE_ASSIGN,
	NODE_STATEMENT,
	NODE_ROOT
};

struct TreeDatatype {
	char* type_name;
	uint32_t modifier;
	struct ArrayDimension {
		unsigned int dimension;
		unsigned int size;
		struct ArrayDimension* next;
	} *dimensions;
};

struct TreeDecl {
	char* identifier;
	unsigned int offset;
	TreeDatatype* datatype;
	TreeDecl* next;
};

struct TreeAssign {
	Token* lhs;
	Token* rhs;
};

struct TreeStatement {
	Token* statement;
};

struct TreeIf {
	Token* condition;
	TreeBlock* block;
};

struct TreeWhile {
	Token* condition;
	TreeBlock* block;
};

struct TreeFunction {
	char* identifier;
	unsigned int nargs;
	unsigned int nlocals; /* doesn't include nargs */
	TreeDatatype* return_type;
	TreeDecl* arguments;
	TreeBlock* block;
};

struct TreeBlock {
	TreeNode* parent_node;
	TreeNode* children;
	TreeDecl* locals;
};

struct TreeRoot {
	TreeBlock* block;
};

struct TreeNode {
	NodeType type;
	TreeNode* next;
	TreeNode* prev;
	TreeBlock* parent_block;
	unsigned int line;
	union {
		TreeIf* pif;
		TreeWhile* pwhile;
		TreeFunction* pfunc;
		TreeStatement* pstate;
		TreeAssign* pass;
		TreeRoot* proot;
	};
};

struct ParseState {
	TreeNode* root;
	TreeFunction* func;
	TreeBlock* block;
	Token* token;
};

TreeNode* generate_tree(Token*);

#endif
