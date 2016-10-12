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
typedef struct TreeStruct TreeStruct;
typedef struct TreeReturn TreeReturn;
typedef struct TreeContinue TreeContinue;
typedef struct TreeBreak TreeBreak;
typedef struct ParseState ParseState;
typedef enum NodeType NodeType;
typedef enum TreeType TreeType;

enum NodeType {
	NODE_NOTYPE,
	NODE_IF,
	NODE_WHILE,
	NODE_FUNCTION,
	NODE_ASSIGN,
	NODE_STATEMENT,
	NODE_RETURN,
	NODE_ROOT,
	NODE_CONTINUE,
	NODE_BREAK
};

enum TreeType {
	TYPE_NOTYPE,
	TYPE_INT,
	TYPE_FLOAT,
	TYPE_STRING,
	TYPE_BYTE,
	TYPE_STRUCT,
	TYPE_NULL
};

struct TreeStruct {
	char* type_name;
	unsigned int complete; /* 0 if struct only declared */
	unsigned int size;
	TreeDecl* children;
	TreeStruct* next;
};

struct TreeDatatype {
	TreeType type;
	TreeStruct* pstruct; /* only applicable if type == TYPE_STRUCT */
	unsigned int ptr_level;
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

struct TreeReturn {
	Token* statement;
};

struct TreeIf {
	Token* condition;
	TreeBlock* block;
	enum IfType {
		IF_REG,
		IF_ELIF,
		IF_ELSE
	} if_type;
};

struct TreeWhile {
	Token* condition;
	TreeBlock* block;
};

struct TreeFunction {
	char* identifier;
	unsigned int nargs;
	unsigned int nlocals; /* doesn't include nargs */
	unsigned int reserve_space;
	unsigned int is_cfunc;
	unsigned int is_vararg; /* only C funcs can be vararg */
	TreeDatatype* return_type;
	TreeDecl* arguments;
	TreeBlock* block;
};

struct TreeContinue {
	
};

struct TreeBreak {

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
		TreeReturn* pret;
		TreeRoot* proot;
		TreeContinue* pcont;
		TreeBreak* pbreak;
	};
};

struct ParseState {
	TreeNode* root;
	TreeFunction* func;
	TreeBlock* block;
	TreeStruct* defined_types; /* used defined types */
	Token* token;
};

ParseState* generate_tree(Token*);
char* tostring_datatype(TreeDatatype*); /* expose to generate.c */

#endif
