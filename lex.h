#ifndef LEX_H
#define LEX_H

typedef struct Lexer Lexer;
typedef struct Token Token;

#define TOK_IF			1
#define TOK_ELSE		2
#define TOK_WHILE		3
#define TOK_DO			4
#define TOK_FUNCTION	5
#define TOK_RETURN		6
#define TOK_SWITCH		7
#define TOK_CASE		8
#define TOK_CONTINUE	9
#define TOK_BREAK		10
#define TOK_FOR			11
#define TOK_IDENTIFIER	12
#define TOK_INT			13
#define TOK_STRING		14
#define TOK_FUNCCALL	15
#define TOK_STRUCT		16
#define TOK_FLOAT		17
#define TOK_CFUNC		18
#define TOK_SPACE		32
#define TOK_EXCL		33
#define TOK_DQUOTE		34
#define TOK_POUND		35
#define TOK_DOLLAR		36
#define TOK_PERCENT		37
#define TOK_AMPERSAND	38
#define TOK_QUOTE		39
#define TOK_OPENPAR		40
#define TOK_CLOSEPAR	41
#define TOK_ASTER		42
#define TOK_PLUS		43
#define TOK_COMMA		44
#define TOK_HYPHON		45
#define TOK_PERIOD		46
#define TOK_FORSLASH	47
#define TOK_COLON		58
#define TOK_SEMICOLON	59
#define TOK_LT			60
#define TOK_ASSIGN		61
#define TOK_GT			62
#define TOK_QUESTION	63
#define TOK_AT			64
#define TOK_OPENSQ		91
#define TOK_BACKSLASH	92
#define TOK_CLOSESQ		93
#define TOK_UPCARROT	94
#define TOK_UNDERSCORE	95
#define TOK_IFORGOTLOL	96
#define TOK_DOTS		97
#define TOK_OPENCURL	123
#define TOK_LINE		124
#define TOK_CLOSECURL	125
#define TOK_TILDE		126
#define TOK_LOGAND		128
#define TOK_LOGOR		129
#define TOK_SHR			130
#define TOK_SHL			131
#define TOK_INC			132
#define TOK_INCBY		133
#define TOK_DEC			134
#define TOK_DECBY		135
#define TOK_MULBY		136
#define TOK_DIVBY		137
#define TOK_MODBY		138
#define TOK_ANDBY		139
#define TOK_ORBY		140
#define	TOK_XORBY		141
#define TOK_SHRBY		142
#define TOK_SHLBY		143
#define TOK_ARROWBY		144
#define TOK_EQ			145
#define TOK_NOTEQ		146
#define TOK_GE			147
#define TOK_LE			148
#define TOK_ARROW		149
#define TOK_IGNORE		200

struct Token {
	char*			word;
	unsigned int	line;	
	unsigned int	type;

	Token*			next;
	Token*			prev;
};

Token* generate_tokens(const char*);
void append_token(Token*, char*, unsigned int, unsigned int);
void print_tokens(Token*);
Token* blank_token();

#endif
