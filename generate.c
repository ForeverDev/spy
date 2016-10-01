#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "generate.h"

#define LABEL_FORMAT "__LABEL__%u"
#define FUNC_FORMAT "__FUNC__%s"
#define CFUNC_FORMAT "__CFUNC__%s"
#define DEF_FUNC FUNC_FORMAT ":"
#define DEF_LABEL LABEL_FORMAT ":"
#define JZ_LABEL "jz " LABEL_FORMAT
#define JNZ_LABEL "jnz " LABEL_FORMAT
#define JMP_LABEL "jmp " LABEL_FORMAT

typedef struct ExpNode ExpNode;
typedef struct ExpStack ExpStack;
typedef struct ExpOperator ExpOperator;
typedef struct ExpFuncCall ExpFuncCall;
typedef enum ExpType ExpType;

enum ExpType {
	EXP_NOTYPE,
	EXP_FUNC_CALL,
	EXP_ARRAY_INDEX,
	EXP_LITERAL,
	EXP_VARIABLE,
	EXP_OPERATOR,
	EXP_FIELD,
	EXP_IDENTIFIER,
	EXP_STRUCT,
	EXP_DATATYPE
};

struct ExpOperator {
	unsigned int pres;
	unsigned int assoc;
};

struct ExpFuncCall {
	char* identifier;
	ExpNode* argument;
};

struct ExpNode {
	ExpType type;
	ExpNode* next;
	union {
		Token* pliteral;
		Token* poperator;
		Token* pidentifier;
		TreeDecl* pvariable;
		TreeDatatype* pdatatype;
		TreeStruct* pstruct;
		ExpFuncCall* pfunc;
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
static void write(CompileState*, const char*, ...);
static TreeDecl* find_local(CompileState*, const char*);
static void compile_error(CompileState*, const char*, ...);
static TreeStruct* type_defined(CompileState*, const char*);

/* generating (etc) functions */
static void generate_expression(CompileState*, ExpNode*);
static void generate_if(CompileState*);
static void generate_while(CompileState*);
static void generate_function_decl(CompileState*);
static void generate_assignment(CompileState*);
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

static TreeStruct*
find_type(CompileState* C, const char* type_name) {
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
write(CompileState* C, const char* format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(C->fout, format, args);
	va_end(args);	
}

static void
print_expression(ExpNode* expression) {
	char* out;
	for (ExpNode* i = expression; i; i = i->next) {
		switch (i->type) {
			case EXP_NOTYPE:
			case EXP_FUNC_CALL:
			case EXP_ARRAY_INDEX:
				break;
			case EXP_OPERATOR:
				out = i->poperator->word;
				break;
			case EXP_VARIABLE:
				out = i->pvariable->identifier;
				break;
			case EXP_LITERAL:	
				out = i->pliteral->word;
				break;
			case EXP_IDENTIFIER:
				out = i->pidentifier->word;
				break;
			case EXP_STRUCT:
				out = i->pstruct->type_name;
				break;
		}
		printf("%s ", out);
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
		C->focus->type == NODE_FUNCTION ? !C->focus->pfunc->block->children : 0
	);
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
	int should_dive = 0;
	switch (C->focus->type) {
		case NODE_ROOT:
		case NODE_IF:
		case NODE_WHILE:
		case NODE_FUNCTION:
			should_dive = !block_empty(C);
			break;
	}
	if (should_dive) {
		C->depth++;
		C->focus = (
			C->focus->type == NODE_ROOT ? C->focus->proot->block->children :
			C->focus->type == NODE_IF ? C->focus->pif->block->children :
			C->focus->type == NODE_WHILE ? C->focus->pwhile->block->children :
			C->focus->type == NODE_FUNCTION ? C->focus->pfunc->block->children :
			NULL
		);
		return 1;
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
				write(C, i->str);
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
	node->pfunc = malloc(sizeof(ExpFuncCall));
	node->pfunc->identifier = C->token->word;
	node->pfunc->argument = NULL; /* TBD */
	
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
		switch (C->token->type) {
			case TYPE_OPENPAR:
				counter++;
				break;
			case TYPE_CLOSEPAR:
				counter--;
				break;
		}
		C->token = C->token->next;
	}
	
	/* if necessary, detach */
	if (C->token && C->token->next) {
		Token* save = C->token->next;
		C->token->next->prev = NULL;
		C->token->next = NULL;
		C->token = save;
	}
	
}

/* wraps a Token expression into an ExpNode expression
 * and converts it from infix to postfix
 */
static ExpNode*
postfix_expression(CompileState* C, Token* expression) {

	static const ExpOperator ops[256] = {
		[TYPE_COMMA]		= {1, 1},
		[TYPE_ASSIGN]		= {2, 1},
		[TYPE_LOGAND]		= {2, 1},
		[TYPE_LOGOR]		= {2, 1},
		[TYPE_EQ]			= {3, 1},
		[TYPE_NOTEQ]		= {3, 1},
		[TYPE_PERIOD]		= {4, 1},
		[TYPE_GT]			= {5, 1},
		[TYPE_GE]			= {5, 1},
		[TYPE_LT]			= {5, 1},
		[TYPE_LE]			= {5, 1},
		[TYPE_AMPERSAND]	= {6, 1},
		[TYPE_LINE]			= {6, 1},
		[TYPE_XOR]			= {6, 1},
		[TYPE_SHL]			= {6, 1},
		[TYPE_SHR]			= {6, 1},
		[TYPE_PLUS]			= {7, 1},
		[TYPE_HYPHON]		= {7, 1},
		[TYPE_ASTER]		= {8, 1},
		[TYPE_PERCENT]		= {8, 1},
		[TYPE_FORSLASH]		= {8, 1},
		[TYPE_PERIOD]		= {9, 1}
	};
	
	/* implement shunting yard algorithm for expressions */
	ExpStack* postfix = NULL;	
	ExpStack* operators = NULL;
	ExpNode* node;
	for (C->token = expression; C->token; C->token = C->token->next) {
		if (C->token->next && C->token->type == TYPE_IDENTIFIER && C->token->next->type == TYPE_OPENPAR) {
			node = postfix_function_call(C);
			exp_push(&postfix, node);
			if (!C->token) {
				break;
			}
		} else if (C->token->type == TYPE_OPENPAR) {
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
				&& exp_top(&operators)->poperator->type != TYPE_OPENPAR
				&& ops[C->token->type].pres <= ops[exp_top(&operators)->poperator->type].pres
			) {
				exp_push(&postfix, exp_pop(&operators));
			}
			node = malloc(sizeof(ExpNode));	
			node->type = EXP_OPERATOR;
			node->poperator = C->token;
			node->next = NULL;
			exp_push(&operators, node);
		} else if (C->token->type == TYPE_IDENTIFIER) {
			node = malloc(sizeof(ExpNode));
			node->type = EXP_IDENTIFIER;
			node->pidentifier = C->token; // find_local(C, C->token->word);
			node->next = NULL;
			exp_push(&postfix, node);
		} else if (C->token->type == TYPE_NUMBER) {
			node = malloc(sizeof(ExpNode));
			node->type = EXP_LITERAL;
			node->pliteral = C->token;
			node->next = NULL;
			exp_push(&postfix, node);
		} else if (C->token->type == TYPE_CLOSEPAR) {
			while (operators && exp_top(&operators)->poperator->type != TYPE_OPENPAR) {
				exp_push(&postfix, exp_pop(&operators));	
			}
			exp_pop(&operators);
		}
	}

	while (operators) {
		exp_push(&postfix, exp_pop(&operators));
	}
	
	/* the ExpNodes on the stack aren't linked yet (they're currently
	 * linked by the stack...)
	 * link them together by going through the stack
	 */
	for (ExpStack* i = postfix; i->next; i = i->next) {
		i->value->next = i->next->value;
	}

	return postfix->value;
}

/* converts an expression in postfix to bytecode...
 * use postfix_expression before calling to convert an
 * infix expression to a postfix expression 
 */
static void
generate_expression(CompileState* C, ExpNode* expression) {

	/* used for typechecking, finding fields in structs, etc */
	ExpStack* stack = NULL;
	ExpNode* pop[2];

	print_expression(expression);
		
	for (ExpNode* i = expression; i; i = i->next) {
		switch (i->type) {
			case EXP_LITERAL:
				exp_push(&stack, i);
				write(C, "ipush %s\n", i->pliteral->word);
				break;
			case EXP_VARIABLE:
				exp_push(&stack, i);
				write(C, "ilload %d\n", i->pvariable->offset);
				break;
			case EXP_IDENTIFIER: {
				TreeDecl* local = find_local(C, i->pidentifier->word);
				ExpNode* node = malloc(sizeof(ExpNode));
				node->next = NULL;
				if (local) {
					TreeStruct* type = find_type(C, local->datatype->type_name);
					if (type) {
						node->type = EXP_STRUCT;
						node->pstruct = type;
						write(C, "ilea %d\n", local->offset);
					} else {	
						node->type = EXP_DATATYPE;
						node->pdatatype = local->datatype;
						write(C, "ilload %d\n", local->offset);
					}
				} else {
				/* if it's not a local, it is either a member of a struct
				 * or an undeclared identifier.... if it IS a member, it
				 * must be a member of the struct on the top of the stack
				 * so, we can pull the struct off the top and check if it
				 * is a member
				 */
					ExpNode* top = exp_top(&stack);
					int found_member = 0;
					if (!top || top->type != EXP_STRUCT) {
						compile_error(C, "the '.' operator can only be used on a struct");	
					}
					for (TreeDecl* j = top->pstruct->children; j; j = j->next) {
						if (!strcmp(j->identifier, i->pidentifier->word)) {
							/* no need to generate any code yet, it will
							 * be handled by the . operator */
							found_member = 1;
							/* the type we push is insignificant because the .
							 * operator does all the checking... just push it
							 * as an identifier for now 
							 */
							node->type = EXP_IDENTIFIER;
							node->pidentifier = i->pidentifier;
							break;
						}	
					}
					if (!found_member) {
						compile_error(C, "undeclared identifier '%s'", i->pidentifier->word);
					}
				}
				exp_push(&stack, node);
				break;
			}
			case EXP_OPERATOR: 
				for (int i = 0; i < 2; i++) {
					if (!stack) {
						compile_error(C, "malformed expression");
					}
					pop[i] = exp_pop(&stack);
				}
				switch (i->poperator->type) {
					case TYPE_EQ:
						write(C, "icmp\n");
						goto arith_typecheck;
					case TYPE_PLUS:
						write(C, "iadd\n");
						goto arith_typecheck;
					case TYPE_HYPHON:
						write(C, "isub\n");
						goto arith_typecheck;
					case TYPE_ASTER:
						write(C, "imul\n");
						goto arith_typecheck;
					case TYPE_FORSLASH:
						write(C, "idiv\n");
						goto arith_typecheck;
					case TYPE_GT:
						write(C, "igt\n");
						goto arith_typecheck;
					case TYPE_GE:
						write(C, "ige\n");
						goto arith_typecheck;
					case TYPE_LT:
						write(C, "ilt\n");
						goto arith_typecheck;
					case TYPE_LE:
						write(C, "ile\n");
						goto arith_typecheck;
					case TYPE_PERIOD:
						if (pop[1]->type == EXP_LITERAL) {
							compile_error(C, "can't use operator '.' on a literal");
						}
						/* if pop[1] is a struct, we know it's an embedded period,
						 * e.g. (a.b.c)
						 *
						 * otherwise, if pop[1] is an identifier, we know it's the first
						 * period in the statement, e.g. (a.b)
						 */
						 /*
						TreeDecl* children;
						if (pop[1]->type == EXP_IDENTIFIER) {
							TreeDecl* local = find_local(C, pop[1]->pidentifier->word);
							if (!local) {
								compile_error(C, "undeclared identifier '%s'", pop[1]->pidentifier->word);
							}
							TreeStruct* template = find_type(C, local->datatype->type_name);
							if (!template) {
								compile_error(C, "can't find type name of '%s'", local->identifier);
							}
							children = template->children;
						} else if (pop[1]->type == EXP_STRUCT) {
							children = pop[1]->pstruct->children;
						}
						*/
						/* scan through the children of the parent struct and look for the member */
						for (TreeDecl* j = pop[1]->pstruct->children; j; j = j->next) {
							if (!strcmp(j->identifier, pop[0]->pidentifier->word)) {
								ExpNode* push = malloc(sizeof(ExpNode));
								push->next = NULL;
								write(C, "icinc %d\n", j->offset);
								TreeStruct* member_type;
								/* if the member is a struct, don't dereference, it's not a prim. type */
								if ((member_type = find_type(C, j->datatype->type_name))) {
									push->type = EXP_STRUCT;
									push->pstruct = member_type;
								/* else, it is a primitive type, dereference */
								} else {
									write(C, "ider\n");
									push->type = EXP_DATATYPE;
									push->pdatatype = j->datatype;
								}
								exp_push(&stack, push);
								goto no_typecheck;
							}
						}
						goto no_typecheck;
				}
				arith_typecheck:
				exp_push(&stack, pop[0]);
				break;
				no_typecheck:
				break;
			case EXP_FUNC_CALL:

				break;
			case EXP_ARRAY_INDEX:
			case EXP_NOTYPE:
				break;
		}
	}
}

static void
generate_if(CompileState* C) {
	unsigned int label = C->label_count++;
	generate_expression(C, postfix_expression(C, C->focus->pif->condition));
	write(C, JZ_LABEL "\n", label);
	push_instruction(C, DEF_LABEL "\n", label);
}

static void
generate_while(CompileState* C) {
	unsigned int top_label = C->label_count++;
	unsigned int done_label = C->label_count++;
	write(C, DEF_LABEL "\n", top_label);
	generate_expression(C, postfix_expression(C, C->focus->pwhile->condition));
	write(C, JZ_LABEL "\n", done_label);
	push_instruction(C, JMP_LABEL "\n", top_label);
	push_instruction(C, DEF_LABEL "\n", done_label);
}

static void
generate_function_decl(CompileState* C) {
	write(C, DEF_FUNC "\n", C->focus->pfunc->identifier);
	C->return_label = C->label_count++;
	C->func = C->focus->pfunc;
	
	/* load arguments onto the stack */
	for (int i = 0; i < C->focus->pfunc->nargs; i++) {
		write(C, "iarg %d\n", i);	
	}

	/* reserve space for locals.... TODO currently locals
	 * have a maximum size of 1, but structs will have
	 * larger sizes... make sure to allocate enough space
	 * for them in the future
	 */
	write(C, "res %d\n", C->focus->pfunc->reserve_space);

	push_instruction(C, DEF_LABEL "\n", C->return_label);
	push_instruction(C, "iret\n");
}

static void
generate_assignment(CompileState* C) {
	generate_expression(C, postfix_expression(C, C->focus->pass->rhs));
	ExpNode* lhs = postfix_expression(C, C->focus->pass->lhs);
	print_expression(lhs);
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
	C->return_label = 0;
	C->token = NULL;
	if (!C->fout) {
		printf("couldn't open output file '%s' for writing\n", fout_name);
		exit(1);
	}

	int advance_success = 1;
	
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
			case NODE_FUNCTION:
				generate_function_decl(C);
				break;
			case NODE_ASSIGN:
				generate_assignment(C);
				break;
		}
		advance_success = advance(C);
	}

}
