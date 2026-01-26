#ifndef TOKENIZER_H
#define TOKENIZER_H 1

typedef enum
{
    TOK_EOF,
    TOK_LABEL,
    TOK_OPCODE,
    TOK_TARGET,
    TOK_NUMBER,
    TOK_COMMA,
    TOK_DOT,
    TOK_COLON,
    TOK_CHAR_LITERAL,
    TOK_STRING,

    TOK_BRACKET_LEFT,
    TOK_BRACKET_RIGHT,
    TOK_SQUARE_BRACKET_LEFT,
    TOK_SQUARE_BRACKET_RIGHT,
    TOK_CURLY_BRACKET_LEFT,
    TOK_CURLY_BRACKET_RIGHT,

    TOK_PLUS,
    TOK_MINUS,
    TOK_ASTERISK,
    TOK_FORWARDSLASH,

    TOK_EXCLAMATION_MARK,
    TOK_AT,
    TOK_HASHTAG,

    TOK_DOLLARSIGN,
    TOK_PERCENT,
    TOK_CARET,
    TOK_AMPERSAND,
    TOK_QUESTION_MARK,
    TOK_TILDA,

    TOK_COMMENT
} token_type;

typedef struct
{
    token_type type;
    char text[1024]; // copy of the lexeme
    int value;      // numeric value for numbers
} token;

#define encode_instruction(opcode, target) ((target & 0x0F) << 4) | (opcode & 0x0F)

token next_token(const char **src, int *lines_ret);
int string_to_opcode(const char *str);
int string_to_target(const char *str);

#endif