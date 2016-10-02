#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "parse.h"

static void parse_if(ParseState*);
static void parse_while(ParseState*);
static void parse_function(ParseState*);
static void parse_statement(ParseState*);
static void parse_struct_declaration(ParseState*);
static void parse_return(ParseState*);
static void jump_out(ParseState*);
static void jump_in(ParseState*, TreeBlock*);
static void string_token(Token*, Token*);
static void print_block(TreeBlock*, unsigned int);
static void list_tokens(Token*);
static void register_local(ParseState*, TreeDecl*);
static void parse_error(ParseState*, const char*, ...);
static uint32_t read_modifier(ParseState*);
static int check_datatype(ParseState*, const char*);
static int is_keyword(ParseState*);
static int get_variable_size(ParseState*, TreeDecl*);

static TreeNode* new_node(ParseState*, NodeType);
static Token* parse_until(ParseState*, unsigned int);
static Token* parse_count(ParseState*, unsigned int, unsigned int);
static TreeDecl* parse_decl(ParseState*);
static TreeDatatype* parse_datatype(ParseState*);

static const char* keywords[32] = {
	"func", "if", "while", "for", "do",
	"break", "continue", "return"
};

static void
parse_error(ParseState* P, const char* format, ...) {
	va_list args;
	va_start(args, format);

	printf("<");
	for (int i = 0; i < 40; i++) {
		fputc('-', stdout);	
	}
	printf(">\n\n\n*** SPYRE COMPILE-TIME ERROR ***\n\nMESSAGE:  ");
	vprintf(format, args);
	printf("\nLINE:     %d\n\n\n<", P->token->line);
	for (int i = 0; i < 40; i++) {
		fputc('-', stdout);	
	}
	printf(">\n");

	va_end(args);
	exit(1);
}

static int
get_variable_size(ParseState* P, TreeDecl* variable) {	
	const char* type_name = variable->datatype->type_name;
	if (!strcmp(type_name, "int") || !strcmp(type_name, "float")) {
		return 1;
	} 
	/* it's a struct */
	for (TreeStruct* i = P->defined_types; i; i = i->next) {
		if (!strcmp(i->type_name, type_name)) {
			printf("%s is %d\n", i->type_name, i->size);
			return i->size;
		}
	}
	return 0;
}

static int
check_datatype(ParseState* P, const char* type_name) {
	if (
		!strcmp(type_name, "int") ||
		!strcmp(type_name, "float") ||
		!strcmp(type_name, "string") ||
		!strcmp(type_name, "null")
	) {
		return 1;
	}
	for (TreeStruct* i = P->defined_types; i; i = i->next) {
		if (!strcmp(i->type_name, type_name)) {
			return 1;
		}
	}
	return 0;
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
		/* don't report failed modifier unless the datatype exists */
		if (!check_datatype(P, mod)) {
			parse_error(P, "unknown type name '%s'", mod);
		} else {
			parse_error(P, "unknown variable modifier '%s'", mod);
		}
	}
	return 0;
}

static void
register_local(ParseState* P, TreeDecl* local) {
	if (!P->block) {
		parse_error(P, "can't find a block to store local '%s'\n", local->identifier);
	}
	P->func->nlocals++;
	P->func->reserve_space += get_variable_size(P, local);
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
			case NODE_RETURN:
				printf("TYPE: RETURN\n");
				INDENT(1);
				printf("EXPRESSION: ");
				for (Token* j = i->pret->statement; j; j = j->next) {
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
	data->ptr_level = 0;
	uint32_t mod;
	while (!check_datatype(P, P->token->word) && (mod = read_modifier(P))) {
		data->modifier |= mod;
	}
	data->type_name = P->token->word;
	P->token = P->token->next;
	while (P->token->type == TYPE_UPCARROT) {
		data->ptr_level++;
		P->token = P->token->next;
	}
	/* ends on token after datatype */
	return data;
}

static TreeDecl*
parse_decl(ParseState* P) {
	/* expects to be on identifier of declaration */
	TreeDecl* decl = malloc(sizeof(TreeDecl));
	decl->next = NULL;
	decl->identifier = P->token->word;
	decl->offset = 0; /* to be assigned specifically when parse_decl is called */
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
	node->line = P->token->line;
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
			node->pfunc->reserve_space = 0;
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
		case NODE_RETURN:
			node->pret = malloc(sizeof(TreeReturn));
			node->pret->statement = NULL;
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
	/* expects to start on token IDENTIFIER */
	TreeNode* node = new_node(P, NODE_FUNCTION);
	node->pfunc->identifier = P->token->word;
	P->token = P->token->next->next->next;
	/* only instantiate arguments if list isn't empty */
	if (P->token->type != TYPE_CLOSEPAR) {
		while (P->token->type != TYPE_CLOSEPAR) {
			if (P->token->type == TYPE_COMMA) {
				P->token = P->token->next;
			}
			TreeDecl* arg = parse_decl(P);
			arg->offset = node->pfunc->nargs++;
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
parse_return(ParseState* P) {
	/* expects to be on token RETURN */
	TreeNode* node = new_node(P, NODE_RETURN);
	P->token = P->token->next;
	node->pret->statement = parse_until(P, TYPE_SEMICOLON);
	append_to_block(P, node);
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
		TreeDecl* decl = parse_decl(P);
		decl->offset = P->func->reserve_space;
		register_local(P, decl);
	/* handle statement */
	} else {
		node = new_node(P, NODE_STATEMENT);
		node->pstate->statement = statement;
		append_to_block(P, node);
	}
}

static void
parse_struct_declaration(ParseState* P) {
	/* expects to be on name of struct (syntax is (identifier : struct { ... })) */
	TreeStruct* decl = malloc(sizeof(TreeStruct));
	decl->type_name = P->token->word;
	decl->children = NULL;
	decl->next = NULL;
	decl->size = 0;
	P->token = P->token->next->next->next;
	/* if it ends with a semicolon, it's an imcomplete type and 
	 * has only been declared... still stick it into P->defined_types, 
	 * it will be written over later when its definition is completed 
	 */
	if (P->token->type == TYPE_SEMICOLON) {
		decl->complete = 0;
		if (!P->defined_types) {
			P->defined_types = decl;
		} else {
			TreeStruct* read;
			for (read = P->defined_types; read->next; read = read->next);
			read->next = decl;
		}
		P->token = P->token->next;
	/* otherwise it is a complete definition */
	} else if (P->token->type == TYPE_OPENCURL) {
		decl->complete = 1;
		P->token = P->token->next;
		while (P->token && P->token->type != TYPE_CLOSECURL) {
			TreeDecl* field = parse_decl(P);
			/* check of an illegal self-reference */
			if (!strcmp(field->datatype->type_name, decl->type_name)) {
				parse_error(P, "struct '%s' has an incomplete type", decl->type_name);
			}
			field->offset = decl->size;
			P->token = P->token->next; /* need to skip over semicolon */
			if (!decl->children) {
				decl->children = field;
			} else {
				TreeDecl* read;
				for (read = decl->children; read->next; read = read->next);
				read->next = field;
			}
			decl->size += get_variable_size(P, field);
		}
		/* if token still exists, it must be TYPE_OPENCURL, advance */
		if (P->token) {
			P->token = P->token->next;
		}
		/* now figure out if we should write over a previously defined
		 * struct or just append to the list of defined_types
		 */
		TreeStruct* prev_defined = NULL;
		for (TreeStruct* i = P->defined_types; i; i = i->next) {
			if (!strcmp(i->type_name, decl->type_name)) {
				if (i->complete) {
					parse_error(P, "attempt to re-define struct '%s'", decl->type_name);
				} else {
					i->complete = 1;
					i->children = decl->children;
					i->size = decl->size;
					free(decl);
					return;
				}
			}
		}		
		if (!P->defined_types) {
			P->defined_types = decl;
			return;
		}
		/* if reached here, couldn't fine prev. defined type, append new one */
		TreeStruct* read;
		for (read = P->defined_types; read->next; read = read->next);
		read->next = decl;
	} else {
		parse_error(P, "expected ';' or '{' after '%s : struct'", decl->type_name);
	}
}

ParseState*
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
	P->root->proot->block->locals = NULL;
	P->block = P->root->proot->block;
	P->token = tokens;
	P->func = NULL;
	P->defined_types = NULL;
	
	while (P->token) {
		if (P->token->next && P->token->next->next) {
			/* special case, check to see if it's a struct declaration */
			if (
				P->token->type == TYPE_IDENTIFIER
				&& P->token->next->type == TYPE_COLON 
				&& P->token->next->next->type == TYPE_STRUCT
			) {
				parse_struct_declaration(P);
				continue;
			}
			/* special case, function declaration */
			if (
				P->token->type == TYPE_IDENTIFIER
				&& P->token->next->type == TYPE_COLON 
				&& P->token->next->next->type == TYPE_OPENPAR
			) {
				parse_function(P);
				continue;
			}
		}
		switch (P->token->type) {
			case TYPE_IF:
				parse_if(P);
				break;	
			case TYPE_WHILE:
				parse_while(P);
				break;
			case TYPE_RETURN:
				parse_return(P);
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

	return P;
}
