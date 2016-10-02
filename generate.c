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
#define ASSOC_LEFT 1
#define ASSOC_RIGHT 2

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
	EXP_IDENTIFIER,
	EXP_STRUCT,
	EXP_DATATYPE
};

struct ExpOperator {
	unsigned int pres;
	unsigned int assoc;
};

struct ExpFuncCall {
	TreeFunction* func;
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
static void write(CompileState*, const char*, ...);
static TreeDecl* find_local(CompileState*, const char*);
static void compile_error(CompileState*, const char*, ...);
static TreeStruct* type_defined(CompileState*, const char*);
static void comment(CompileState*, const char*, ...);
static void token_line(CompileState*, Token*);
static int identical_types(TreeDatatype*, TreeDatatype*);
static char* tostring_datatype(TreeDatatype*);
static TreeDatatype* copy_datatype(TreeDatatype*);

/* generating (etc) functions */
static void generate_if(CompileState*);
static void generate_while(CompileState*);
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
	write(C, " ; -----> ");
	write(C, format, args);
	va_end(args);	
}

static void
token_line(CompileState* C, Token* token) {
	for (Token* i = token; i; i = i->next) {
		write(C, "%s ", i->word);
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
	if (strcmp(a->type_name, b->type_name)) {
		return 0;
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

static char*
tostring_datatype(TreeDatatype* type) {
	char* str = calloc(1, 256);
	if (type->modifier & MOD_CONST) {
		strcat(str, "const ");
	}
	if (type->modifier & MOD_VOLATILE) {
		strcat(str, "volatile ");
	}
	if (type->modifier & MOD_STATIC) {
		strcat(str, "static ");
	}
	strcat(str, type->type_name);
	for (int i = 0; i < type->ptr_level; i++) {
		strcat(str, "^");
	}
	return str;
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
				write(C, "\n");
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
	
	node->pcall->argument = postfix_expression(C, argument);
	
	return node;	
}

/* wraps a Token expression into an ExpNode expression
 * and converts it from infix to postfix
 */
static ExpNode*
postfix_expression(CompileState* C, Token* expression) {

	static const ExpOperator ops[256] = {
		/* 1 = left, 2 = right */
		[TYPE_COMMA]		= {1, ASSOC_LEFT},
		[TYPE_ASSIGN]		= {2, ASSOC_LEFT},
		[TYPE_LOGAND]		= {2, ASSOC_LEFT},
		[TYPE_LOGOR]		= {2, ASSOC_LEFT},
		[TYPE_EQ]			= {3, ASSOC_LEFT},
		[TYPE_NOTEQ]		= {3, ASSOC_LEFT},
		[TYPE_PERIOD]		= {4, ASSOC_LEFT},
		[TYPE_GT]			= {5, ASSOC_LEFT},
		[TYPE_GE]			= {5, ASSOC_LEFT},
		[TYPE_LT]			= {5, ASSOC_LEFT},
		[TYPE_LE]			= {5, ASSOC_LEFT},
		[TYPE_LINE]			= {6, ASSOC_LEFT},
		[TYPE_UPCARROT]		= {6, ASSOC_LEFT},
		[TYPE_SHL]			= {6, ASSOC_LEFT},
		[TYPE_SHR]			= {6, ASSOC_LEFT},
		[TYPE_PLUS]			= {7, ASSOC_LEFT},
		[TYPE_HYPHON]		= {7, ASSOC_LEFT},
		[TYPE_ASTER]		= {8, ASSOC_LEFT},
		[TYPE_PERCENT]		= {8, ASSOC_LEFT},
		[TYPE_FORSLASH]		= {8, ASSOC_LEFT},
		[TYPE_AMPERSAND]	= {9, ASSOC_RIGHT},
		[TYPE_UPCARROT]		= {9, ASSOC_RIGHT},
		[TYPE_PERIOD]		= {10, ASSOC_LEFT}
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
		} else if (C->token->type == TYPE_IDENTIFIER) {
			node = malloc(sizeof(ExpNode));
			node->type = EXP_IDENTIFIER;
			node->pidentifier = C->token; // find_local(C, C->token->word);
			node->next = NULL;
			exp_push(&postfix, node);
		} else if (C->token->type == TYPE_INT || C->token->type == TYPE_FLOAT) {
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

	print_expression(expression);

	for (ExpNode* i = expression; i; i = i->next) {
		switch (i->type) {
			case EXP_LITERAL:
				exp_push(&stack, i);
				write(C, "ipush %s\n", i->pliteral->word);
				break;
			case EXP_VARIABLE: {
				int next_amper = (
					i->next 
					&& i->next->type == EXP_OPERATOR
					&& i->next->poperator->type == TYPE_AMPERSAND
				);
				if (next_amper) {
					write(C, "ilea %d\n", i->pvariable->offset);
				} else {
					write(C, "ilload %d\n", i->pvariable->offset);
				}
				exp_push(&stack, i);
				break;
			}
			case EXP_FUNC_CALL: {
				unsigned int ncall_args = 0;
				/* load arguments onto the stack */
				ExpNode* ret = generate_expression(C, i->pcall->argument, 0);
				for (ExpNode* j = ret; j; j = j->next) {
					ncall_args++;
				}
				/* ensure the function is being called with the correct num of args */
				if (ncall_args != i->pcall->func->nargs) {
					compile_error(C,
						"incorrect number of arguments passed to function '%s', expected %d, got %d",
						i->pcall->func->identifier,
						i->pcall->func->nargs,
						ncall_args
					);
				}
				/* these are the arguments in the function declaration */
				unsigned int at_arg = 1;
				TreeDecl* declared_types = i->pcall->func->arguments;
				while (ret && declared_types) {
					if (!identical_types(declared_types->datatype, ret->pdatatype)) {
						compile_error(C,
							"attempt to pass argument #%d of type (%s) to function '%s', expected (%s)",
							at_arg,
							tostring_datatype(ret->pdatatype),
							i->pcall->func->identifier,
							tostring_datatype(declared_types->datatype)
						);
					}
					ret = ret->next;
					declared_types = declared_types->next;
					at_arg++;
				}
				write(C, "call %s, %d\n", i->pcall->func->identifier, ncall_args);
				ExpNode* node = malloc(sizeof(ExpNode));
				node->type = EXP_DATATYPE;
				node->pdatatype = i->pcall->func->return_type;
				node->next = NULL;
				exp_push(&stack, node);
				break;
			}
			case EXP_IDENTIFIER: {
				TreeDecl* local = find_local(C, i->pidentifier->word);
				int next_period = (
					i->next 
					&& i->next->type == EXP_OPERATOR
					&& i->next->poperator->type == TYPE_PERIOD
				);
				int next_amper = (
					i->next 
					&& i->next->type == EXP_OPERATOR
					&& i->next->poperator->type == TYPE_AMPERSAND
				);
				ExpNode* node = malloc(sizeof(ExpNode));
				node->next = NULL;
				/* it's a member if the next token is a period */
				if (local && !next_period) {
					TreeStruct* type = find_type(C, local->datatype->type_name);
					if (type) {
						node->type = EXP_STRUCT;
						node->pstruct = type;
						write(C, "ilea %d\n", local->offset);
					} else {	
						node->type = EXP_DATATYPE;
						node->pdatatype = local->datatype;
						if ((next_amper || is_lhs) && local->datatype->ptr_level <= 0) {
							write(C, "ilea %d\n", local->offset);
						} else {
							write(C, "ilload %d\n", local->offset);
						}
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
						compile_error(C, "undeclared identifier '%s'", i->pidentifier->word);	
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
						compile_error(C, 
							"'%s' is not a valid member of struct '%s'", 
							i->pidentifier->word, 
							top->pstruct->type_name
						);
					}
				}
				exp_push(&stack, node);
				break;
			}
			case EXP_OPERATOR: 
				if (
					i->poperator->type != TYPE_AMPERSAND
					&& i->poperator->type != TYPE_UPCARROT
					&& i->poperator->type != TYPE_COMMA
				) {
					for (int i = 0; i < 2; i++) {
						if (!stack) {
							compile_error(C, "malformed expression");
						}
						pop[i] = exp_pop(&stack);
					}
					if (
						(pop[0]->type == EXP_LITERAL || pop[0]->type == EXP_DATATYPE)
						|| pop[1]->type == EXP_DATATYPE
					) {
						/* pointer arithmetic or numerial arith */
						if (!strcmp(pop[0]->pdatatype->type_name, "int") || pop[0]->pdatatype->ptr_level > 0) {
							exp_push(&stack, pop[1]);
						}
					}
				}
				switch (i->poperator->type) {
					case TYPE_AMPERSAND:
					case TYPE_UPCARROT: {
						pop[0] = exp_pop(&stack);
						TreeDatatype* type = NULL;
						switch (pop[0]->type) {
							case EXP_FUNC_CALL:
								type = copy_datatype(pop[0]->pcall->func->return_type);
								break;
							case EXP_DATATYPE:
								type = copy_datatype(pop[0]->pdatatype);
								break;
							case EXP_LITERAL:
								compile_error(C, "operator '%s' can't be used on a literal", i->poperator->word);
								break;
						}
						if (!type) {
							compile_error(C, "error generating expression");
						}
						if (i->poperator->type == TYPE_UPCARROT) {
							if (type->ptr_level == 0) {
								compile_error(C, "attempt to dereference a non-pointer");
							}
							type->ptr_level--;
							write(C, "ider\n");
						} else {
							type->ptr_level++;
						}
						ExpNode* node = malloc(sizeof(ExpNode));
						node->type = EXP_DATATYPE;
						node->next = NULL;
						node->pdatatype = type;
						exp_push(&stack, node);
						goto no_typecheck;
					}
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
						/* scan through the children of the parent struct and look for the member */
						for (TreeDecl* j = pop[1]->pstruct->children; j; j = j->next) {
							if (!strcmp(j->identifier, pop[0]->pidentifier->word)) {
								ExpNode* push = malloc(sizeof(ExpNode));
								push->next = NULL;
								write(C, "icinc %d\n", j->offset * 8);
								TreeStruct* member_type;
								/* if the member is a struct, don't dereference, it's not a prim. type */
								if ((member_type = find_type(C, j->datatype->type_name))) {
									push->type = EXP_STRUCT;
									push->pstruct = member_type;
								/* else, it is a primitive type, dereference */
								} else {
									int next_amper = (
										i->next 
										&& i->next->type == EXP_OPERATOR
										&& i->next->poperator->type == TYPE_AMPERSAND
									);
									if (!is_lhs && !next_amper) {
										write(C, "ider\n");
									}
									push->type = EXP_DATATYPE;
									push->pdatatype = j->datatype;
								}
								exp_push(&stack, push);
								goto no_typecheck;
							}
						}
						goto no_typecheck;
					case TYPE_COMMA:	
						goto no_typecheck;
				}
				arith_typecheck: 
				break;
				no_typecheck:
				break;
			case EXP_ARRAY_INDEX:
			case EXP_NOTYPE:
				break;
		}
	}
	
	ExpStack* final_stack = NULL;

	for (ExpStack* i = stack; i; i = i->next) {
		/* convert everything to the proper datatype */
		i->value->next = NULL;
		if (i->value->type != EXP_DATATYPE) {
			ExpNode* new = malloc(sizeof(ExpNode));
			new->type = EXP_DATATYPE;
			new->next = NULL;
			switch (i->value->type) {
				case EXP_FUNC_CALL:
					new->pdatatype = i->value->pcall->func->return_type;
					break;
				case EXP_LITERAL:
					new->pdatatype = malloc(sizeof(TreeDatatype));
					new->pdatatype->type_name = (
						i->value->pliteral->type == TYPE_INT ? "int" :
						i->value->pliteral->type == TYPE_FLOAT ? "float" : "string"
					);
					new->pdatatype->ptr_level = 0;
					new->pdatatype->modifier = 0;
					break;
				case EXP_VARIABLE:
					new->pdatatype = i->value->pvariable->datatype;
					break;
				case EXP_OPERATOR:
					break;
			}
			exp_push(&final_stack, new);
		} else {
			exp_push(&final_stack, i->value);
		}
	}

	for (ExpStack* i = final_stack; i && i->next; i = i->next) {
		i->value->next = i->next->value;
	}

	return final_stack ? final_stack->value : NULL;
}

static void
generate_if(CompileState* C) {	
	unsigned int label = C->label_count++;
	comment(C, "");
	write(C, "if ( ");
	token_line(C, C->focus->pass->lhs);
	write(C, ") {\n");
	generate_expression(C, postfix_expression(C, C->focus->pif->condition), 0);
	write(C, JZ_LABEL "\n", label);
	push_instruction(C, DEF_LABEL, label);
}

static void
generate_while(CompileState* C) {
	unsigned int top_label = C->label_count++;
	unsigned int done_label = C->label_count++;
	comment(C, "");
	write(C, "while ( ");
	token_line(C, C->focus->pass->lhs);
	write(C, ") {\n");
	write(C, DEF_LABEL "\n", top_label);
	generate_expression(C, postfix_expression(C, C->focus->pwhile->condition), 0);
	write(C, JZ_LABEL "\n", done_label);
	push_instruction(C, JMP_LABEL, top_label);
	push_instruction(C, DEF_LABEL, done_label);
}

static void
generate_function_decl(CompileState* C) {
	write(C, "\n\n" DEF_FUNC "\n", C->focus->pfunc->identifier);
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

	push_instruction(C, DEF_LABEL, C->return_label);
	push_instruction(C, "iret\n");
}

static void
generate_assignment(CompileState* C) {
	comment(C, "");
	token_line(C, C->focus->pass->lhs);
	write(C, "=\n");
	ExpNode* lhs = generate_expression(C, postfix_expression(C, C->focus->pass->lhs), 1);
	comment(C, "");
	token_line(C, C->focus->pass->rhs);
	write(C, "\n");
	ExpNode* rhs = generate_expression(C, postfix_expression(C, C->focus->pass->rhs), 0);

	/* typecheck */
	if (!(lhs->type == EXP_DATATYPE && rhs->type == EXP_DATATYPE)) {
		compile_error(C, "didn't pop LHS and RHS as datatypes...");
	}
	if (!identical_types(lhs->pdatatype, rhs->pdatatype)) {
		compile_error(C,
			"attempt to assign incompatible types, '%s' to '%s'",
			tostring_datatype(rhs->pdatatype),
			tostring_datatype(lhs->pdatatype)
		);
	}

	write(C, "isave\n");
}

static void
generate_statement(CompileState* C) {
	comment(C, "");
	token_line(C, C->focus->pstate->statement);
	write(C, "\n");
	generate_expression(C, postfix_expression(C, C->focus->pstate->statement), 0);
}

static void
generate_return(CompileState* C) {
	comment(C, "return ");
	token_line(C, C->focus->pret->statement);
	write(C, "\n");
	ExpNode* ret = generate_expression(C, postfix_expression(C, C->focus->pret->statement), 0);
	if (!identical_types(ret->pdatatype, C->func->return_type)) {
		compile_error(C,
			"attempt to return expression of type (%s) from function (%s),"
			"expected expression of type (%s)",
			tostring_datatype(ret->pdatatype),
			C->func->identifier,
			tostring_datatype(C->func->return_type)
		);
	}	
	write(C, JMP_LABEL "\n", C->return_label);
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

	write(C, "jmp __ENTRY_POINT__\n");

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
			case NODE_STATEMENT:
				generate_statement(C);
				break;
			case NODE_RETURN:
				generate_return(C);
				break;
		}
		advance_success = advance(C);
	}
	
	write(C, "__ENTRY_POINT__:\n");
	write(C, "call __FUNC__main, 0\n");

}
