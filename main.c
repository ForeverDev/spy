#include <string.h>
#include <stdlib.h>
#include "spyre.h"
#include "assembler.h"
#include "lex.h"
#include "parse.h"
#include "generate.h"

int main(int argc, char** argv) {

	if (argc <= 1) return printf("expected file name\n");

	char* args[] = {argv[1]};

	const unsigned int flags = SPY_NOFLAG;
	
	if (strlen(argv[1]) == 1) {
		if (!strncmp(argv[1], "a", 1)) {
			Assembler_generateBytecodeFile(argv[2]);
		} else if (!strncmp(argv[1], "r", 1)) {
			//Spy_execute(argv[2], SPY_NOFLAG | SPY_STEP | SPY_DEBUG, 1, args);
			Spy_execute(argv[2], flags, 1, args);
		} else if (!strncmp(argv[1], "c", 1)) {
			Token* tokens = generate_tokens(argv[2]);	
			ParseState* tree = generate_tree(tokens);
			generate_bytecode(tree, "examples/test.spys"); 
		}
	} else {
		unsigned int len = strlen(argv[1]);
		char* asm_file = calloc(1, len + 2);
		char* binary_file = calloc(1, len + 2);
		strcpy(asm_file, argv[1]);
		strcat(asm_file, "s");
		strcpy(binary_file, argv[1]);
		strcat(binary_file, "b");
		Token* tokens = generate_tokens(argv[1]);
		ParseState* tree = generate_tree(tokens);
		generate_bytecode(tree, asm_file);
		Assembler_generateBytecodeFile(asm_file);
		Spy_execute(binary_file, flags, 1, args);
	}

	return 0;

}
