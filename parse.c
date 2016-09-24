#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parse.h"

static void parse_if(ParseState*);
static void parse_while(ParseState*);
static void string_token(Token*, Token*);

static TreeNode* new_node(ParseState*, NodeType);
static Token* parse_until(ParseState*, unsigned int);
static Token* parse_count(ParseState*, unsigned int, unsigned int);

static void
parse_if(ParseState* P) {
	/* expects to start on token IF */
	TreeNode* node = new_node(P, NODE_IF);
	P->token = P->token->next; /* skip to first token of condition */
	node->pif->condition = parse_until(P, TYPE_SEMICOLON);	
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
	for (; P->token->type != type; P->token = P->token->next) {
		Token* copy = malloc(sizeof(Token));
		memcpy(copy, P->token, sizeof(Token));
		copy->next = NULL;
		copy->prev = NULL;
		string_token(expression, copy);
	}
	P->token = P->token->next;
	return expression;
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
	switch (type) {
		case NODE_IF:
			node->pif = malloc(sizeof(TreeIf));
			node->pif->condition = NULL;
			node->pif->block = malloc(sizeof(TreeBlock));
			node->pif->block->parent = node;
			node->pif->block->children = NULL;
			break;
		case NODE_WHILE:
			node->pwhile = malloc(sizeof(TreeWhile));
			node->pwhile->condition = NULL;
			node->pwhile->block = malloc(sizeof(TreeBlock));
			node->pwhile->block->parent = node;
			node->pwhile->block->children = NULL;
			break;
		case NODE_FUNC:
		case NODE_ROOT:
		;
	}
	return node;
}

TreeNode*
generate_tree(Token* tokens) {

	ParseState* P = malloc(sizeof(ParseState));
	P->root = malloc(sizeof(TreeNode));
	P->root->type = NODE_ROOT;
	P->root->next = NULL;
	P->root->prev = NULL;
	P->root->parent = NULL;
	P->root->proot->block = malloc(sizeof(TreeBlock));
	P->root->proot->block->parent = P->root;
	P->root->proot->block->children = NULL;
	P->block = P->root->proot->block;
	P->token = tokens;
	
	while (P->token) {
		switch (P->token->type) {
			case TYPE_IF:
				parse_if(P);
				break;	
		}
	}

	return NULL;
}
