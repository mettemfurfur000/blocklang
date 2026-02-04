#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/definitions.h"
#include "../include/tokenizer.h"

static const char *valid_opcodes[] = {"nop", "wait", "add", "sub", "mlt", "div", "mod", "get",
                                      "put", "push", "pop", "jmp", "jez", "jnz", "jof", "halt"};

static const char *valid_targets[] = {"STK", "ACC", "RG0", "RG1", "RG2", "RG3", "ADJ", "UP",
                                      "RIG", "DWN", "LFT", "ANY", "NIL", "SLN", "CUR", "REF"};

int string_to_opcode(const char *str)
{
    for (size_t i = 0; i < sizeof(valid_opcodes) / sizeof(valid_opcodes[0]); i++)
    {
        if (strcmp(str, valid_opcodes[i]) == 0)
            return (u8)i & 0x0F;
    }
    return -1;
}

int string_to_target(const char *str)
{
    for (size_t i = 0; i < sizeof(valid_targets) / sizeof(valid_targets[0]); i++)
    {
        if (strcmp(str, valid_targets[i]) == 0)
            return (u8)i & 0x0F;
    }
    return -1;
}

bool is_valid_opcode(const char *str)
{
    for (size_t i = 0; i < sizeof(valid_opcodes) / sizeof(valid_opcodes[0]); i++)
    {
        if (strcmp(str, valid_opcodes[i]) == 0)
            return true;
    }
    return false;
}

bool is_valid_target(const char *str)
{
    for (size_t i = 0; i < sizeof(valid_targets) / sizeof(valid_targets[0]); i++)
    {
        if (strcmp(str, valid_targets[i]) == 0)
            return true;
    }
    return false;
}

const char *tok_to_str(token_type t)
{
    switch (t)
    {
        CASE(TOK_EOF)
        CASE(TOK_LABEL)
        CASE(TOK_OPCODE)
        CASE(TOK_TARGET)
        CASE(TOK_NUMBER)
        CASE(TOK_COMMA)
        CASE(TOK_DOT)
        CASE(TOK_COLON)
        CASE(TOK_SEMICOLON)
        CASE(TOK_CHAR_LITERAL)
        CASE(TOK_STRING)
        CASE(TOK_BRACKET_LEFT)
        CASE(TOK_BRACKET_RIGHT)
        CASE(TOK_SQUARE_BRACKET_LEFT)
        CASE(TOK_SQUARE_BRACKET_RIGHT)
        CASE(TOK_CURLY_BRACKET_LEFT)
        CASE(TOK_CURLY_BRACKET_RIGHT)
        CASE(TOK_PLUS)
        CASE(TOK_MINUS)
        CASE(TOK_ASTERISK)
        CASE(TOK_FORWARDSLASH)
        CASE(TOK_EXCLAMATION_MARK)
        CASE(TOK_AT)
        CASE(TOK_HASHTAG)
        CASE(TOK_DOLLARSIGN)
        CASE(TOK_PERCENT)
        CASE(TOK_CARET)
        CASE(TOK_AMPERSAND)
        CASE(TOK_QUESTION_MARK)
        CASE(TOK_TILDA)
        CASE(TOK_LESSER)
        CASE(TOK_GREATER)
        CASE(TOK_LESSER_OR_EQUAL)
        CASE(TOK_GREATER_OR_EQUAL)
        CASE(TOK_EQUAL)
        CASE(TOK_NOT_EQUAL)
        CASE(TOK_PLUS_EQUAL)
        CASE(TOK_MINUS_EQUAL)
        CASE(TOK_ASTERISK_EQUAL)
        CASE(TOK_FORWARDSLASH_EQUAL)
        CASE(TOK_COMMENT)
    default:
        return "Unknown token";
    }
}

#define HANDLE_SINGLE(src, s, symbol, tok_type)                                                                        \
    if (*(s) == symbol)                                                                                                \
    {                                                                                                                  \
        tok.type = tok_type;                                                                                           \
        s++;                                                                                                           \
        *src = s;                                                                                                      \
        tok.text[0] = symbol;                                                                                          \
        tok.text[1] = '\0';                                                                                            \
        return tok;                                                                                                    \
    }

#define HANDLE_DOUBLE(src, s, symbol1, symbol2, tok_type)                                                              \
    if (*(s) == symbol1 && *(s + 1) == symbol2)                                                                        \
    {                                                                                                                  \
        tok.type = tok_type;                                                                                           \
        s += 2;                                                                                                        \
        *src = s;                                                                                                      \
        tok.text[0] = symbol1;                                                                                         \
        tok.text[1] = symbol2;                                                                                         \
        tok.text[2] = '\0';                                                                                            \
        return tok;                                                                                                    \
    }

char handle_escape_sequence(char *s)
{
    if (*s == '\\') // escape sequence
    {
        s++; // char after backslash
        switch (*s)
        {
        case 'a':
            return 0x07;
        case 'b':
            return 0x08;
        case 'e':
            return 0x1b;
        case 'f':
            return 0x0c;
        case 'n':
            return 0x0a;
        case 'r':
            return 0x0d;
        case 't':
            return 0x09;
        case 'v':
            return 0x0B;
        case '\\':
            return '\\';
        case '\'':
            return '\'';
        case '\"':
            return '\"';
        case '\?':
            return '\?';
        default:
            return -1; // unknown escape sequence
        }
    }
    else // a character
        return *s;
}

// Advances the source pointer and returns the next token
token next_token(const char **src, int *lines_ret)
{
    assert(src);
    int __ = 0;
    if (!lines_ret)
        lines_ret = &__;

    token tok = {0};
    const char *s = *src;

    // Skip whitespace and newlines
    while (*s == ' ' || *s == '\t' || *s == '\f' || *s == '\v' || *s == '\n' || *s == '\r')
    {
        *lines_ret += (*s == '\n' || *s == '\r') ? 1 : 0;
        s++;
    }

    if (*s == '\0')
    {
        tok.type = TOK_EOF;
        *src = s;
        return tok;
    }

    // Handle comments - semicolon starts a line comment
    if (*s == ';')
    {
        tok.type = TOK_COMMENT;
        const char *start = s;
        while (*s != '\n' && *s != '\0')
            s++;
        size_t len = s - start;
        if (len >= sizeof(tok.text))
        {
            fprintf(stderr, "Warning: token is too long: %s\n", tok.text);
            len = sizeof(tok.text) - 1;
        }
        strncpy(tok.text, start, len);
        tok.text[len] = '\0';
        *src = s;
        return tok;
    }

    // Handle labels and opcodes
    if ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || *s == '_')
    {
        const char *start = s;
        while ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') || *s == '_')
            s++;
        size_t len = s - start;
        if (len >= sizeof(tok.text))
        {
            fprintf(stderr, "Warning: token is too long: %s\n", tok.text);
            len = sizeof(tok.text) - 1;
        }
        strncpy(tok.text, start, len);
        tok.text[len] = '\0';

        // Check if it's a label (followed by ':')
        if (*s == ':')
        {
            tok.type = TOK_LABEL;
            s++; // consume ':'
        }
        else
        {
            // For .hb high-level language files, treat everything as identifiers/labels
            // Don't check for opcodes or targets - those are only in .bl block language
            tok.type = TOK_LABEL; // all identifiers are labels in high-level language
        }

        *src = s;
        return tok;
    }

    // Handle numbers (decimal or hex)

    if (*s >= '0' && *s <= '9')
    {
        const char *start = s;
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        {
            s += 2; // consume '0x'
            while ((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f') || (*s >= 'A' && *s <= 'F'))
                s++;
            tok.value = (int)strtol(start, NULL, 16);
        }
        else
        {
            while (*s >= '0' && *s <= '9')
                s++;
            tok.value = (int)strtol(start, NULL, 10);
        }
        size_t len = s - start;
        strncpy(tok.text, start, len);
        tok.text[len] = '\0';
        tok.type = TOK_NUMBER;
        *src = s;
        return tok;
    }

    // Handle single character tokens

    HANDLE_SINGLE(src, s, '.', TOK_DOT);
    HANDLE_SINGLE(src, s, ',', TOK_COMMA);
    HANDLE_SINGLE(src, s, ':', TOK_COLON);

    HANDLE_SINGLE(src, s, '(', TOK_BRACKET_LEFT);
    HANDLE_SINGLE(src, s, ')', TOK_BRACKET_RIGHT);
    HANDLE_SINGLE(src, s, '[', TOK_SQUARE_BRACKET_LEFT);
    HANDLE_SINGLE(src, s, ']', TOK_SQUARE_BRACKET_RIGHT);
    HANDLE_SINGLE(src, s, '{', TOK_CURLY_BRACKET_LEFT);
    HANDLE_SINGLE(src, s, '}', TOK_CURLY_BRACKET_RIGHT);

    HANDLE_DOUBLE(src, s, '+', '=', TOK_PLUS_EQUAL);
    HANDLE_SINGLE(src, s, '+', TOK_PLUS);
    HANDLE_DOUBLE(src, s, '-', '=', TOK_MINUS_EQUAL);
    HANDLE_SINGLE(src, s, '-', TOK_MINUS);
    HANDLE_DOUBLE(src, s, '*', '=', TOK_ASTERISK_EQUAL);
    HANDLE_SINGLE(src, s, '*', TOK_ASTERISK);
    HANDLE_DOUBLE(src, s, '/', '=', TOK_FORWARDSLASH_EQUAL);
    HANDLE_SINGLE(src, s, '/', TOK_FORWARDSLASH);

    HANDLE_SINGLE(src, s, '!', TOK_EXCLAMATION_MARK);
    HANDLE_SINGLE(src, s, '@', TOK_AT);
    HANDLE_SINGLE(src, s, '#', TOK_HASHTAG);

    HANDLE_SINGLE(src, s, '$', TOK_DOLLARSIGN);
    HANDLE_SINGLE(src, s, '%', TOK_PERCENT);
    HANDLE_SINGLE(src, s, '^', TOK_CARET);
    HANDLE_SINGLE(src, s, '&', TOK_AMPERSAND);

    HANDLE_SINGLE(src, s, '?', TOK_QUESTION_MARK);
    HANDLE_SINGLE(src, s, '~', TOK_TILDA);

    HANDLE_SINGLE(src, s, '<', TOK_LESSER);
    HANDLE_SINGLE(src, s, '>', TOK_GREATER);
    HANDLE_DOUBLE(src, s, '<', '=', TOK_LESSER_OR_EQUAL);
    HANDLE_DOUBLE(src, s, '>', '=', TOK_GREATER_OR_EQUAL);

    HANDLE_SINGLE(src, s, '=', TOK_EQUAL);
    HANDLE_DOUBLE(src, s, '!', '=', TOK_NOT_EQUAL);

    // Handle character literals

    if (*s == '\'')
    {
        tok.type = TOK_CHAR_LITERAL;
        s++; // first literal

        char c = handle_escape_sequence((char *)s);
        if (c == -1)
        {
            fprintf(stderr, "Unknown escape sequence in character literal at position %d\n", (int)(s - *src));
            exit(1);
        }

        tok.text[0] = c;
        tok.text[1] = '\0';

        s++; // single quote?
        if (*s != '\'')
        {
            fprintf(stderr, "Missing terminating character: \"\'\" at position %d\n", (int)(s - *src));
            exit(1);
        }
        s++; // next token

        *src = s;
        return tok;
    }

    // Strings

    if (*s == '\"')
    {
        s++;
        tok.type = TOK_STRING;
        const char *start = s;
        while (*s != '\"' && *s != '\0')
            s++;
        size_t len = s - start;
        if (len >= sizeof(tok.text))
        {
            fprintf(stderr, "Warning: token is too ling: %s\n", tok.text);
            len = sizeof(tok.text) - 1;
        }

        size_t out_index = 0;
        for (size_t i = 0; i < len; i++)
        {
            if (start[i] == '\\')
            {
                char esc = handle_escape_sequence((char *)&start[i]);
                if (esc == -1)
                {
                    fprintf(stderr, "Unknown escape sequence in string literal at position %d\n", (int)(s - *src));
                    exit(1);
                }
                tok.text[out_index++] = esc;
                i++; // skip next character as it's part of the escape sequence
            }
            else
            {
                tok.text[out_index++] = start[i];
            }
        }
        
        tok.text[len] = '\0';
        s++;

        *src = s;
        return tok;
    }

    // Unknown character

    fprintf(stderr, "Unknown character: \"%c\" at position %d\n", *s, (int)(s - *src));
    exit(1);

    return tok; // Unreachable
}