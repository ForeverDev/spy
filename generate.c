#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "generate.h"


typedef struct ExpNode ExpNode;
typedef struct ExpStack ExpStack;
typedef enum ExpType ExpType;

enum ExpType {
	EXP_NOTYPE,
	EXP_FUNC_CALL,
	EXP_ARRAY_INDEX,
	EXP_TOKEN
};

struct ExpNode {
	ExpType type;
	union {
		Token* ptoken;
		ExpNode* pfunc;
		ExpNode* parray;
	};
};

struct ExpStack {
	ExpNode* value;
	ExpStack* next;
	ExpStack* prev;
};

static int advance(CompileState*);
static int block_empty(CompileState*);
static const char* focus_tostring(CompileState*);
static ExpNode* infix_to_postfix(CompileState*, Token*);
static void generate_if(CompileState*);

/* ExpStack functions */
static void exp_push(ExpStack**, ExpNode*);
static ExpNode* exp_pop(ExpStack*);

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
	new->prev = NULL;
	ExpStack* i;
	for (i = *stack; i->next; i = i->next);
	i->next = new;
	new->prev = i;
}

static ExpNode*
exp_pop(ExpStack* stack) {
	ExpStack* i;
	for (i = stack; i->next; i = i->next);
	i->prev->next = NULL;
	ExpNode* ret = i->value;
	free(i);
	return ret;
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
		if (!C->focus->parent_block->parent_node) {
			break;
		}
		C->focus = C->focus->parent_block->parent_node;
	}
	if (!C->focus->next) {
		return 0;
	}
	C->focus = C->focus->next;
	return 1;
}

static ExpNode*
infix_to_postfix(CompileState* C, Token* expression) {
	ExpNode* node = calloc(1, sizeof(ExpNode));	
	for (Token* i = expression; i; i = i->next) {
		
	}
	return node;
}

static void
generate_if(CompileState* C) {
	
}

void
generate_bytecode(TreeNode* tree, const char* fout_name) {
	
	CompileState* C = malloc(sizeof(CompileState));
	C->root = tree; 
	C->focus = tree;
	C->fout = fopen(fout_name, "rb");
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
		}
		advance_success = advance(C);
	}

}
