#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/definitions.h"

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

u8 encode_instruction(int opcode, int target)
{
    return ((target & 0x0F) << 4) | (opcode & 0x0F);
}

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

typedef struct
{
    char name[32];
    u8 address;
} label_entry;

int get_label_address(label_entry labels[], int total_labels, const char *label)
{
    for (int i = 0; i < total_labels; i++)
    {
        if (strcmp(labels[i].name, label) == 0)
            return labels[i].address;
    }
    return -1; // Label not found
}

bool assemble_program(const char *source, void **dest, u8 *out_len)
{
    // if (!source || !dest || !out_len)
    //     return false;

    // First pass: determine length and collect labels
    const char *s = source;

    u8 program_length = 0;

    label_entry labels[256] = {};
    int total_labels = 0;

    while (true)
    {
        token tok = next_token(&s);
        if (tok.type == TOK_EOF)
            break;

        if (tok.type == TOK_LABEL)
        {
            // Store label with current program length as address
            bool found = false;

            for (int i = 0; i < total_labels; i++)
            {
                if (strcmp(labels[i].name, tok.text) == 0)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                strncpy(labels[total_labels].name, tok.text, sizeof(labels[total_labels].name));
                // labels[total_labels].address = program_length + 1;
                labels[total_labels].address = program_length;
                total_labels++;
            }
        }
        else if (tok.type == TOK_OPCODE)
        {
            program_length++;

            if (strcmp(tok.text, "halt") == 0 || strcmp(tok.text, "nop") == 0)
            {
                // No operand here
                continue;
            }

            token next = next_token(&s);

            // opcodes take (target | label | number | char literal | none ), depending on opcodes

            if (strcmp(tok.text, "jmp") == 0 || strcmp(tok.text, "jez") == 0 || strcmp(tok.text, "jnz") == 0 ||
                strcmp(tok.text, "jof") == 0)
            {
                if (next.type != TOK_LABEL && next.type != TOK_NUMBER && next.type != TOK_TARGET)
                {
                    fprintf(stderr, "Expected a label, number or a target after jump opcode [%s], got [%s]\n", tok.text,
                            next.text);
                    return false;
                }

                if (next.type == TOK_NUMBER || next.type == TOK_LABEL)
                {
                    // Labels and numbers in front of a jump opcode require an extra byte to be stored in bytecode
                    program_length++;
                }

                continue;
            }

            // the rest of possible arguments
            if (next.type == TOK_NUMBER || next.type == TOK_CHAR_LITERAL || next.type == TOK_LABEL)
            {
                program_length++; // numbers, literals and labels take extra byte and an ADJ target later
            }
            else if (next.type != TOK_TARGET)
            {
                fprintf(stderr, "Expected a target or a number after opcode [%s], got [%s]\n", tok.text, next.text);
                return false;
            }
        }
        else if (tok.type == TOK_COMMENT)
        {
            // Ignore comments
        }
        else if (tok.type == TOK_DOT)
        {
            // Dots followed by a string will be copied to bytecode as-is (including \0)
            token next = next_token(&s);

            if (next.type != TOK_STRING)
            {
                fprintf(stderr, "Expected a string after a dot, got [%s]\n", next.text);
                return false;
            }

            program_length += strlen(next.text) + 1;
        }
        else
        {
            fprintf(stderr, "Unexpected token type %d\n", tok.type);
            return false;
        }
    }

    // Allocate bytecode
    u8 *bytecode = (u8 *)malloc(program_length);

    if (!bytecode)
        return false;

    // Second pass: generate bytecode
    s = source;

    u8 *bc_ptr = bytecode;

    while (true)
    {
        token tok = next_token(&s);
        if (tok.type == TOK_EOF)
            break;
        else if (tok.type == TOK_LABEL || tok.type == TOK_COMMENT)
        {
            // Ignore labels in second pass
        }
        else if (tok.type == TOK_DOT)
        {
            // Dots followed by a string will be copied to bytecode as-is (including \0)
            token next = next_token(&s);

            if (next.type == TOK_STRING)
            {
                const u32 len = strlen(next.text) + 1;

                for (u32 i = 0; i < len; i++)
                {
                    *bc_ptr++ = next.text[i];
                }
            }
            else
            {
                fprintf(stderr, "Expected a string after a dot, got [%s]\n", next.text);
                return false;
            }
        }
        else if (tok.type == TOK_OPCODE)
        {
            // Generate bytecode for opcode
            int opcode = string_to_opcode(tok.text);

            if (opcode == ADJ)
            {
                fprintf(stderr, "Cannot use ADJ directly\n");
                return false;
            }

            if (opcode == HALT || opcode == NOP)
            {
                // No operand here
                *bc_ptr++ = encode_instruction(opcode, NIL);
                continue;
            }

            token next = next_token(&s); // Get the next token

            if (opcode == JMP || opcode == JEZ || opcode == JNZ ||
                opcode == JOF) // jumps can accept labels, numbers, targets
            {
                if (next.type == TOK_LABEL)
                {
                    // Lookup label address
                    int label_address = get_label_address(labels, total_labels, next.text);

                    if (label_address == -1)
                    {
                        fprintf(stderr, "Undefined label [%s] after jump opcode [%s]\n", next.text, tok.text);
                        return false;
                    }

                    *bc_ptr++ = encode_instruction(opcode, ADJ); // use ADJ as a special target for labels
                    *bc_ptr++ = (u8)label_address;               // write label address
                }
                else if (next.type == TOK_NUMBER)
                {
                    *bc_ptr++ = encode_instruction(opcode, ADJ);
                    *bc_ptr++ = next.value;
                }
                else if (next.type == TOK_TARGET)
                {
                    int target = string_to_target(next.text);

                    if (target == -1)
                    {
                        assert(false && "Unreachable - target validity already checked");
                    }

                    *bc_ptr++ = encode_instruction(opcode, target);
                }
                else
                {
                    assert(false && "Unreachable - invalid token");
                }

                continue;
            }

            // the rest of the instructions can take a target/number/label/char literal here

            if (next.type == TOK_CHAR_LITERAL)
            {
                *bc_ptr++ = encode_instruction(opcode, ADJ);
                *bc_ptr++ = next.text[0];
            }
            else if (next.type == TOK_LABEL)
            {
                *bc_ptr++ = encode_instruction(opcode, ADJ);

                // Lookup label address
                int label_address = get_label_address(labels, total_labels, next.text);

                if (label_address == -1)
                {
                    fprintf(stderr, "Undefined label [%s] for opcode [%s]\n", next.text, tok.text);
                    return false;
                }

                *bc_ptr++ = (u8)label_address;
            }
            else if (next.type == TOK_NUMBER)
            {
                *bc_ptr++ = encode_instruction(opcode, ADJ); // encode vale as adjacent byte
                *bc_ptr++ = next.value & 0xff;
            }
            else if (next.type == TOK_TARGET)
            {
                int target = string_to_target(next.text);

                if (target == -1)
                {
                    assert(false && "Unreachable - target validity already checked");
                }

                *bc_ptr++ = encode_instruction(opcode, target);
            }
            else
            {
                fprintf(stderr, "Expected a target, number, label, or a literal after opcode [%s], got [%s]\n",
                        tok.text, next.text);
                free(bytecode);
                return false;
            }
        }
        else
        {
            fprintf(stderr, "Unexpected token id %d, text:[%s]\n", tok.type, tok.text);
            exit(-1);
            // assert(false && "Unreachable - invalid token");
        }
    }

    // Output results

    if (dest)
        *dest = bytecode;
    else
        free(bytecode);

    if (out_len)
        *out_len = program_length;

    return true;
}

// Prints all tokens in said string
void debug_tokenize(const char *src)
{
    const char *s = src;

    while (true)
    {
        token tok = next_token(&s);
        if (tok.type == TOK_EOF)
            break;

        printf("%lld: %d - [%s]\n", s - src, tok.type, tok.text);
    }
}