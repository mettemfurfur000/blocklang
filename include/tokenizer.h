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
    TOK_COMMENT
} token_type;

typedef struct
{
    token_type type;
    char text[32]; // copy of the lexeme
    int value;     // numeric value for numbers
} token;

#define encode_instruction(opcode, target) ((target & 0x0F) << 4) | (opcode & 0x0F)

token next_token(const char **src);
int string_to_opcode(const char *str);
int string_to_target(const char *str);

#endif