#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "parse.h"

static void parse_if(ParseState*);
static void parse_while(ParseState*);
static void parse_function(ParseState*);
static void parse_statement(ParseState*);
static void jump_out(ParseState*);
static void jump_in(ParseState*, TreeBlock*);
static void string_token(Token*, Token*);
static void print_block(TreeBlock*, unsigned int);
static void list_tokens(Token*);
static void register_local(ParseState*, TreeDecl*);
static uint32_t read_modifier(ParseState*);
static int check_datatype(const char*);
static int is_keyword(ParseState*);

static TreeNode* new_node(ParseState*, NodeType);
static Token* parse_until(ParseState*, unsigned int);
static Token* parse_count(ParseState*, unsigned int, unsigned int);
static TreeDecl* parse_decl(ParseState*);
static TreeDatatype* parse_datatype(ParseState*);

static const char* keywords[32] = {
	"func", "if", "while", "for", "do",
	"break", "continue", "return"
};

void
parse_error(ParseState* P, const char* format, ...) {
	va_list args;
	va_start(args, format);

	printf("<");
	for (int i = 0; i < 40; i++) {
		fputc('-', stdout);	
	}
	printf(">\n\n\n*** SPYRE COMPILE-TIME ERROR ***\n\nMESSAGE:  ");
	vprintf(format, args);
	printf("\nLINE:     %d\nTOKEN:    %s\n\n\n<", P->token->line, P->token->word);
	for (int i = 0; i < 40; i++) {
		fputc('-', stdout);	
	}
	printf(">\n");

	va_end(args);
	exit(1);
}

static int
check_datatype(const char* datatype) {
	return (
		!strcmp(datatype, "int") ||
		!strcmp(datatype, "float") ||
		!strcmp(datatype, "string") ||
		!strcmp(datatype, "null")
	);
}

static int
is_keyword(ParseState* P) {
	for (const char** i = &keywords[0]; *i; i++) {
		if (!strcmp(P->token->word, *i)) {
			return 1;
		}
	}
	return 0;
}

static uint32_t
read_modifier(ParseState* P) {
	/* expects to be on a modifier of a variable declaration */
	const char* mod = P->token->word;
	P->token = P->token->next;
	if (!strcmp(mod, "const")) {
		return MOD_CONST;
	} else if (!strcmp(mod, "volatile")) {
		return MOD_VOLATILE;
	} else if (!strcmp(mod, "unsigned")) {
		return MOD_UNSIGNED;
	} else if (!strcmp(mod, "signed")) {
		return MOD_SIGNED;
	} else if (!strcmp(mod, "static")) {
		return MOD_STATIC;
	} else {
		parse_error(P, "unknown variable modifier '%s'", mod);
	}
	return 0;
}

static void
register_local(ParseState* P, TreeDecl* local) {
	if (!P->block) {
		parse_error(P, "can't find a block to store local '%s'\n", local->identifier);
	}
	P->func->nlocals++;
	local->next = NULL;
	if (!P->block->locals) {
		P->block->locals = local;
		return;
	}
	TreeDecl* read;
	for (read = P->block->locals; read->next; read = read->next);
	read->next = local;
}

static void
list_tokens(Token* token) {
	for (Token* i = token; i; i = i->next) {
		printf("%s ", i->word);
	}
}

#define INDENT(i) for (int _ = 0; _ < depth + (i); _++) printf("\t");

static void print_block(TreeBlock* block, unsigned int depth) {
	if (!block->children) {
		return;
	}
	printf("{\n");
	for (TreeNode* i = block->children; i; i = i->next) {
		INDENT(1);
		switch (i->type) {
			case NODE_IF:
				printf("TYPE: IF\n");
				INDENT(1);
				printf("CONDITION: ");
				list_tokens(i->pif->condition);
				printf("\n");
				INDENT(1);
				printf("BLOCK: ");
				if (i->pif->block->children) {
					print_block(i->pif->block, depth + 1);
				} else {
					printf("{}\n");
				}
				break;
			case NODE_WHILE:
				printf("TYPE: WHILE\n");
				INDENT(1);
				printf("CONDITION: ");
				list_tokens(i->pwhile->condition);
				printf("\n");
				INDENT(1);
				printf("BLOCK: ");
				if (i->pwhile->block->children) {
					print_block(i->pwhile->block, depth + 1);
				} else {
					printf("{}\n");
				}
				break;
			case NODE_FUNCTION:
				printf("TYPE: FUNCTION\n");
				INDENT(1);
				printf("IDENTIFIER: %s\n", i->pfunc->identifier);
				INDENT(1);
				printf("ARGS: \n");
				for (TreeDecl* j = i->pfunc->arguments; j; j = j->next) {
					INDENT(1);
					printf("  %s : ", j->identifier);
					if (j->datatype->modifier & MOD_CONST) {
						printf("const ");
					}
					if (j->datatype->modifier & MOD_VOLATILE) {
						printf("volatile ");
					}
					if (j->datatype->modifier & MOD_UNSIGNED) {
						printf("unsigned ");
					}
					if (j->datatype->modifier & MOD_SIGNED) {
						printf("signed ");
					}	
					if (j->datatype->modifier & MOD_STATIC) {
						printf("static ");
					}
					printf("%s\n", j->datatype->type_name);
				}
				INDENT(1);
				printf("RETURN_TYPE: %s\n", i->pfunc->return_type->type_name);
				INDENT(1);
				printf("BLOCK: ");
				if (i->pfunc->block->children) {
					print_block(i->pfunc->block, depth + 1);
				} else {
					printf("{}\n");
				}
				break;
			case NODE_ASSIGN:
				printf("TYPE: ASSIGNMENT\n");
				INDENT(1);
				printf("LHS: ");
				for (Token* j = i->pass->lhs; j; j = j->next) {
					printf("%s ", j->word);
				}
				printf("\n");
				INDENT(1);
				printf("RHS: ");
				for (Token* j = i->pass->rhs; j; j = j->next) {
					printf("%s ", j->word);
				}
				printf("\n");
				break;
			case NODE_STATEMENT:
				printf("TYPE: STATEMENT\n");
				INDENT(1);
				printf("EXPRESSION: ");
				for (Token* j = i->pstate->statement; j; j = j->next) {
					printf("%s ", j->word);
				}
				printf("\n");
				break;
			case NODE_ROOT:
				break;
		}
		if (i->next) {
			printf("\n");
		}
	}
	INDENT(0);
	printf("}\n");
}

static void
jump_out(ParseState* P) {
	/* expects to be on token '}' */
	P->token = P->token->next;
	if (!P->block->parent_node) {
		P->block = NULL; /* signal that parsing is done */
		return;
	}
	P->block = P->block->parent_node->parent_block;
}

static inline void 
jump_in(ParseState* P, TreeBlock* block) {
	P->block = block;
}

static void
append_to_block(ParseState* P, TreeNode* node) {
	if (!P->block->children) {
		P->block->children = node;
		return;
	}
	TreeNode* child;
	for (child = P->block->children; child->next; child = child->next);
	child->next = node;
	node->next = NULL;
	node->parent_block = P->block;
}

static Token*
parse_until(ParseState* P, unsigned int type) {
	if (P->token->type == type) {
		return NULL;
	}
	Token* expression = malloc(sizeof(Token));
	memcpy(expression, P->token, sizeof(Token));
	expression->next = NULL;
	expression->prev = NULL;
	P->token = P->token->next;
	for (; P->token->type != type; P->token = P->token->next) {
		if (is_keyword(P)) {
			parse_error(P, "unexpected keyword '%s' in expression, did you forget a semicolon?", P->token->word);
		}
		Token* copy = malloc(sizeof(Token));
		memcpy(copy, P->token, sizeof(Token));
		copy->next = NULL;
		copy->prev = NULL;
		string_token(expression, copy);
	}
	P->token = P->token->next;
	return expression;
}	

static TreeDatatype*
parse_datatype(ParseState* P) {
	/* expects to be on first modifier or datatype */
	TreeDatatype* data = malloc(sizeof(TreeDatatype));
	data->modifier = 0;
	uint32_t mod;
	while (!check_datatype(P->token->word) && (mod = read_modifier(P))) {
		data->modifier |= mod;
	}
	data->type_name = P->token->word;
	/* ends on token after datatype */
	P->token = P->token->next;
	return data;
}

static TreeDecl*
parse_decl(ParseState* P) {
	/* expects to be on identifier of declaration */
	TreeDecl* decl = malloc(sizeof(TreeDecl));
	decl->next = NULL;
	decl->identifier = P->token->word;
	P->token = P->token->next->next; 
	decl->datatype = parse_datatype(P);
	/* ends on character after datatype */
	return decl;
}

static void
string_token(Token* a, Token* b) {
	for (; a->next; a = a->next);
	a->next = b;
	b->prev = a;
	b->next = NULL;
}

static TreeNode*
new_node(ParseState* P, NodeType type) {
	TreeNode* node = malloc(sizeof(TreeNode));
	node->type = type;
	node->next = NULL;
	node->parent_block = P->block;	
	switch (type) {
		case NODE_IF:
			node->pif = malloc(sizeof(TreeIf));
			node->pif->condition = NULL;
			node->pif->block = malloc(sizeof(TreeBlock));
			node->pif->block->parent_node = node;
			node->pif->block->children = NULL;
			node->pif->block->locals = NULL;
			break;
		case NODE_WHILE:
			node->pwhile = malloc(sizeof(TreeWhile));
			node->pwhile->condition = NULL;
			node->pwhile->block = malloc(sizeof(TreeBlock));
			node->pwhile->block->parent_node = node;
			node->pwhile->block->children = NULL;
			node->pwhile->block->locals = NULL;
			break;
		case NODE_FUNCTION:
			node->pfunc = malloc(sizeof(TreeFunction));
			node->pfunc->identifier = NULL;
			node->pfunc->return_type = NULL;
			node->pfunc->arguments = NULL;
			node->pfunc->nargs = 0;
			node->pfunc->nlocals = 0;
			node->pfunc->block = malloc(sizeof(TreeBlock));
			node->pfunc->block->parent_node = node;
			node->pfunc->block->children = NULL;
			node->pfunc->block->locals = NULL;
			break;
		case NODE_ASSIGN:
			node->pass = malloc(sizeof(TreeAssign));
			node->pass->lhs = NULL;
			node->pass->rhs = NULL;
			break;	
		case NODE_STATEMENT:
			node->pstate = malloc(sizeof(TreeStatement));
			node->pstate->statement = NULL;
			break;
		case NODE_ROOT:
			break;
	}
	return node;
}

static void
parse_if(ParseState* P) {
	/* expects to start on token IF */
	TreeNode* node = new_node(P, NODE_IF);
	P->token = P->token->next; /* skip to first token of condition */
	node->pif->condition = parse_until(P, TYPE_OPENCURL);	
	append_to_block(P, node);
	jump_in(P, node->pif->block);
}

static void
parse_while(ParseState* P) {
	/* expects to start on token WHILE */
	TreeNode* node = new_node(P, NODE_WHILE);
	P->token = P->token->next; /* skip to first token of condition */
	node->pwhile->condition = parse_until(P, TYPE_OPENCURL);	
	append_to_block(P, node);
	jump_in(P, node->pwhile->block);
}

static void
parse_function(ParseState* P) {
	/* expects to start on token FUNC */
	TreeNode* node = new_node(P, NODE_FUNCTION);
	P->token = P->token->next;
	node->pfunc->identifier = P->token->word;
	P->token = P->token->next->next;
	/* only instantiate arguments if list isn't empty */
	if (P->token->type != TYPE_CLOSEPAR) {
		while (P->token->type != TYPE_CLOSEPAR) {
			node->pfunc->nargs++;
			if (P->token->type == TYPE_COMMA) {
				P->token = P->token->next;
			}
			TreeDecl* arg = parse_decl(P);
			if (!node->pfunc->arguments) {
				node->pfunc->arguments = arg;
			} else {
				TreeDecl* i;
				for (i = node->pfunc->arguments; i->next; i = i->next);
				i->next = arg;
			}
		}
	}
	/* skip ')' and '->' */
	P->token = P->token->next->next;
	node->pfunc->return_type = parse_datatype(P);
	/* skip '{' */
	P->token = P->token->next;
	P->func = node->pfunc;
	append_to_block(P, node);
	jump_in(P, node->pfunc->block);
}

static void
parse_statement(ParseState* P) {
	TreeNode* node;
	Token* start = P->token;
	Token* statement = parse_until(P, TYPE_SEMICOLON);
	/* check if there is an assignment operator or colon */
	Token* assign_token = NULL;
	int is_decl = 0;
	for (Token* i = statement; i; i = i->next) {
		if (i->type == TYPE_ASSIGN) {
			assign_token = i;
			break;
		} else if (i->type == TYPE_COLON) {
			is_decl = 1;
			break;
		}
	}
	/* handle assignment */
	if (assign_token) {
		/* we're going to split this into the LHS and RHS...
		 * the way this will happen is we will set the last token
		 * in the LHS->next to NULL because it points to
		 * assign_token, then set the assign_token->next->prev to NULL
		 * and assign that to RHS, then free the '=' token
		 */
		node = new_node(P, NODE_ASSIGN);
		node->pass->lhs = statement;
		node->pass->rhs = assign_token->next;
		assign_token->next->prev = NULL;
		assign_token->prev->next = NULL;
		free(assign_token);
		append_to_block(P, node);
	/* handle declaration */
	} else if (is_decl) {
		P->token = start; /* go back to statement.... kind of hacky */
		register_local(P, parse_decl(P));
	/* handle statement */
	} else {
		node = new_node(P, NODE_STATEMENT);
		node->pstate->statement = statement;
		append_to_block(P, node);
	}
}

TreeNode*
generate_tree(Token* tokens) {
	
	ParseState* P = malloc(sizeof(ParseState));
	P->root = malloc(sizeof(TreeNode));
	P->root->type = NODE_ROOT;
	P->root->next = NULL;
	P->root->prev = NULL;
	P->root->parent_block = NULL;
	P->root->proot = malloc(sizeof(TreeRoot));
	P->root->proot->block = malloc(sizeof(TreeBlock));
	P->root->proot->block->parent_node = P->root;
	P->root->proot->block->children = NULL;
	P->block = P->root->proot->block;
	P->token = tokens;
	P->func = NULL;
	
	while (P->token) {
		switch (P->token->type) {
			case TYPE_IF:
				parse_if(P);
				break;	
			case TYPE_WHILE:
				parse_while(P);
				break;
			case TYPE_FUNCTION:
				parse_function(P);
				break;
			case TYPE_CLOSECURL:
				jump_out(P);
				if (!P->block) {
					goto finished;
				}
				break;
			case TYPE_SEMICOLON:
				P->token = P->token->next;
				break;
			default:
				parse_statement(P);
				break;
		}
	}
	finished:
	print_block(P->root->proot->block, 0);

	return P->root;
}
