#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/definitions.h"
#include "../include/tokenizer.h"

typedef struct
{
    char name[64];
    u8 address;
    u16 line;
    bool was_used;
} label_entry;

int get_label_address(label_entry labels[], int total_labels, const char *label)
{
    for (int i = 0; i < total_labels; i++)
    {
        if (strcmp(labels[i].name, label) == 0)
        {
            labels[i].was_used = true;
            return labels[i].address;
        }
    }
    return -1; // Label not found
}

void look_for_unused_labels(label_entry labels[], int total_labels)
{
    for (int i = 0; i < total_labels; i++)
        if (labels[i].was_used == false)
            fprintf(stderr, "Line %d: Warning - unused label %s\n", labels[i].line, labels[i].name);
}

void fix_opcode(token *t)
{
    if (is_valid_opcode(t->text))
        t->type = TOK_OPCODE;
}

void fix_target(token *t)
{
    if (is_valid_target(t->text))
        t->type = TOK_TARGET;
}

token new_token_asm(const char **src, int *lines_ret)
{
    token t = next_token(src, lines_ret);
    fix_opcode(&t);
    fix_target(&t);
    return t;
}

void bytecode_write(u8 *bytecode, u8 value, u16 *line_table, u8 *instruction_index, u16 cur_line)
{
    bytecode[*instruction_index] = value;
    line_table[*instruction_index] = cur_line;

    (*instruction_index)++;
    // debug output
    // printf("Line %d [%d] = %02x / %c\n", cur_line, *instruction_index - 1, value, value >= 32 && value <= 126 ? value : '.');
}

bool assemble_program(const char *source, void **dest, u8 *out_len, u16 *line_table)
{
    // First pass: determine length and collect labels
    const char *s = source;

    u16 program_length = 0;

    label_entry labels[256] = {};
    int total_labels = 0;
    int cur_line = 1;

    while (true)
    {
        token tok = new_token_asm(&s, &cur_line);
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
                if (strlen(tok.text) > sizeof(labels[total_labels].name))
                {
                    fprintf(stderr, "Line %d: label cannot be longer than %lld bytes long\n", cur_line,
                            sizeof(labels[total_labels].name));
                    exit(-1);
                }
                strncpy(labels[total_labels].name, tok.text, sizeof(labels[total_labels].name));
                // labels[total_labels].address = program_length + 1;
                labels[total_labels].address = program_length;
                labels[total_labels].line = cur_line;
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

            token next = new_token_asm(&s, &cur_line);

            // opcodes take (target | label | number | char literal | none ), depending on opcodes

            if (strcmp(tok.text, "jmp") == 0 || strcmp(tok.text, "jez") == 0 || strcmp(tok.text, "jnz") == 0 ||
                strcmp(tok.text, "jof") == 0)
            {
                if (next.type != TOK_LABEL && next.type != TOK_NUMBER && next.type != TOK_TARGET)
                {
                    fprintf(stderr,
                            "Line %d: Expected a label, number or a target after jump opcode \"%s\", got \"%s\"\n",
                            cur_line, tok.text, next.text);
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
                fprintf(stderr, "Line %d: Expected a target or a number after opcode \"%s\", got \"%s\"\n", cur_line,
                        tok.text, next.text);
                return false;
            }
        }
        else if (tok.type == TOK_COMMENT)
        {
            // Ignore comments
        }
        else if (tok.type == TOK_DOT)
        {
            // Dots followed by a string will be copied to bytecode as-is (\0 appended automatically)
            token next = new_token_asm(&s, &cur_line);

            if (next.type == TOK_STRING)
            {
                program_length += strlen(next.text) + 1;
            }
            else if (next.type == TOK_SQUARE_BRACKET_LEFT)
            {
                while (next.type != TOK_SQUARE_BRACKET_RIGHT)
                {
                    next = new_token_asm(&s, &cur_line);
                    if (next.type != TOK_NUMBER && next.type != TOK_SQUARE_BRACKET_RIGHT)
                    {
                        fprintf(stderr, "Line %d: Expected a number or a ] to close an array of numbers, got: %s\n",
                                cur_line, next.text);
                        return false;
                    }
                    program_length++;
                }
            }
            else
            {
                fprintf(stderr, "Line %d: Expected a string or an array of numbers after a dot, got \"%s\"\n", cur_line,
                        next.text);
                return false;
            }
        }
        else
        {
            fprintf(stderr, "Line %d: Unexpected token type %s\n", cur_line, tok_to_str(tok.type));
            return false;
        }
    }

    if (program_length > BYTECODE_LIMIT)
    {
        fprintf(stderr, "Line %d: Bytecode length exceeds the limit of %d bytes\n", cur_line, BYTECODE_LIMIT);
        return false;
    }

    u8 bytecode[256] = {}; // static allocation to avoid dynamic memory issues

    // Second pass: generate bytecode
    s = source;

    u8 bc_index = 0; // Track current instruction index
    cur_line = 1;

    while (true)
    {
        token tok = new_token_asm(&s, &cur_line);
        if (tok.type == TOK_EOF)
            break;
        else if (tok.type == TOK_LABEL || tok.type == TOK_COMMENT)
        {
            // Ignore labels in second pass
        }
        else if (tok.type == TOK_DOT)
        {
            // Dots followed by a string will be copied to bytecode as-is (\0 injected automatically)
            token next = new_token_asm(&s, &cur_line);

            if (next.type == TOK_STRING)
            {
                const u32 len = strlen(next.text) + 1;

                for (u32 i = 0; i < len; i++)
                    bytecode_write(bytecode, next.text[i], line_table, &bc_index, cur_line);
            }
            else if (next.type == TOK_SQUARE_BRACKET_LEFT)
            {
                while (next.type != TOK_SQUARE_BRACKET_RIGHT)
                {
                    next = new_token_asm(&s, &cur_line);
                    if (next.type != TOK_NUMBER && next.type != TOK_SQUARE_BRACKET_RIGHT)
                    {
                        fprintf(stderr, "Line %d: Expected a number or a ] to close an array of numbers, got: %s\n",
                                cur_line, next.text);
                        return false;
                    }
                    if (next.value > 0xff)
                        fprintf(stderr, "Line %d: Warning - number will not fit in a byte: %d\n", cur_line, next.value);
                    bytecode_write(bytecode, next.value & 0xff, line_table, &bc_index, cur_line);
                }
            }
            else
            {
                fprintf(stderr, "Line %d: Expected a string after a dot, got \"%s\"]\n", cur_line, next.text);
                return false;
            }
        }
        else if (tok.type == TOK_OPCODE)
        {
            // Generate bytecode for opcode
            int opcode = string_to_opcode(tok.text);

            if (opcode == HALT || opcode == NOP)
            {
                bytecode_write(bytecode, INSTRUCTION(opcode, NIL), line_table, &bc_index, cur_line);
                continue;
            }

            token next = new_token_asm(&s, &cur_line); // Get the next token

            if (opcode == JMP || opcode == JEZ || opcode == JNZ ||
                opcode == JOF) // jumps can accept labels, numbers, targets
            {
                if (next.type == TOK_LABEL)
                {
                    // Lookup label address
                    int label_address = get_label_address(labels, total_labels, next.text);

                    if (label_address == -1)
                    {
                        fprintf(stderr, "Line %d: Undefined label \"%s\" after jump opcode \"%s\"\n", cur_line,
                                next.text, tok.text);
                        return false;
                    }

                    bytecode_write(bytecode, INSTRUCTION(opcode, ADJ), line_table, &bc_index, cur_line);
                    bytecode_write(bytecode, (u8)label_address, line_table, &bc_index, cur_line);
                }
                else if (next.type == TOK_NUMBER)
                {
                    bytecode_write(bytecode, INSTRUCTION(opcode, ADJ), line_table, &bc_index, cur_line);
                    if (next.value > 0xff)
                        fprintf(stderr, "Line %d: Warning - number will not fit in a byte: %d\n", cur_line, next.value);
                    bytecode_write(bytecode, next.value & 0xff, line_table, &bc_index, cur_line);
                }
                else if (next.type == TOK_TARGET)
                {
                    int target = string_to_target(next.text);

                    if (target == -1)
                    {
                        fprintf(stderr, "Line %d: Unreachable - target validity already checked", cur_line);
                        exit(-1);
                    }

                    bytecode_write(bytecode, INSTRUCTION(opcode, target), line_table, &bc_index, cur_line);
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
                bytecode_write(bytecode, INSTRUCTION(opcode, ADJ), line_table, &bc_index, cur_line);
                bytecode_write(bytecode, next.text[0], line_table, &bc_index, cur_line);
            }
            else if (next.type == TOK_LABEL)
            {
                bytecode_write(bytecode, INSTRUCTION(opcode, ADJ), line_table, &bc_index, cur_line);

                // Lookup label address
                int label_address = get_label_address(labels, total_labels, next.text);

                if (label_address == -1)
                {
                    fprintf(stderr, "Line %d: Undefined label \"%s\" for opcode \"%s\"\n", cur_line, next.text,
                            tok.text);
                    return false;
                }

                bytecode_write(bytecode, (u8)label_address, line_table, &bc_index, cur_line);
            }
            else if (next.type == TOK_NUMBER)
            {
                bytecode_write(bytecode, INSTRUCTION(opcode, ADJ), line_table, &bc_index,
                               cur_line); // encode value as adjacent byte
                if (next.value > 0xff)
                    fprintf(stderr, "Line %d: Warning - number will not fit in a byte in front of an ADJ: %d\n",
                            cur_line, next.value);
                bytecode_write(bytecode, next.value & 0xff, line_table, &bc_index, cur_line);
            }
            else if (next.type == TOK_TARGET)
            {
                int target = string_to_target(next.text);

                if (target == -1)
                {
                    assert(false && "Unreachable - target validity already checked");
                }

                bytecode_write(bytecode, INSTRUCTION(opcode, target), line_table, &bc_index, cur_line);
            }
            else
            {
                fprintf(stderr,
                        "Line %d: Expected a target, number, label, or a literal after opcode \"%s\", got \"%s\"\n",
                        cur_line, tok.text, next.text);
                return false;
            }
        }
        else
        {
            fprintf(stderr, "Line %d: Unexpected token id %d, text:\"%s\"\n", cur_line, tok.type, tok.text);
            exit(-1);
            // assert(false && "Unreachable - invalid token");
        }
    }

    look_for_unused_labels(labels, total_labels);

    // Output results

    if (dest)
    {
        *dest = calloc(1, program_length);
        if (!*dest)
            return false;
        memcpy(*dest, bytecode, program_length);
    }

    if (out_len)
        *out_len = (u8)program_length;

    return true;
}

// Prints all tokens in said string
void debug_tokenize(const char *src)
{
    const char *s = src;

    i32 line = 1;

    while (true)
    {
        token tok = new_token_asm(&s, &line);
        if (tok.type == TOK_EOF)
            break;

        printf("Line\t%d: \"%s\" \t \"%s\"\n", line, tok_to_str(tok.type), tok.text);
    }
}