#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "parse.h"

static TreeNode* parse_if(ParseState*);
static TreeNode* parse_else(ParseState*);
static TreeNode* parse_while(ParseState*);
static TreeNode* parse_for(ParseState*);
static TreeNode* parse_function(ParseState*);
static TreeNode* parse_continue(ParseState*);
static TreeNode* parse_break(ParseState*);
static TreeNode* parse_statement(ParseState*);
static TreeNode* parse_return(ParseState*);
static TreeNode* parse_cfunction(ParseState*);
static void parse_struct_declaration(ParseState*);
static void parse_function_args(ParseState*, TreeFunction*);
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
	"break", "continue", "return", "else",
	"elif"
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
	if (P->token) {
		printf("\nLINE:     %d\n\n\n<", P->token->line);
	} else {
		printf("\nLINE:     N/A\n\n\n<");
	}
	for (int i = 0; i < 40; i++) {
		fputc('-', stdout);	
	}
	printf(">\n");

	va_end(args);
	exit(1);
}

static int
get_variable_size(ParseState* P, TreeDecl* variable) {	
	/*
	if (variable->datatype->type == TYPE_BYTE && variable->datatype->ptr_level == 0) {
		return 1;
	}
	*/
	if (variable->datatype->type != TYPE_STRUCT) {
		return 1;
		//return 8;
	} 
	if (variable->datatype->ptr_level > 0) {
		return 1;
		//return 8;
	}
	return variable->datatype->pstruct->size;
}

static int
check_datatype(ParseState* P, const char* type_name) {
	if (
		!strcmp(type_name, "int") ||
		!strcmp(type_name, "float") ||
		!strcmp(type_name, "string") ||
		!strcmp(type_name, "byte") ||
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
	if (local->datatype->type == TYPE_STRUCT && local->datatype->ptr_level == 0) {
		P->func->reserve_space++;
	}
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
					printf("  %s : %s\n", j->identifier, tostring_datatype(j->datatype));
				}
				INDENT(1);
				printf("RETURN_TYPE: %s\n", tostring_datatype(i->pfunc->return_type));
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

char*
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
	strcat(str, (
		type->type == TYPE_INT ? "int" :
		type->type == TYPE_FLOAT ? "float" :
		type->type == TYPE_BYTE ? "byte" :
		type->type == TYPE_STRING ? "string" :
		type->type == TYPE_NULL ? "null" :
		type->pstruct->type_name
	));
	for (int i = 0; i < type->ptr_level; i++) {
		strcat(str, "^");
	}
	return str;
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
	for (; P->token && P->token->type != type; P->token = P->token->next) {
		if (is_keyword(P) || P->token->type == TOK_OPENCURL || P->token->type == TOK_CLOSECURL) {
			parse_error(P, "unexpected '%s' in expression - did you forget a semicolon?", P->token->word);
		}
		Token* copy = malloc(sizeof(Token));
		memcpy(copy, P->token, sizeof(Token));
		copy->next = NULL;
		copy->prev = NULL;
		string_token(expression, copy);
	}
	if (!P->token) {
		parse_error(P, "unexpected EOF while parsing expression - did you forget a semicolon?");
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
	data->type = (
		!strcmp(P->token->word, "int") ? TYPE_INT :
		!strcmp(P->token->word, "float") ? TYPE_FLOAT :
		!strcmp(P->token->word, "string") ? TYPE_STRING :
		!strcmp(P->token->word, "byte") ? TYPE_BYTE : 
		!strcmp(P->token->word, "null") ? TYPE_NULL : TYPE_STRUCT
	);
	data->pstruct = NULL;
	if (data->type == TYPE_STRUCT) {
		for (TreeStruct* i = P->defined_types; i; i = i->next) {
			if (!strcmp(i->type_name, P->token->word)) {
				data->pstruct = i;
			}
		}
		if (!data->pstruct) {
			parse_error(P, "'%s' is not a valid type", P->token->word);
		}
	}
	P->token = P->token->next;
	while (P->token->type == TOK_UPCARROT) {
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
			node->pif->if_type = IF_REG;
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
		case NODE_FOR:
			node->pfor = malloc(sizeof(TreeFor));
			node->pfor->condition = NULL;
			node->pfor->block = malloc(sizeof(TreeBlock));
			node->pfor->block->parent_node = node;
			node->pfor->block->children = NULL;
			node->pfor->block->locals = NULL;
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
		case NODE_CONTINUE:
			node->pcont = malloc(sizeof(TreeContinue));
			break;
		case NODE_BREAK:
			node->pbreak = malloc(sizeof(TreeBreak));
			break;	
		case NODE_ROOT:
			break;
	}
	return node;
}

/* handles else and elif */
static TreeNode*
parse_if(ParseState* P) {
	/* expects to start on token IF */
	TreeNode* node = new_node(P, NODE_IF);
	if (P->token->type == TOK_ELIF) {
		node->pif->if_type = IF_ELIF;
	}
	P->token = P->token->next; /* skip to first token of condition */
	node->pif->condition = parse_until(P, TOK_OPENCURL);	
	return node;
}

static TreeNode*
parse_else(ParseState* P) {
	TreeNode* node = new_node(P, NODE_IF);
	node->pif->if_type = IF_ELSE;
	P->token = P->token->next->next; /* goto first token in block */
	node->pif->condition = NULL;
	return node;
}

static TreeNode*
parse_while(ParseState* P) {
	/* expects to start on token WHILE */
	TreeNode* node = new_node(P, NODE_WHILE);
	P->token = P->token->next; /* skip to first token of condition */
	node->pwhile->condition = parse_until(P, TOK_OPENCURL);	
	return node;
}

static TreeNode*
parse_for(ParseState* P) {
	/* expects to be on token FOR */
	TreeNode* node = new_node(P, NODE_FOR);
	P->token = P->token->next;
	/* now on first token of initializer */
	node->pfor->init = parse_statement(P);
	node->pfor->condition = parse_until(P, TOK_SEMICOLON);
	node->pfor->statement = parse_statement(P);
	P->token = P->token->next;
	return node;
}

static void
parse_function_args(ParseState* P, TreeFunction* func) {
	/* expects to be on first token of args */
	/* only instantiate arguments if list isn't empty */
	if (P->token->type != TOK_CLOSEPAR) {
		while (P->token->type != TOK_CLOSEPAR) {
			if (P->token->type == TOK_COMMA) {
				P->token = P->token->next;
			}
			/* if true it's a vararg func */
			if (P->token->type == TOK_DOTS) {
				if (!func->is_cfunc) {
					parse_error(P, "only C functions can be vararg");
				}
				P->token = P->token->next;
				func->is_vararg = 1;
				break;
			}
			TreeDecl* arg = parse_decl(P);
			arg->offset = func->nargs++;
			if (!func->arguments) {
				func->arguments = arg;
			} else {
				TreeDecl* i;
				for (i = func->arguments; i->next; i = i->next);
				i->next = arg;
			}
		}
	}
}

static TreeNode*
parse_function(ParseState* P) {
	/* expects to start on token IDENTIFIER */
	TreeNode* node = new_node(P, NODE_FUNCTION);
	node->pfunc->identifier = P->token->word;
	node->pfunc->is_cfunc = 0;
	P->token = P->token->next->next->next;
	parse_function_args(P, node->pfunc);
	node->pfunc->reserve_space += node->pfunc->nargs;
	/* skip ')' and '->' */
	P->token = P->token->next->next;
	node->pfunc->return_type = parse_datatype(P);
	/* skip '{' */
	P->token = P->token->next;
	P->func = node->pfunc;
	return node;
}

static TreeNode*
parse_cfunction(ParseState* P) {
	/* expects to be on token IDENTIFIER */
	TreeNode* node = new_node(P, NODE_FUNCTION);
	node->pfunc->identifier = P->token->word;
	node->pfunc->is_cfunc = 1;
	P->token = P->token->next->next->next->next; /* skip to first token in args */
	parse_function_args(P, node->pfunc);
	P->token = P->token->next->next;
	node->pfunc->return_type = parse_datatype(P);
	/* now on semicolon */
	P->token = P->token->next;
	/* don't jump in, it's just a declaration */
	return node;
}

static TreeNode*
parse_return(ParseState* P) {
	/* expects to be on token RETURN */
	TreeNode* node = new_node(P, NODE_RETURN);
	P->token = P->token->next;
	node->pret->statement = parse_until(P, TOK_SEMICOLON);
	return node;
}

static TreeNode*
parse_statement(ParseState* P) {
	TreeNode* node = NULL;
	Token* start = P->token;
	Token* statement = parse_until(P, TOK_SEMICOLON);
	/* check if there is an assignment operator or colon */
	Token* assign_token = NULL;
	int is_decl = 0;
	for (Token* i = statement; i; i = i->next) {
		if (i->type == TOK_ASSIGN) {
			assign_token = i;
			break;
		} else if (i->type == TOK_COLON) {
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
	}
	return node;
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
	if (P->token->type == TOK_SEMICOLON) {
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
	} else if (P->token->type == TOK_OPENCURL) {
		decl->complete = 1;
		P->token = P->token->next;
		while (P->token && P->token->type != TOK_CLOSECURL) {
			TreeDecl* field = parse_decl(P);
			/* check of an illegal self-reference */
			if (
				field->datatype->type == TYPE_STRUCT &&
				!strcmp(field->datatype->pstruct->type_name, decl->type_name)
			) {
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
		/* if token still exists, it must be TOK_OPENCURL, advance */
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

static TreeNode*
parse_continue(ParseState* P) {
	P->token = P->token->next;
	TreeNode* node = new_node(P, NODE_CONTINUE);
	return node;
}

static TreeNode*
parse_break(ParseState* P) {
	P->token = P->token->next;
	TreeNode* node = new_node(P, NODE_BREAK);
	return node;
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
		if (P->token->next && P->token->type == TOK_FORSLASH && P->token->next->type == TOK_ASTER) {
			unsigned int start_line = P->token->line;
			P->token = P->token->next->next;
			while (1) {
				if (!P->token || !P->token->next) {
					parse_error(P, "expected end to comment on line %d", start_line);
				}
				if (P->token->type == TOK_ASTER && P->token->next->type == TOK_FORSLASH) {
					P->token = P->token->next->next;
					break;
				}
				P->token = P->token->next;
			}
			continue;
		}
		TreeNode* node = NULL;
		if (P->token->next && P->token->next->next) {
			/* special case, check to see if it's a struct declaration */
			if (
				P->token->type == TOK_IDENTIFIER
				&& P->token->next->type == TOK_COLON 
				&& P->token->next->next->type == TOK_STRUCT
			) {
				parse_struct_declaration(P);
				continue;
			}
			/* special case, function declaration */
			if (
				P->token->type == TOK_IDENTIFIER
				&& P->token->next->type == TOK_COLON 
				&& P->token->next->next->type == TOK_OPENPAR
			) {
				node = parse_function(P);
				append_to_block(P, node);
				jump_in(P, node->pfunc->block);
				continue;
			}
			/* special case, c function declaration */
			if (
				P->token->type == TOK_IDENTIFIER
				&& P->token->next->type == TOK_COLON
				&& P->token->next->next->type == TOK_CFUNC
			) {
				node = parse_cfunction(P);
				append_to_block(P, node);
				continue;
			}
		}
		switch (P->token->type) {
			case TOK_IF:
			case TOK_ELIF:
				node = parse_if(P);
				append_to_block(P, node);
				jump_in(P, node->pif->block);
				break;	
			case TOK_ELSE:
				node = parse_else(P);
				append_to_block(P, node);
				jump_in(P, node->pif->block);
				break;
			case TOK_WHILE:
				node = parse_while(P);
				append_to_block(P, node);
				jump_in(P, node->pwhile->block);
				break;
			case TOK_FOR:
				node = parse_for(P);
				append_to_block(P, node);
				jump_in(P, node->pfor->block);
				break;
			case TOK_CONTINUE:
				node = parse_continue(P);
				append_to_block(P, node);
				break;
			case TOK_BREAK:
				node = parse_break(P);
				append_to_block(P, node);
				break;
			case TOK_RETURN:
				node = parse_return(P);
				append_to_block(P, node);
				break;
			case TOK_CLOSECURL:
				jump_out(P);
				if (!P->block) {
					goto finished;
				}
				break;
			case TOK_SEMICOLON:
				P->token = P->token->next;
				break;
			default:
				node = parse_statement(P);
				if (node) {
					append_to_block(P, node);
				}
				break;
		}
	}
	finished:

	return P;
}
