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

// Advances the source pointer and returns the next token
token next_token(const char **src)
{
    token tok = {0};
    const char *s = *src;

    // Skip whitespace and newlines
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;

    if (*s == '\0')
    {
        tok.type = TOK_EOF;
        *src = s;
        return tok;
    }

    // Handle comments
    if (*s == ';')
    {
        tok.type = TOK_COMMENT;
        const char *start = s;
        while (*s != '\n' && *s != '\0')
            s++;
        size_t len = s - start;
        if (len >= sizeof(tok.text))
            len = sizeof(tok.text) - 1;
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
            len = sizeof(tok.text) - 1;
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
            // Check if it's an opcode or target
            if (is_valid_opcode(tok.text))
            {
                tok.type = TOK_OPCODE;
                *src = s;
                return tok;
            }

            if (is_valid_target(tok.text))
            {
                tok.type = TOK_TARGET;
                *src = s;
                return tok;
            }

            // Unknown identifier
            tok.type = TOK_LABEL; // treat as a label for now
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
        tok.type = TOK_NUMBER;
        *src = s;
        return tok;
    }

    // Handle single character tokens

    if (*s == '.')
    {
        tok.type = TOK_DOT;
        s++;
        *src = s;
        return tok;
    }

    if (*s == ',')
    {
        tok.type = TOK_COMMA;
        s++;
        *src = s;
        return tok;
    }

    if (*s == ':')
    {
        tok.type = TOK_COLON;
        s++;
        *src = s;
        return tok;
    }

    // Handle character literals

    if (*s == '\'')
    {
        tok.type = TOK_CHAR_LITERAL;
        s++;            // first literal
        if (*s == '\\') // escape sequence
        {
            s++; // char after backslash
            switch (*s)
            {
            case 'a':
                tok.text[0] = 0x07;
                break;
            case 'b':
                tok.text[0] = 0x08;
                break;
            case 'e':
                tok.text[0] = 0x1b;
                break;
            case 'f':
                tok.text[0] = 0x0c;
                break;
            case 'n':
                tok.text[0] = 0x0a;
                break;
            case 'r':
                tok.text[0] = 0x0d;
                break;
            case 't':
                tok.text[0] = 0x09;
                break;
            case 'v':
                tok.text[0] = 0x0B;
                break;
            case '\\':
                tok.text[0] = '\\';
                break;
            case '\'':
                tok.text[0] = '\'';
                break;
            case '\"':
                tok.text[0] = '\"';
                break;
            case '?':
                tok.text[0] = '\?';
                break;
            default:
                fprintf(stderr, "Unknown escape sequence symbol: \"%c\" at position %d\n", *s, (int)(s - *src));
                exit(1);
            }
        }
        else // a character
            tok.text[0] = *s;

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
            len = sizeof(tok.text) - 1;
        strncpy(tok.text, start, len);
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