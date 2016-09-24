#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "lex.h"

void
print_tokens(Token* head) {
	while (head) {
		printf("(%d : %s)\n", head->type, head->word);
		head = head->next;
	}
}

Token*
blank_token() {
	return (Token *)calloc(1, sizeof(Token));
}

void
append_token(Token* head, char* word, unsigned int line, unsigned int type) {
	if (head->type == 0) {
		head->word = word;
		head->line = line;
		head->type = type;
		head->next = NULL;
		head->prev = NULL;
	} else {
		Token* new = malloc(sizeof(Token));
		while (head->next) {
			head = head->next;
		}
		new->word = word;
		new->line = line;
		new->type = type;
		new->next = NULL;
		new->prev = head;
		head->next = new;
	}
}

Token*
generate_tokens(const char* filename) {

	Token* tokens = malloc(sizeof(Token));	
	tokens->next = NULL;
	tokens->type = 0; /* empty */
	tokens->line = 0;
	tokens->word = NULL;

	FILE* handle;
	char* contents;
	unsigned long long flen;
	handle = fopen(filename, "rb");
	fseek(handle, 0, SEEK_END);
	flen = ftell(handle);
	fseek(handle, 0, SEEK_SET);
	contents = malloc(flen + 1);
	fread(contents, 1, flen, handle);
	contents[flen] = 0;

	unsigned int line = 1;
	
	/* general purpose vars */
	char* buf;
	char* start;
	unsigned int len = 0;

	while (*contents) {
		len = 0;
		if (*contents == '\n') {
			contents++;
			line++;
			continue;
		} else if (*contents == '\t' || *contents == 32 || *contents == 13) {
			contents++;
			continue;
		} else if (isalpha(*contents) || *contents == '_' || *contents == '"') {
			int is_string = 0;
			if (*contents == '"') {
				is_string = 1;
				contents++;
			}
			start = contents;
			if (is_string) {
				while (*contents != '"') {
					contents++;
					len++;
				}
				contents++;
			} else {
				while (*contents && (isalnum(*contents) || *contents == '_') && *contents != ' ') {
					contents++;
					len++;
				}
			}
			buf = calloc(1, len + 1);
			for (unsigned i = 0; i < contents - start; i++) {
				buf[i] = start[i];
			}
			buf[len] = 0;
			append_token(tokens, buf, line, (
				is_string ? 14 : 
				!strcmp(buf, "if") ? 1 : 
				!strcmp(buf, "else") ? 2 : 
				!strcmp(buf, "while") ? 3 :
				!strcmp(buf, "do") ? 4 : 
				!strcmp(buf, "func") ? 5 :
				!strcmp(buf, "return") ? 6 : 
				!strcmp(buf, "switch") ? 7 : 
				!strcmp(buf, "case") ? 8 : 
				!strcmp(buf, "continue") ? 9 : 
				!strcmp(buf, "break") ? 10 : 
				!strcmp(buf, "for") ? 11 : 12
			));
		} else if (isdigit(*contents)) {
			start = contents;
			/* TODO register all number formats and convert to base 10 */
			while (isdigit(*contents) || *contents == '.') {
				contents++;
				len++;
			}	
			buf = calloc(1, len + 1);
			strncpy(buf, start, len);
			buf[len] = 0;
			append_token(tokens, buf, line, 13);
		} else if (ispunct(*contents)) {
			/* replace with strcmp? */
			#define CHECK2(str) (*contents == str[0] && contents[1] == str[1])
			#define CHECK3(str) (*contents == str[0] && contents[1] == str[1] && contents[2] == str[2])

			unsigned int type;

			start = contents;
			type = (
				CHECK3(">>=") ? 142 :
				CHECK3("<<=") ? 143 :
				CHECK3("->=") ? 144 :
				CHECK2("&&") ? 128 : 
				CHECK2("||") ? 129 :
				CHECK2(">>") ? 130 : 
				CHECK2("<<") ? 131 :
				CHECK2("++") ? 132 :
				CHECK2("+=") ? 133 :
				CHECK2("--") ? 134 :
				CHECK2("-=") ? 135 :
				CHECK2("*=") ? 136 :
				CHECK2("/=") ? 137 :
				CHECK2("%=") ? 138 :
				CHECK2("&=") ? 139 :
				CHECK2("|=") ? 140 :
				CHECK2("^=") ? 141 :
				CHECK2("==") ? 145 :
				CHECK2("!=") ? 146 : 
				CHECK2(">=") ? 147 :
				CHECK2("<=") ? 148 : 
				CHECK2("->") ? 149 : (unsigned int)*contents
			);

			if (type == (unsigned int)*contents) {
				contents++;
			} else if (type >= 142 && type <= 144) {
				contents += 3;
			} else {
				contents += 2;
			}
			
			len = (unsigned int)(contents - start);
			buf = malloc(len + 1);
			strncpy(buf, start, len);
			buf[len] = 0;
			append_token(tokens, buf, line, type);
		}
	}

	return tokens;

}
