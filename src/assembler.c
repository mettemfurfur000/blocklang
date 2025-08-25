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
    TOK_COLON,
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

int string_to_opcode(const char *str)
{
    const char *opcodes[] = {"nop", "wait", "add", "sub", "mlt", "div", "mod", "get",
                             "put", "push", "pop", "jmp", "jez", "jnz", "jof", "halt"};
    for (size_t i = 0; i < sizeof(opcodes) / sizeof(opcodes[0]); i++)
    {
        if (strcmp(str, opcodes[i]) == 0)
            return (u8)i & 0x0F;
    }
    return -1;
}

int string_to_target(const char *str)
{
    const char *targets[] = {"STK", "ACC", "RG0", "RG1", "RG2", "RG3", "ADJ", "UP", "RIG", "DWN", "LFT", "ANY", "NIL"};
    for (size_t i = 0; i < sizeof(targets) / sizeof(targets[0]); i++)
    {
        if (strcmp(str, targets[i]) == 0)
            return (u8)i & 0x0F;
    }
    return -1;
}

static token unknown_identifiers[256] = {};
static int total_unknown = 0;

static const char *valid_opcodes[] = {"nop", "wait", "add", "sub", "mlt", "div", "mod", "get",
                                      "put", "push", "pop", "jmp", "jez", "jnz", "jof", "halt"};

static const char *valid_targets[] = {"STK", "ACC", "RG0", "RG1", "RG2", "RG3",
                                      //"ADJ", // adj is only used internally as a target for literal values
                                      "UP", "RIG", "DWN", "LFT", "ANY", "NIL"};

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

    // Skip whitespace
    while (*s == ' ' || *s == '\t')
        s++;

    // Skip newlines
    while (*s == '\n' || *s == '\r')
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
            unknown_identifiers[total_unknown++] = tok;
            tok.type = TOK_LABEL; // treat as label for now
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

    // Unknown character

    fprintf(stderr, "Unknown character: %c\n", *s);
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
                strncpy(labels[total_labels].name, tok.text, sizeof(labels[total_labels].name) - 1);
                labels[total_labels].address = program_length + 1;
                total_labels++;

                // Labels require extra byte to be stored in bytecode

                program_length++;
            }
        }
        else if (tok.type == TOK_OPCODE)
        {
            program_length++; // opcode + (target | label | number | none )

            if (strcmp(tok.text, "halt") == 0 || strcmp(tok.text, "nop") == 0)
            {
                // No operand here
                continue;
            }

            token next = next_token(&s);

            if (strcmp(tok.text, "jmp") == 0 || strcmp(tok.text, "jez") == 0 || strcmp(tok.text, "jnz") == 0 ||
                strcmp(tok.text, "jof") == 0)
            {
                if (next.type != TOK_LABEL && next.type != TOK_NUMBER && next.type != TOK_TARGET)
                {
                    fprintf(stderr, "Expected a label, number or a target after jump opcode [%s], got [%s]\n", tok.text,
                            next.text);
                    return false;
                }
            }
            // the rest of the instructions expect a target/number here
            else if (next.type == TOK_NUMBER)
            {
                // numbers require an extra byte
                program_length++;
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
        else if (tok.type == TOK_LABEL)
        {
            // Ignore labels in second pass
        }
        else if (tok.type == TOK_OPCODE)
        {
            // Generate bytecode for opcode
            int opcode = string_to_opcode(tok.text);

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
                    *bc_ptr++ = label_address;                   // write label address
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

            // the rest of the instructions expect a target/number here

            if (next.type == TOK_NUMBER)
            {
                *bc_ptr++ = encode_instruction(opcode, ADJ);
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
                fprintf(stderr, "Expected a target or a number after opcode [%s], got [%s]\n", tok.text, next.text);
                free(bytecode);
                return false;
            }
        }
        else
        {
            fprintf(stderr, "Impossible: %d - %s", tok.type, tok.text);
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

    // Clear unknown identifiers for next assembly
    total_unknown = 0;

    return true;
}