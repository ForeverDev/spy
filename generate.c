#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "generate.h"

#define LABEL_FORMAT "__LABEL__%u"
#define FUNC_FORMAT "__FUNC__%s"
#define CFUNC_FORMAT "__CFUNC__%s"
#define STR_FORMAT "__STR__%d"
#define DEF_FUNC FUNC_FORMAT ":"
#define DEF_LABEL LABEL_FORMAT ":"
#define JZ_LABEL "jz " LABEL_FORMAT
#define JNZ_LABEL "jnz " LABEL_FORMAT
#define JMP_LABEL "jmp " LABEL_FORMAT
#define ASSOC_LEFT 1
#define ASSOC_RIGHT 2

typedef struct ExpNode ExpNode;
typedef struct ExpStack ExpStack;
typedef struct ExpOperator ExpOperator;
typedef struct ExpFuncCall ExpFuncCall;
typedef struct ExpLiteral ExpLiteral;
typedef enum ExpType ExpType;

enum ExpType {
	EXP_NOTYPE,
	EXP_FUNC_CALL,
	EXP_ARRAY_INDEX,
	EXP_LITERAL,
	EXP_OPERATOR,
	EXP_IDENTIFIER,
	EXP_DATATYPE
};

struct ExpOperator {
	unsigned int pres;
	unsigned int assoc;
};

struct ExpLiteral {
	char* word;
	TreeDatatype* datatype;
};

struct ExpFuncCall {
	TreeFunction* func;
	ExpNode* argument;
};

struct ExpNode {
	ExpType type;
	ExpNode* next;
	union {
		Token* poperator;
		Token* pidentifier;
		TreeDecl* pvariable;
		TreeDatatype* pdatatype;
		ExpLiteral* pliteral;
		ExpFuncCall* pcall;
		ExpNode* parray;
	};
};

struct ExpStack {
	ExpNode* value;
	ExpStack* next;
	ExpStack* prev;
};

static void print_expression(ExpNode*);
static int advance(CompileState*);
static int block_empty(CompileState*);
static const char* focus_tostring(CompileState*);
static void push_instruction(CompileState*, const char*, ...);
static StringList* pop_instruction(CompileState*);
static void asmput(CompileState*, const char*, ...);
static TreeDecl* find_local(CompileState*, const char*);
static void compile_error(CompileState*, const char*, ...);
static TreeStruct* type_defined(CompileState*, const char*);
static void comment(CompileState*, const char*, ...);
static void token_line(CompileState*, Token*);
static int identical_types(TreeDatatype*, TreeDatatype*);
static TreeDatatype* copy_datatype(TreeDatatype*);
static void literal_scan(CompileState*);
static void init_declarations(CompileState*, TreeBlock*);
static TreeDatatype* raw_datatype(CompileState*, ExpNode*);

/* generating (etc) functions */
static void generate_if(CompileState*);
static void generate_while(CompileState*);
static void generate_for(CompileState*);
static void generate_function_decl(CompileState*);
static void generate_assignment(CompileState*);
static void generate_statement(CompileState*);
static void generate_return(CompileState*);
static ExpNode* generate_expression(CompileState*, ExpNode*, int);
static ExpNode* postfix_expression(CompileState*, Token*);
static ExpNode* postfix_function_call(CompileState*);

/* ExpStack functions */
static void exp_push(ExpStack**, ExpNode*);
static ExpNode* exp_pop(ExpStack**);
static ExpNode* exp_top(ExpStack**);

static void
exp_push(ExpStack** stack, ExpNode* node) {
	if (!(*stack)) {
		*stack = malloc(sizeof(ExpStack));
		(*stack)->value = node;
		(*stack)->next = NULL;
		(*stack)->prev = NULL;
		return;
	}
	ExpStack* new = malloc(sizeof(ExpStack));
	new->value = node;
	new->next = NULL;
	ExpStack* i;
	for (i = *stack; i->next; i = i->next);
	i->next = new;
	new->prev = i;
}

static ExpNode*
exp_pop(ExpStack** stack) {
	if (!(*stack)) return NULL;
	ExpStack* i;
	for (i = *stack; i->next; i = i->next);
	if (!(*stack)->next) *stack = NULL;
	ExpNode* ret = i->value;
	if (i->prev) {
		i->prev->next = NULL;
	}
	//free(i);
	return ret;
}

static ExpNode*
exp_top(ExpStack** stack) {
	if (!(*stack)) return NULL;
	ExpStack* i;
	for (i = *stack; i->next; i = i->next);
	return i->value;
}

static void
compile_error(CompileState* C, const char* format, ...) {
	va_list args;
	va_start(args, format);

	printf("<");
	for (int i = 0; i < 40; i++) {
		fputc('-', stdout);	
	}
	printf(">\n\n\n*** SPYRE COMPILE-TIME ERROR ***\n\nMESSAGE:  ");
	vprintf(format, args);
	printf("\nLINE:     %d\n\n\n<", C->focus->line);
	for (int i = 0; i < 40; i++) {
		fputc('-', stdout);	
	}
	printf(">\n");

	va_end(args);
	exit(1);
}

static void
comment(CompileState* C, const char* format, ...) {
	va_list args;
	va_start(args, format);
	asmput(C, " ; -----> ");
	asmput(C, format, args);
	va_end(args);	
}

static void
token_line(CompileState* C, Token* token) {
	int index = 0;
	for (Token* i = token; i; i = i->next) {
		if ((++index) % 9 == 0) {
			asmput(C, "\n");
			comment(C, "\t");
		}
		asmput(C, "%s", i->word);
	}
}

static TreeDatatype*
raw_datatype(CompileState* C, ExpNode* node) {
	switch (node->type) {
		case EXP_FUNC_CALL:
			return node->pcall->func->return_type;
		case EXP_LITERAL:
			return node->pliteral->datatype;
		case EXP_IDENTIFIER:
			compile_error(C, "malformed datatype");
		case EXP_DATATYPE:
			return node->pdatatype;
	}
	return NULL;
}

/* finds literals and adds them to the .spys file */
static void
literal_scan(CompileState* C) {
	Token* scan[4] = {0};
	static StringList* list = NULL;
	switch (C->focus->type) {
		case NODE_IF:
			scan[0] = C->focus->pif->condition;
			break;
		case NODE_WHILE:
			scan[0] = C->focus->pwhile->condition;
			break;
		case NODE_ASSIGN:
			scan[0] = C->focus->pass->rhs;
			break;
		case NODE_RETURN:
			scan[0] = C->focus->pret->statement;
			break;
		case NODE_STATEMENT:
			scan[0] = C->focus->pstate->statement;
			break;
	}
	for (int i = 0; i < 4 && scan[i]; i++) {
		for (Token* j = scan[i]; j; j = j->next) {
			if (j->type == TOK_STRING) {
				char* saved_word = j->word;
				j->word = malloc(32);
				sprintf(j->word, STR_FORMAT, C->literal_count);
				asmput(C, 
					"let " STR_FORMAT " \"%s\"\n",
					C->literal_count++,
					saved_word	
				);
			}	
		}
	}
}

static TreeDatatype*
copy_datatype(TreeDatatype* type) {
	TreeDatatype* new = malloc(sizeof(TreeDatatype));
	memcpy(new, type, sizeof(TreeDatatype));
	return new;
}

static int
identical_types(TreeDatatype* a, TreeDatatype* b) {
	/* pointer arithmetic is valid */
	if (b->ptr_level > 0 && a->type == TYPE_INT) {
		if (b->ptr_level > 0) {
			return 1;
		}
	}
	if (a->type == TYPE_INT && b->type == TYPE_BYTE) {
		return 1;
	}
	if (b->type == TYPE_BYTE && a->type == TYPE_INT) {
		return 1;
	}
	if (a->type != b->type) {
		return 0;
	}
	if (a->type == TYPE_STRUCT && b->type == TYPE_STRUCT) {
		if (strcmp(a->pstruct->type_name, b->pstruct->type_name)) {
			return 0;
		}
	}
	if (a->ptr_level != b->ptr_level) {
		return 0;
	}
	/*
	if (a->modifier != b->modifier) {
		return 0;
	}
	*/
	return 1;
}

static TreeStruct*
find_struct(CompileState* C, const char* type_name) {
	for (TreeStruct* i = C->defined_types; i; i = i->next) {
		if (!strcmp(i->type_name, type_name)) {
			return i;
		}
	}
	return NULL;
}

static TreeDecl*
find_local(CompileState* C, const char* identifier) {
	/* first check if it is a function argument */
	if (C->func) {
		for (TreeDecl* i = C->func->arguments; i; i = i->next) {
			if (!strcmp(i->identifier, identifier)) {
				return i;	
			}
		}
	}
	/* if here, it's not a function argument, so search
	 * through the blocks upwards for the local
	 */
	TreeBlock* block = C->focus->parent_block;
	while (block) {
		for (TreeDecl* i = block->locals; i; i = i->next) {
			if (!strcmp(i->identifier, identifier)) {
				return i;	
			}
		}
		block = block->parent_node->parent_block;
	}
	return NULL;
	//compile_error(C, "undeclared identifier '%s'", identifier);
}

static void
push_instruction(CompileState* C, const char* format, ...) {
	va_list args;
	va_start(args, format);

	char* instruction = malloc(128);
	vsprintf(instruction, format, args); 

	va_end(args);

	if (!C->ins_stack) {
		InsStack* new = malloc(sizeof(InsStack));
		new->depth = C->depth;
		new->next = NULL;
		new->prev = NULL;
		new->instruction = malloc(sizeof(StringList));
		new->instruction->str = instruction;
		new->instruction->next = NULL;
		C->ins_stack = new;
		return;	
	}

	StringList* new_str = malloc(sizeof(StringList));
	new_str->str = instruction;
	new_str->next = NULL;

	InsStack* read;
	for (read = C->ins_stack; read->next; read = read->next);
	if (read->depth != C->depth) {
		/* push a new stack */
		InsStack* new_stack = malloc(sizeof(InsStack));
		new_stack->instruction = new_str;
		new_stack->depth = C->depth;
		new_stack->next = NULL;
		read->next = new_stack;
		new_stack->prev = read;
	} else {
		/* append to stack */
		StringList* list;
		for (list = read->instruction; list->next; list = list->next);
		list->next = new_str;
	}
}

static StringList*
pop_instruction(CompileState* C) {
	if (!C->ins_stack) {
		return NULL;
	}
	InsStack* read;
	StringList* ret;
	for (read = C->ins_stack; read->next; read = read->next);
	if (read->prev) {
		read->prev->next = NULL;
	} else {
		C->ins_stack = NULL;
	}
	ret = read->instruction;
	//free(read);
	return ret;	 
}

static void
asmput(CompileState* C, const char* format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(C->fout, format, args);
	va_end(args);	
}

static void
print_expression(ExpNode* expression) {
	char* out;
	for (ExpNode* i = expression; i; i = i->next) {
		out = NULL;
		switch (i->type) {
			case EXP_NOTYPE:
			case EXP_FUNC_CALL:
				out = i->pcall->func->identifier;
				break;
			case EXP_ARRAY_INDEX:
				break;
			case EXP_OPERATOR:
				out = i->poperator->word;
				break;
			case EXP_IDENTIFIER:
				out = i->pidentifier->word;
				break;
			case EXP_LITERAL:
				out = i->pliteral->word;
				break;
			case EXP_DATATYPE:
				break;
		}
		if (out) {
			if (i->type == EXP_FUNC_CALL) {
				printf("%s() ", out);
			} else {
				printf("%s ", out);
			}
		} else {
		}
	}
	printf("\n");
}

/* returns a string representing the type of C->focus */
static inline const char*
focus_tostring(CompileState* C) {
	NodeType type = C->focus->type;
	return (
		type == NODE_ROOT ? "ROOT" :
		type == NODE_IF ? "IF" :
		type == NODE_FUNCTION ? "FUNCTION" :
		type == NODE_ASSIGN ? "ASSIGNMENT" :
		type == NODE_STATEMENT ? "STATEMENT" :
		type == NODE_WHILE ? "WHILE" :
		"UNKNOWN"
	);
}

/*
 * RETURN:
 *	1 -> current node's block is empty
 *	0 -> current node's block is not empty
 */
static inline int
block_empty(CompileState* C) {
	return (
		C->focus->type == NODE_ROOT ? !C->focus->proot->block->children :
		C->focus->type == NODE_IF ? !C->focus->pif->block->children :
		C->focus->type == NODE_WHILE ? !C->focus->pwhile->block->children :
		C->focus->type == NODE_FOR ? !C->focus->pfor->block->children :
		C->focus->type == NODE_FUNCTION ? !C->focus->pfunc->block->children : 0
	);
}

/* if a struct is declared, we need to make sure that the
 * variable is really just a pointer referring to the memory
 * on the stack
 */
static void
init_declarations(CompileState* C, TreeBlock* block) {
	if (!block) return;
	for (TreeDecl* i = block->locals; i; i = i->next) {
		/* if we find a struct declaration... */
		int a, b;
		a = i->datatype->type == TYPE_STRUCT;
		b = i->datatype->ptr_level > 0;
		if ((a && !b) || (b && !a)) {
			/* the memory on the stack is one ahead of the pointer */
			asmput(C, "lea %d\n", i->offset + 1);
			asmput(C, "ilsave %d\n", i->offset);
		}
	}
	for (TreeNode* i = block->children; i; i = i->next) {
		switch (i->type) {
			case NODE_IF:
				init_declarations(C, i->pif->block);
				break;
			case NODE_WHILE:
				init_declarations(C, i->pwhile->block);
				break;
			case NODE_FUNCTION:
				init_declarations(C, i->pfunc->block);
				break;
		}
	}
}

/* RETURN: 
 *	1 -> advance success
 *	0 -> no possible advance, tree fully walked
 */
static int
advance(CompileState* C) {
	/* advances to the next node in the proper order...
	 * first priority is if the current node has a block,
	 * if so, jump into it.  Next priority is if there is
	 * another node next in the current block, if so,
	 * advance to it.  Final priority is if the current
	 * node is last in the current block.  If so, jump out of 
	 * blocks until there is a node next in the current block
	 */

	/* if we're currently in a function but it doesn't have
	 * a block, pop instructions for the return label
	 */
	if (C->ins_stack && C->focus->type == NODE_FUNCTION && !C->focus->pfunc->block) {
		for (StringList* i = pop_instruction(C); i; i = i->next) {
			asmput(C, i->str);
			if (i->str[strlen(i->str) - 1] != '\n') {
				asmput(C, "\n");
			}
		}
	}

	int should_dive = 0;
	switch (C->focus->type) {
		case NODE_ROOT:
		case NODE_IF:
		case NODE_WHILE:
		case NODE_FUNCTION:
		case NODE_FOR:
			should_dive = !block_empty(C);
			break;
	}
	if (should_dive) {
		C->depth++;
		C->focus = (
			C->focus->type == NODE_ROOT ? C->focus->proot->block->children :
			C->focus->type == NODE_IF ? C->focus->pif->block->children :
			C->focus->type == NODE_WHILE ? C->focus->pwhile->block->children :
			C->focus->type == NODE_FOR ? C->focus->pfor->block->children :
			C->focus->type == NODE_FUNCTION ? C->focus->pfunc->block->children :
			NULL
		);
		return 1;
	} else {
		switch (C->focus->type) {
			case NODE_ROOT:
			case NODE_IF:
			case NODE_WHILE:
			case NODE_FUNCTION:
			case NODE_FOR:
				for (StringList* i = pop_instruction(C); i; i = i->next) {
					asmput(C, i->str);
					if (i->str[strlen(i->str) - 1] != '\n') {
						asmput(C, "\n");
					}
				}
				break;
		}
	}
	/* if reached, attempt to move to the next node in the current block */
	if (C->focus->next) {
		C->focus = C->focus->next;
		return 1;
	}
	/* if reached, jump out of blocks until there is a next avaiable */
	while (C->focus && !C->focus->next) {
		if (!C->focus) {
			break;
		}
		if (!C->focus->parent_block) {
			break;
		}
		C->depth--;
		C->focus = C->focus->parent_block->parent_node;
		if (C->ins_stack) {
			for (StringList* i = pop_instruction(C); i; i = i->next) {
				asmput(C, i->str);
				if (i->str[strlen(i->str) - 1] != '\n') {
					asmput(C, "\n");
				}
			}
		}
	}
	if (!C->focus->next) {
		return 0;
	}
	C->focus = C->focus->next;
	return 1;
}

static ExpNode*
postfix_function_call(CompileState* C) {
	
	/* expects to be on the name of the function */
		
	/* node->pfunc will hold the function argument in postfix, initialize
	 * it to NULL for now... we need to parse ahead, find the end of the 
	 * call, and call postfix_expression again
	 */
	ExpNode* node = malloc(sizeof(ExpNode));
	node->type = EXP_FUNC_CALL;
	node->next = NULL; 
	node->pcall = malloc(sizeof(ExpFuncCall));
	node->pcall->argument = NULL; /* TBD */
	node->pcall->func = NULL;

	/* find the function */
	for (TreeNode* i = C->root->proot->block->children; i; i = i->next) {
		if (i->type == NODE_FUNCTION) {
			if (!strcmp(i->pfunc->identifier, C->token->word)) {
				node->pcall->func = i->pfunc;
				break;
			}
		}
	}
	if (!node->pcall->func) {
		compile_error(C, "attempt to call undefined function '%s'", C->token->word);
	}
	
	/* skip to first token in argument list */
	C->token = C->token->next->next;
	
	/* save the current token... we're going to scan ahead, find
	 * the last token in the argument list, and DETACH it from the 
	 * following token, so that the call we make to postfix_expression
	 * doesn't go past the end of the call.  After, C->token will be
	 * assigned to the token immediately following the function call
	 * so that conversion can continue properly
	 */
	Token* argument = C->token;

	unsigned int counter = 1;
	while (counter > 0) {
		int found = 0;
		switch (C->token->type) {
			case TOK_OPENPAR:
				found = 1;
				counter++;
				break;
			case TOK_CLOSEPAR:
				found = 1;
				counter--;
				break;
		}
		if (found && counter <= 0) break;
		C->token = C->token->next;
	}

	Token* save = C->token;
	if (C->token) {
		C->token->prev->next = NULL;
	}
	
	node->pcall->argument = postfix_expression(C, argument);
	if (save) {
		C->token = save;
		save->prev->next = save;
	}

	return node;	
}

/* wraps a Token expression into an ExpNode expression
 * and converts it from infix to postfix
 */
static ExpNode*
postfix_expression(CompileState* C, Token* expression) {

	static const ExpOperator ops[256] = {
		/* 1 = left, 2 = right */
		[TOK_COMMA]			= {1, ASSOC_LEFT},
		[TOK_ASSIGN]		= {3, ASSOC_LEFT},
		[TOK_LOGAND]		= {3, ASSOC_LEFT},
		[TOK_LOGOR]			= {3, ASSOC_LEFT},
		[TOK_EQ]			= {4, ASSOC_LEFT},
		[TOK_NOTEQ]			= {4, ASSOC_LEFT},
		[TOK_GT]			= {6, ASSOC_LEFT},
		[TOK_GE]			= {6, ASSOC_LEFT},
		[TOK_LT]			= {6, ASSOC_LEFT},
		[TOK_LE]			= {6, ASSOC_LEFT},
		[TOK_LINE]			= {7, ASSOC_LEFT},
		[TOK_SHL]			= {7, ASSOC_LEFT},
		[TOK_SHR]			= {7, ASSOC_LEFT},
		[TOK_PLUS]			= {8, ASSOC_LEFT},
		[TOK_HYPHON]		= {8, ASSOC_LEFT},
		[TOK_ASTER]			= {9, ASSOC_LEFT},
		[TOK_PERCENT]		= {9, ASSOC_LEFT},
		[TOK_FORSLASH]		= {9, ASSOC_LEFT},
		[TOK_AMPERSAND]		= {10, ASSOC_RIGHT},
		[TOK_UPCARROT]		= {10, ASSOC_RIGHT},
		[TOK_EXCL]			= {10, ASSOC_RIGHT},
		[TOK_PERIOD]		= {11, ASSOC_LEFT}
	};

	/* implement shunting yard algorithm for expressions */
	ExpStack* postfix = NULL;	
	ExpStack* operators = NULL;
	ExpNode* node;
	for (C->token = expression; C->token; C->token = C->token->next) {
		if (C->token->next && C->token->type == TOK_IDENTIFIER && C->token->next->type == TOK_OPENPAR) {
			node = postfix_function_call(C);
			exp_push(&postfix, node);
		} else if (C->token->type == TOK_OPENPAR) {
			node = malloc(sizeof(ExpNode));
			node->type = EXP_OPERATOR;
			node->poperator = C->token;
			node->next = NULL;
			exp_push(&operators, node);
		/* assoc is used to see if it exists, because all operators
		 * should have a non-zero assoc
		 */
		} else if (ops[C->token->type].assoc) {
			while (
				operators
				&& exp_top(&operators)->poperator->type != TOK_OPENPAR
				&& (
					(
						/* left-assoc */
						ops[C->token->type].assoc == 1 
						&& ops[C->token->type].pres <= ops[exp_top(&operators)->poperator->type].pres
					)
					|| 
					(
						/* right-assoc */
						ops[C->token->type].assoc == 2
						&& ops[C->token->type].pres < ops[exp_top(&operators)->poperator->type].pres
					)
				)
			) {
				exp_push(&postfix, exp_pop(&operators));
			}
			node = malloc(sizeof(ExpNode));	
			node->type = EXP_OPERATOR;
			node->poperator = C->token;
			node->next = NULL;
			exp_push(&operators, node);
		} else if (C->token->type == TOK_IDENTIFIER) {
			node = malloc(sizeof(ExpNode));
			node->type = EXP_IDENTIFIER;
			node->pidentifier = C->token; // find_local(C, C->token->word);
			node->next = NULL;
			exp_push(&postfix, node);
		} else if (
			C->token->type == TOK_INT 
			|| C->token->type == TOK_FLOAT
			|| C->token->type == TOK_STRING
		) {
			TreeDatatype* data = malloc(sizeof(TreeDatatype));
			data->type = (
				C->token->type == TOK_INT ? TYPE_INT :
				C->token->type == TOK_FLOAT ? TYPE_FLOAT :
				C->token->type == TOK_STRING ? TYPE_STRING : TYPE_NOTYPE
			);
			data->pstruct = NULL;
			data->ptr_level = 0;
			data->modifier = 0;
			node = malloc(sizeof(ExpNode));
			node->type = EXP_LITERAL;
			node->pliteral = malloc(sizeof(ExpLiteral));
			node->pliteral->word = C->token->word;
			node->pliteral->datatype = data;
			node->next = NULL;
			exp_push(&postfix, node);
		} else if (C->token->type == TOK_CLOSEPAR) {
			while (operators && exp_top(&operators)->poperator->type != TOK_OPENPAR) {
				exp_push(&postfix, exp_pop(&operators));	
			}
			exp_pop(&operators);
		}
		if (!C->token) break;
	}

	while (operators) {
		exp_push(&postfix, exp_pop(&operators));
	}
	
	/* the ExpNodes on the stack aren't linked yet (they're currently
	 * linked by the stack...)
	 * link them together by going through the stack
	 */
	for (ExpStack* i = postfix; i && i->next; i = i->next) {
		i->value->next = i->next->value;
	}
	
	return postfix ? postfix->value : NULL;
}

/* converts an expression in postfix to bytecode...
 * use postfix_expression before calling to convert an
 * infix expression to a postfix expression 
 */
static ExpNode* 
generate_expression(CompileState* C, ExpNode* expression, int is_lhs) {

	/* used for typechecking, finding fields in structs, etc */
	ExpStack* stack = NULL;
	ExpNode* pop[2];

	int last_der = 0;
	for (ExpNode* node = expression; node; node = node->next) {
		if (!node->next) {
			last_der = node->type == EXP_OPERATOR && node->poperator->type == TOK_UPCARROT;
		}
	}

	for (ExpNode* node = expression; node; node = node->next) {
		int is_2last = node->next && !node->next->next;
		switch (node->type) {
			case EXP_LITERAL: {
				switch (node->pliteral->datatype->type) {
					case TYPE_INT:
					case TYPE_STRING:
						C->target(C, "ipush %s\n", node->pliteral->word);
						break;
					case TYPE_FLOAT:
						C->target(C, "fpush %s\n", node->pliteral->word);
						break;
				}
				ExpNode* push = malloc(sizeof(ExpNode));
				push->type = EXP_DATATYPE;
				push->next = NULL;
				/* push a char pointer if it's a string */
				if (node->pliteral->datatype->type == TYPE_STRING) {
					push->pdatatype = malloc(sizeof(TreeDatatype));
					push->pdatatype->type = TYPE_BYTE;
					push->pdatatype->ptr_level = 1;
					push->pdatatype->modifier = 0;
					push->pdatatype->pstruct = NULL;		
				} else {
					push->pdatatype = node->pliteral->datatype;
				}
				exp_push(&stack, push);
				break;
			}
			case EXP_FUNC_CALL: {
				int n_call_args = 0;
				TreeFunction* func = node->pcall->func;
				/* load function arguments onto the stack */
				ExpNode* ret = generate_expression(C, node->pcall->argument, 0);
				for (ExpNode* i = ret; i; i = i->next) {
					n_call_args++;
				}
				if (n_call_args != func->nargs && !func->is_vararg) {
					compile_error(C,
						"incorrect number of arguments passed to function '%s', expected %d, got %d",
						func->identifier,
						func->nargs,
						n_call_args
					);
				}

				int at_arg = 1;
				TreeDecl* expected_type = func->arguments;
				while (expected_type && ret) {
					if (expected_type->datatype->type == TYPE_INT && ret->pdatatype->type == TYPE_FLOAT) {
						C->target(C, "ftoi %d\n", n_call_args - at_arg);
					} else if (expected_type->datatype->type == TYPE_FLOAT && ret->pdatatype->type == TYPE_INT) {
						C->target(C, "itof %d\n", n_call_args - at_arg);
					} else if (!identical_types(expected_type->datatype, ret->pdatatype)) {
						compile_error(C,
							"passing incorrect type argument (#%d) to function '%s', expected (%s), got (%s)",
							at_arg,
							func->identifier,
							tostring_datatype(expected_type->datatype),
							tostring_datatype(ret->pdatatype)
						);
					}
					expected_type = expected_type->next;
					ret = ret->next;
					at_arg++;
				}

				if (func->is_cfunc) {
					C->target(C, "ccall " CFUNC_FORMAT ", %d\n", func->identifier, n_call_args); 
				} else {
					C->target(C, "call " FUNC_FORMAT ", %d\n", func->identifier, n_call_args);
				}

				ExpNode* push = malloc(sizeof(ExpNode));
				push->type = EXP_DATATYPE;
				push->pdatatype = copy_datatype(func->return_type);
				push->next = NULL;
				exp_push(&stack, push);

				break;
			}
			case EXP_ARRAY_INDEX:
				break;
			case EXP_IDENTIFIER: {
				TreeDecl* local = find_local(C, node->pidentifier->word);
				int next_period = (
					node->next
					&& node->next->type == EXP_OPERATOR
					&& node->next->poperator->type == TOK_PERIOD
				);
				int next_ampersand = (
					node->next
					&& node->next->type == EXP_OPERATOR
					&& node->next->poperator->type == TOK_AMPERSAND
				);
				ExpNode* push = malloc(sizeof(ExpNode));
				push->next = NULL;
				push->type = EXP_DATATYPE;
				/* if a local is found and the next thing isn't a period, it's a variable */
				if (local && !next_period) {
					push->pdatatype = copy_datatype(local->datatype);
					/* if it's a struct load its address */
					if (local->datatype->type == TYPE_STRUCT) {
						if ((is_lhs && local->datatype->ptr_level >= 1) || next_ampersand) {
							C->target(C, "lea %d\n", local->offset);
						} else {
							C->target(C, "ilload %d\n", local->offset);	
						}
					/* otherwise load its value */
					} else {
						/* next_ampersand cancels a dereference, so does is_lhs */
						if (next_ampersand && local->datatype->ptr_level <= 0) {
							C->target(C, "lea %d\n", local->offset);
						} else {
							switch (local->datatype->type) {
								case TYPE_INT:
									if (is_lhs && local->datatype->ptr_level <= 0 && !last_der) {
										C->target(C, "lea %d\n", local->offset);
									} else {
										C->target(C, "ilload %d\n", local->offset);
									}
									break;
								case TYPE_BYTE:
									if (is_lhs) {
										C->target(C, "lea %d\n", local->offset);
									} else {
										C->target(C, "ilload %d\n", local->offset);
									}
									break;
								case TYPE_FLOAT:
									if (is_lhs && local->datatype->ptr_level <= 0 && !last_der) {
										C->target(C, "lea %d\n", local->offset);	
									} else {
										C->target(C, "flload %d\n", local->offset);
									}
									break;
							}
						}
					}
				/* its a member of a struct or an undeclared identifier */
				} else {
					ExpNode* top = exp_top(&stack);
					if (!top) {
						compile_error(C, "unexpected token '.'");
					}
					if (top->type == EXP_LITERAL) {
						compile_error(C, "the '.' operator can't be used on a literal");
					}
					int found_member = 0;
					if (top->type == EXP_DATATYPE && top->pdatatype->type == TYPE_STRUCT) {
						for (TreeDecl* i = top->pdatatype->pstruct->children; i; i = i->next) {
							if (!strcmp(i->identifier, node->pidentifier->word)) {
								found_member = 1;
								break;
							}
						}
					}
					if (!found_member) {
						compile_error(C, "undeclared identifier '%s'", node->pidentifier->word);
					}
					/* because we know it's a member of a struct, we want to repush the identifier */
					push->type = EXP_IDENTIFIER;
					push->pidentifier = node->pidentifier;
				}
				exp_push(&stack, push);
				break;
			}
			case EXP_OPERATOR: {
				if (node->poperator->type == TOK_COMMA) {
					break;
				}
				if (node->poperator->type != TOK_UPCARROT && node->poperator->type != TOK_AMPERSAND
					&& node->poperator->type != TOK_EXCL) {
					for (int i = 0; i < 2; i++) {
						if (!stack) {
							compile_error(C, "malformed expression");
						}
						pop[i] = exp_pop(&stack);
					}
				}
				if (node->poperator->type == TOK_PERIOD) {
					/* dereferencing a struct.... we already made sure that B is
					 * a member of A (assuming form A.B)
					 */
					if (!(pop[1]->type == EXP_DATATYPE && pop[1]->pdatatype->type == TYPE_STRUCT)) {
						compile_error(C, "the '.' operator can only be used on structs");
					}
					if (pop[1]->pdatatype->ptr_level > 0) {
						compile_error(C, "attempt to use '.' operator on a pointer to a struct.");
					}
					int next_ampersand = (
						node->next
						&& node->next->type == EXP_OPERATOR
						&& node->next->poperator->type == TOK_AMPERSAND
					);
					/* search through the struct for the member */
					ExpNode* push = malloc(sizeof(ExpNode));
					push->type = EXP_DATATYPE;
					push->next = NULL;
					for (TreeDecl* i = pop[1]->pdatatype->pstruct->children; i; i = i->next) {
						if (!strcmp(i->identifier, pop[0]->pidentifier->word)) {
							char prefix = i->datatype->type == TYPE_FLOAT ? 'f' : 'i';
							push->pdatatype = copy_datatype(i->datatype);
							C->target(C, "icinc %d\n", i->offset * 8);
							/* don't dereference if its lhs, ampersand next, or a struct */
							if (!is_lhs && !next_ampersand && i->datatype->type != TYPE_STRUCT) {
								C->target(C, "%cder\n", prefix);
							}
							break;
						}
					}
					exp_push(&stack, push);
				} else if (node->poperator->type == TOK_AMPERSAND) {
					pop[0] = exp_pop(&stack);
					TreeDatatype* newtype = copy_datatype(pop[0]->pdatatype);
					if (pop[0]->type == EXP_DATATYPE) {
						newtype->ptr_level++;
						pop[0]->pdatatype = newtype;
					}
					exp_push(&stack, pop[0]);
				} else if (node->poperator->type == TOK_EXCL) {
					pop[0] = exp_pop(&stack);
					pop[0]->type = EXP_DATATYPE;
					pop[0]->pdatatype = malloc(sizeof(TreeDatatype));
					pop[0]->pdatatype->type = TYPE_INT;
					pop[0]->pdatatype->ptr_level = 0;
					pop[0]->pdatatype->modifier = 0;
					pop[0]->pdatatype->pstruct = NULL;
					C->target(C, "lnot\n");
					exp_push(&stack, pop[0]);
				} else if (node->poperator->type == TOK_UPCARROT) {
					pop[0] = exp_pop(&stack);
					if (pop[0]->type == EXP_DATATYPE) {
						TreeDatatype* newtype = copy_datatype(pop[0]->pdatatype);
						if (newtype->ptr_level <= 0) {
							compile_error(C, "attempt to dereference a non-pointer");
						}
						newtype->ptr_level--;
						if (is_lhs && !node->next) {
							pop[0]->pdatatype = newtype;
							exp_push(&stack, pop[0]);;
							break;
						}
						switch (newtype->type) {
							case TYPE_BYTE:
								C->target(C, "cder\n");
								break;
							case TYPE_INT:
								C->target(C, "ider\n");
								break;
							case TYPE_FLOAT:
								C->target(C, "fder\n");
								break;
							default:
								/* if we're assigning to a single struct pointer (LHS),
								 * DEREFERENCE TWICE!
								 */
								if (is_lhs && newtype->type == TYPE_STRUCT) {
									C->target(C, "ider\n");	
								}
								C->target(C, "ider\n");
								break;
						}
						pop[0]->pdatatype = newtype;
					}
					exp_push(&stack, pop[0]);
				} else {
					ExpNode* push;
					TreeDatatype* a = raw_datatype(C, pop[0]);
					TreeDatatype* b = raw_datatype(C, pop[1]);
					if (a->type == TYPE_INT && b->type == TYPE_FLOAT) {
						a->type = TYPE_FLOAT;
						C->target(C, "itof 0\n");
					} else if (a->type == TYPE_FLOAT && b->type == TYPE_INT) {
						b->type = TYPE_FLOAT;
						C->target(C, "itof 1\n");
					} if (!identical_types(a, b)) {
						compile_error(C,
							"attempt to perform arithmetic on non-matching types '%s' and '%s'",
							tostring_datatype(a),
							tostring_datatype(b)
						);
					}
					const char prefix = (
						a->type == TYPE_INT ? 'i' :
						a->type == TYPE_STRING ? 'i' : 
						a->type == TYPE_BYTE ? 'i' : 
						a->type == TYPE_STRUCT ? 'i' : 'f'
					);	
					push = malloc(sizeof(ExpNode));
					push->type = EXP_DATATYPE;
					/* b if arithmetic pointer */
					int mul_size = 0;
					if (b->ptr_level > 0 && a->type == TYPE_INT) {
						push->pdatatype = b;
						if (b->type == TYPE_STRUCT) {
							if (b->ptr_level > 0) {
								mul_size = 8;
							} else {
								mul_size = 8 * b->pstruct->size;
							}
						} else {
							mul_size = 8;
						}
					} else {
						push->pdatatype = a;
					}
					if (b->type != TYPE_BYTE && mul_size && (node->poperator->type == TOK_PLUS || node->poperator->type == TOK_HYPHON)) {
						C->target(C, "ipush %d\nimul ; ----> pointer arithmetic\n", mul_size);
					}
					push->next = NULL;
					/* A and B are identical, push back */
					switch (node->poperator->type) {
						case TOK_PLUS:
							C->target(C, "%cadd\n", prefix);
							break;
						case TOK_HYPHON:
							C->target(C, "%csub\n", prefix);
							break;
						case TOK_ASTER:
							C->target(C, "%cmul\n", prefix);
							break;
						case TOK_FORSLASH:
							C->target(C, "%cdiv\n", prefix);
							break;
						case TOK_GT:
							C->target(C, "%cgt\n", prefix);
							break;
						case TOK_GE:
							C->target(C, "%cge\n", prefix);
							break;
						case TOK_LT:
							C->target(C, "%clt\n", prefix);
							break;
						case TOK_LE:
							C->target(C, "%cle\n", prefix);
							break;
						case TOK_EQ:
							C->target(C, "%ccmp\n", prefix);
							break;
						case TOK_LOGAND:
							push->pdatatype->type = TYPE_INT;
							C->target(C, "land\n");
							break;
						case TOK_EXCL:
							push->pdatatype->type = TYPE_INT;
							C->target(C, "lnot\n");
							break;
						case TOK_LOGOR:
							push->pdatatype->type = TYPE_INT;
							C->target(C, "lor\n");
							break;
					}
					exp_push(&stack, push);
				}
			}
		}
	}

	/* patch together */
	for (ExpStack* i = stack; i->next; i = i->next) {
		i->value->next = i->next->value;
	}
	
	return stack->value;
}

static void
generate_if(CompileState* C) {	
	if (C->focus->pif->if_type == IF_REG) {
		C->if_label = C->label_count++;
	}
	unsigned int label = C->label_count++;
	comment(C, "");
	switch (C->focus->pif->if_type) {
		case IF_REG:
			asmput(C, "if ( ");
			token_line(C, C->focus->pass->lhs);
			asmput(C, ") {\n");
			break;
		case IF_ELIF:
			asmput(C, "elif ( ");
			token_line(C, C->focus->pass->lhs);
			asmput(C, ") {\n");
			break;
		case IF_ELSE:
			asmput(C, "else {\n");
			break;
	}
	if (C->focus->pif->if_type != IF_ELSE && C->focus->next && C->focus->next->type == NODE_IF) {
		generate_expression(C, postfix_expression(C, C->focus->pif->condition), 0);
		asmput(C, JZ_LABEL "\n", label);
		push_instruction(C, JMP_LABEL, C->if_label);
		push_instruction(C, DEF_LABEL, label);
	} else if (C->focus->pif->if_type == IF_REG) {
		generate_expression(C, postfix_expression(C, C->focus->pif->condition), 0);
		asmput(C, JZ_LABEL "\n", label);
		push_instruction(C, DEF_LABEL, label);
	} else {
		push_instruction(C, DEF_LABEL, C->if_label);
	}
}

static void
generate_while(CompileState* C) {
	unsigned int top_label = C->label_count++;
	unsigned int bottom_label = C->label_count++;
	C->top_label = top_label;
	C->bottom_label = bottom_label;
	comment(C, "");
	asmput(C, "while ( ");
	token_line(C, C->focus->pass->lhs);
	asmput(C, ") {\n");
	asmput(C, DEF_LABEL "\n", top_label);
	generate_expression(C, postfix_expression(C, C->focus->pwhile->condition), 0);
	asmput(C, JZ_LABEL "\n", bottom_label);
	push_instruction(C, JMP_LABEL, top_label);
	push_instruction(C, DEF_LABEL, bottom_label);
}

static void
generate_for(CompileState* C) {
	TreeNode* save = C->focus;
	C->focus = save->pfor->init;
	C->target = asmput;
	if (C->focus) {
		switch (C->focus->type) {
			case NODE_ASSIGN:
				generate_assignment(C);
				break;
			case NODE_STATEMENT:
				generate_statement(C);
				break;
		}
	}
	unsigned int top_label = C->label_count++;
	unsigned int bottom_label = C->label_count++;
	asmput(C, DEF_LABEL "\n", top_label);
	C->focus = save;
	generate_expression(C, postfix_expression(C, C->focus->pfor->condition), 0);
	asmput(C, JZ_LABEL "\n", bottom_label);
	C->focus = C->focus->pfor->statement;
	C->target = push_instruction;
	if (C->focus) {
		switch (C->focus->type) {
			case NODE_ASSIGN:
				generate_assignment(C);
				break;
			case NODE_STATEMENT:
				generate_statement(C);
				break;
		}
	}
	C->target = asmput;
	C->focus = save;
	push_instruction(C, JMP_LABEL, top_label);
	push_instruction(C, DEF_LABEL, bottom_label);
}

static void
generate_function_decl(CompileState* C) {
	/* don't generate code for a C function */
	if (C->focus->pfunc->is_cfunc) {
		return;
	}
	asmput(C, "\n\n" DEF_FUNC "\n", C->focus->pfunc->identifier);
	C->return_label = C->label_count++;
	C->func = C->focus->pfunc;

	/* reserve space for locals.... TODO currently locals
	 * have a maximum size of 1, but structs will have
	 * larger sizes... make sure to allocate enough space
	 * for them in the future
	 */
	asmput(C, "res %d\n", C->focus->pfunc->reserve_space);
	
	/* push the arguments and assign them to their proper offset */	
	for (int i = 0; i < C->focus->pfunc->nargs; i++) {
		asmput(C, "iarg %d\n", i);	
		asmput(C, "ilsave %d\n", i);
	}
	
	if (C->focus->pfunc->block->children) {
		init_declarations(C, C->focus->pfunc->block);
		push_instruction(C, DEF_LABEL, C->return_label);
		push_instruction(C, "iret\n");
	} else {
		asmput(C, DEF_LABEL, C->return_label);
		asmput(C, "iret\n");
	}
}

static void
generate_assignment(CompileState* C) {

	ExpNode* lhs = generate_expression(C, postfix_expression(C, C->focus->pass->lhs), 1);
	ExpNode* rhs = generate_expression(C, postfix_expression(C, C->focus->pass->rhs), 0);

	/* typecheck */
	if (!(lhs->type == EXP_DATATYPE && rhs->type == EXP_DATATYPE)) {
		compile_error(C, "didn't pop LHS and RHS as datatypes...");
	}
	
	/* implicit cast if needed */
	if (lhs->pdatatype->type == TYPE_FLOAT && rhs->pdatatype->type == TYPE_INT) {
		C->target(C, "itof 0\n");
	} else if (lhs->pdatatype->type == TYPE_INT && rhs->pdatatype->type == TYPE_FLOAT) {
		C->target(C, "ftoi 0\n");	
	} else if (!identical_types(lhs->pdatatype, rhs->pdatatype)) {
		compile_error(C,
			"attempt to assign '%s' to '%s'",
			tostring_datatype(rhs->pdatatype),
			tostring_datatype(lhs->pdatatype)
		);
	}
	
	if (lhs->pdatatype->type == TYPE_FLOAT) {
		C->target(C, "fsave\n");
	} else {
		C->target(C, "isave\n");
	}
}

static void
generate_statement(CompileState* C) {
	generate_expression(C, postfix_expression(C, C->focus->pstate->statement), 0);
}

static void
generate_return(CompileState* C) {
	comment(C, "return ");
	token_line(C, C->focus->pret->statement);
	asmput(C, "\n");
	ExpNode* ret = generate_expression(C, postfix_expression(C, C->focus->pret->statement), 0);
	if (C->func->return_type->type == TYPE_INT && ret->pdatatype->type == TYPE_FLOAT) {
		asmput(C, "ftoi 0\n");
	} else if (C->func->return_type->type == TYPE_FLOAT && ret->pdatatype->type == TYPE_INT) {
		asmput(C, "itof 0\n");
	} else if (!identical_types(ret->pdatatype, C->func->return_type)) {
		compile_error(C,
			"attempt to return expression of type (%s) from function (%s),"
			"expected expression of type (%s)",
			tostring_datatype(ret->pdatatype),
			C->func->identifier,
			tostring_datatype(C->func->return_type)
		);
	}	
	asmput(C, JMP_LABEL "\n", C->return_label);
}

void
generate_bytecode(ParseState* P, const char* fout_name) {
	
	CompileState* C = malloc(sizeof(CompileState));
	C->defined_types = P->defined_types;
	C->root = P->root; 
	C->focus = P->root;
	C->fout = fopen(fout_name, "wb");
	C->ins_stack = NULL;
	C->depth = 0;
	C->label_count = 0;
	C->literal_count = 0;
	C->return_label = 0;
	C->if_label = 0;
	C->top_label = 0;
	C->bottom_label = 0;
	C->token = NULL;
	C->target = asmput;
	if (!C->fout) {
		printf("couldn't open output file '%s' for writing\n", fout_name);
		exit(1);
	}

	int advance_success = 1;
	
	/* first walk the tree and look for cfunc declarations */
	while (advance_success) {
		/* check for c func */
		if (C->focus->type == NODE_FUNCTION && C->focus->pfunc->is_cfunc) {
			asmput(C, 
				"let " CFUNC_FORMAT " \"%s\"\n",
				C->focus->pfunc->identifier,
				C->focus->pfunc->identifier
			);	
		/* check for string literal */
		} else {
			literal_scan(C);
		}
		advance_success = advance(C);
	}

	advance_success = 1;
	C->focus = P->root;
	asmput(C, "jmp __ENTRY_POINT__\n");

	while (advance_success) {
		switch (C->focus->type) {
			case NODE_ROOT:
				break;
			case NODE_IF:
				generate_if(C);
				break;
			case NODE_WHILE:
				generate_while(C);
				break;
			case NODE_FOR:
				generate_for(C);
				break;
			case NODE_FUNCTION:
				generate_function_decl(C);
				break;
			case NODE_ASSIGN:
				generate_assignment(C);
				break;
			case NODE_STATEMENT:
				generate_statement(C);
				break;
			case NODE_RETURN:
				generate_return(C);
				break;
			case NODE_CONTINUE:
				asmput(C, JMP_LABEL "\n", C->top_label);
				break;
			case NODE_BREAK:
				asmput(C, JMP_LABEL "\n", C->bottom_label);
				break;
		}
		advance_success = advance(C);
	}
	
	asmput(C, "__ENTRY_POINT__:\n");
	asmput(C, "call __FUNC__main, 0\n");
	fclose(C->fout);

}
