#ifndef LEX_H
#define LEX_H

typedef struct Lexer Lexer;
typedef struct Token Token;
typedef enum TokenType TokenType;

#define TYPE_IF			1
#define TYPE_ELSE		2
#define TYPE_WHILE		3
#define TYPE_DO			4
#define TYPE_FUNCTION	5
#define TYPE_RETURN		6
#define TYPE_SWITCH		7
#define TYPE_CASE		8
#define TYPE_CONTINUE	9
#define TYPE_BREAK		10
#define TYPE_FOR		11
#define TYPE_IDENTIFIER	12
#define TYPE_NUMBER		13
#define TYPE_STRING		14
#define TYPE_FUNCCALL	15
#define TYPE_SPACE		32
#define TYPE_EXCL		33
#define TYPE_DQUOTE		34
#define TYPE_POUND		35
#define TYPE_DOLLAR		36
#define TYPE_PERCENT	37
#define TYPE_AMPERSAND	38
#define TYPE_QUOTE		39
#define TYPE_OPENPAR	40
#define TYPE_CLOSEPAR	41
#define TYPE_ASTER		42
#define TYPE_PLUS		43
#define TYPE_COMMA		44
#define TYPE_HYPHON		45
#define TYPE_PERIOD		46
#define TYPE_FORSLASH	47
#define TYPE_COLON		58
#define TYPE_SEMICOLON	59
#define TYPE_LT			60
#define TYPE_ASSIGN		61
#define TYPE_GT			62
#define TYPE_QUESTION	63
#define TYPE_AT			64
#define TYPE_OPENSQ		91
#define TYPE_BACKSLASH	92
#define TYPE_CLOSESQ	93
#define TYPE_XOR		94
#define TYPE_UNDERSCORE	95
#define TYPE_IFORGOTLOL	96
#define TYPE_OPENCURL	123
#define TYPE_LINE		124
#define TYPE_CLOSECURL	125
#define TYPE_TILDE		126
#define TYPE_LOGAND		128
#define TYPE_LOGOR		129
#define TYPE_SHR		130
#define TYPE_SHL		131
#define TYPE_INC		132
#define TYPE_INCBY		133
#define TYPE_DEC		134
#define TYPE_DECBY		135
#define TYPE_MULBY		136
#define TYPE_DIVBY		137
#define TYPE_MODBY		138
#define TYPE_ANDBY		139
#define TYPE_ORBY		140
#define	TYPE_XORBY		141
#define TYPE_SHRBY		142
#define TYPE_SHLBY		143
#define TYPE_ARROWBY	144
#define TYPE_EQ			145
#define TYPE_NOTEQ		146
#define TYPE_GE			147
#define TYPE_LE			148
#define TYPE_ARROW		149
#define TYPE_IGNORE		200

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
