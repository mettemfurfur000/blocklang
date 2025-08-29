#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/definitions.h"
#include "../include/tokenizer.h"

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

    u16 program_length = 0;

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

    if (program_length > BYTECODE_LIMIT)
    {
        fprintf(stderr, "Bytecode length exceeds the limit of %d bytes\n", BYTECODE_LIMIT);
        return false;
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
        *out_len = (u8)program_length;

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