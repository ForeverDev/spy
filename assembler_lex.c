#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "assembler_lex.h"

AssemblerToken*
AsmLexer_convertToAssemblerTokens(const char* source) {
	AsmLexer L;
	L.tokens = NULL;
	L.line = 1;

	char c;
	while ((c = *source++)) {
		if (c == '\n') {
			L.line++;
		} else if (c == ' ' || c == '\t') {
			continue;
		} else if (c == ';') {
			while (*source && *source != '\n') source++;
		} else if (c == '\'') {
			char* word = (char *)malloc(128);
			if (*source == '\\') {
				char result = 0;
				switch (*++source) {
					case '\\': result = '\\'; break;
					case '\'': result = '\''; break;
					case 't':  result = '\t'; break;
					case 'n':  result = '\n'; break;
					case '0':  result = 0;	  break;
				}
				sprintf(word, "%d", result);
			} else {
				sprintf(word, "%d", *source++);
			}
			source++;
			AsmLexer_appendAssemblerToken(&L, word, NUMBER);
		} else if (c == '"') {
			char* word;
			size_t len = 0;
			const char* at = source;
			while (*at++ != '"') len++;
			at--;
			word = (char *)calloc(1, len + 1);
			for (int i = 0; i < len; i++) {
				if (*source == '"') break;
				switch (*source) {
					case '\\':
						switch (*++source) {
							case 'n':
								word[i] = '\n';
								break;
							case 't':
								word[i] = '\t';
								break;
							case '"':
								word[i] = '"';
								break;
							case '0':
								word[i] = 0;
								break;
							case '\\':
								word[i] = '\\';
								break;
							case '\'':
								word[i] = '\'';
								break;
						}
						break;
					default:
						if (*source == '"') break;
						word[i] = *source;
				}
				source++;
			}
			word[len] = 0;
			source++;
			AsmLexer_appendAssemblerToken(&L, word, LITERAL);
		} else if (ispunct(c) && c != '_') {
			char word[2];
			word[0] = c;
			word[1] = 0;
			AsmLexer_appendAssemblerToken(&L, word, PUNCT);
		} else if (isdigit(c)) {
			//if (*source == 'x' && c == '0') source += 2;
			char* word;
			size_t len = 0;
			const char* at = source;
			while ((isdigit(*at) || *at == '.') && ++len) at++;
			at--;
			word = (char *)malloc(len + 2);	
			memcpy(word, source - 1, len + 1);
			word[len + 1] = 0;
			source += len;
			AsmLexer_appendAssemblerToken(&L, word, NUMBER);
		} else if (isalpha(c) || c == '_') {
			char* word;
			size_t len = 0;
			const char* at = source;
			while ((isalnum(*at) || *at == '_') && ++len) at++;
			word = (char *)malloc(len + 2);	
			memcpy(word, source - 1, len + 1);
			word[len + 1] = 0;
			source += len;
			AsmLexer_appendAssemblerToken(&L, word, IDENTIFIER);
		}
	}	

	/* tokens on heap, L on stack */
	return L.tokens;
}

static void
AsmLexer_appendAssemblerToken(AsmLexer* L, const char* word, AssemblerTokenType type) {
	AssemblerToken* token = (AssemblerToken *)malloc(sizeof(AssemblerToken));
	token->next = NULL;
	token->prev = NULL;
	token->line = L->line;
	token->type = type;
	size_t length = strlen(word);
	token->word = (char *)malloc(length + 1);
	strcpy(token->word, word);
	token->word[length] = 0;
	if (!L->tokens) {
		L->tokens = token;
	} else {
		AssemblerToken* at = L->tokens;
		while (at->next) at = at->next;
		at->next = token;
		token->prev = at;
	}
}

static void
AsmLexer_printAssemblerTokens(AsmLexer* L) {
	AssemblerToken* at = L->tokens;
	while (at) {
		printf("%s (%s)\n", at->word, (
			at->type == PUNCT ? "operator" :
			at->type == NUMBER ? "number" : 
			at->type == IDENTIFIER ? "identifier" :
			at->type == LITERAL ? "literal" : "?"
		));		
		at = at->next;
	}
}
