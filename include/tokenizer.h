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
    TOK_SEMICOLON,
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

    TOK_LESSER,
    TOK_GREATER,
    TOK_LESSER_OR_EQUAL,
    TOK_GREATER_OR_EQUAL,

    TOK_EQUAL,
    TOK_NOT_EQUAL,
    
    TOK_PLUS_EQUAL,
    TOK_MINUS_EQUAL,
    TOK_ASTERISK_EQUAL,
    TOK_FORWARDSLASH_EQUAL,

    TOK_COMMENT
} token_type;

typedef struct
{
    token_type type;
    char text[256]; // copy of the lexeme
    int value;      // numeric value for numbers
} token;

#define encode_instruction(opcode, target) ((target & 0x0F) << 4) | (opcode & 0x0F)

const char *tok_to_str(token_type t);
bool is_valid_opcode(const char *str);
bool is_valid_target(const char *str);

token next_token(const char **src, int *lines_ret);
int string_to_opcode(const char *str);
int string_to_target(const char *str);

#endif