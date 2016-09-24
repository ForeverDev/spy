#ifndef ASSEMBLER_LEX_H
#define ASSEMBLER_LEX_H

typedef struct AssemblerToken AssemblerToken;
typedef struct AsmLexer AsmLexer;
typedef enum AssemblerTokenType AssemblerTokenType;

enum AssemblerTokenType {
	NOTOK, PUNCT, NUMBER, IDENTIFIER, LITERAL
};

struct AssemblerToken {
	char*					word;
	unsigned int			line;
	AssemblerTokenType		type;
	AssemblerToken*			next;
	AssemblerToken*			prev;
};

struct AsmLexer {
	AssemblerToken*	tokens;	
	unsigned int	line;
};

AssemblerToken*		AsmLexer_convertToAssemblerTokens(const char*);
static void			AsmLexer_appendAssemblerToken(AsmLexer*, const char*, AssemblerTokenType);
static void			AsmLexer_printAssemblerTokens(AsmLexer*);

#endif
