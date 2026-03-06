#include "../include/codegen.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOTAL_REGISTERS 4
#define MAX_VARIABLES 16
#define OUTPUT_BUFFER_SIZE (64 * 1024)
#define LABEL_PREFIX "__codegen_"

static char output_buffer[OUTPUT_BUFFER_SIZE];
static size_t output_pos = 0;
static u32 label_counter = 0;
static bool allocated_registers[TOTAL_REGISTERS] = {};
static char *reg_names[] = {
    "RG0",
    "RG1",
    "RG2",
    "RG3",
};

typedef struct
{
    char name[32];
    const char *reg;
    bool in_use;
} variable_t;

static variable_t variables[MAX_VARIABLES];
static u32 var_count = 0;

static void reset_variables(void)
{
    memset(variables, 0, sizeof(variables));
    var_count = 0;
}

static const char *find_variable(const char *name)
{
    for (u32 i = 0; i < var_count; i++)
        if (strcmp(variables[i].name, name) == 0)
            return variables[i].reg;
    return NULL;
}

static const char *alloc_reg()
{
    for (u32 i = 0; i < TOTAL_REGISTERS; i++)
        if (!allocated_registers[i])
        {
            allocated_registers[i] = true;
            return reg_names[i];
        }
    ERROR_ABORT("ran out of registers");
}

static void free_reg(const char *name)
{
    for (u32 i = 0; i < TOTAL_REGISTERS; i++)
        if (name[2] == reg_names[i][2])
        {
            allocated_registers[i] = false;
            return;
        }
    ERROR_ABORT("unknown register: %s", name);
}

static const char *alloc_variable(const char *name)
{
    const char *existing = find_variable(name);
    if (existing)
        return existing;
    
    if (var_count >= MAX_VARIABLES)
        ERROR_ABORT("too many variables");
    
    const char *reg = alloc_reg();
    strncpy(variables[var_count].name, name, sizeof(variables[var_count].name) - 1);
    variables[var_count].reg = reg;
    variables[var_count].in_use = true;
    var_count++;
    
    return reg;
}

static void emit(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    output_pos += vsnprintf(output_buffer + output_pos, OUTPUT_BUFFER_SIZE - output_pos, fmt, args);
    va_end(args);
}

static void emit_line(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    output_pos += vsnprintf(output_buffer + output_pos, OUTPUT_BUFFER_SIZE - output_pos, fmt, args);
    output_pos += snprintf(output_buffer + output_pos, OUTPUT_BUFFER_SIZE - output_pos, "\n");
    va_end(args);
}

static const char *gen_label()
{
    static char label[64];
    snprintf(label, sizeof(label), "%s%u", LABEL_PREFIX, label_counter++);
    return label;
}

static const char *node_to_string(node *n)
{
    static char buf[64];
    if (!n)
        return "NULL";
    switch (n->type)
    {
    case NODE_IDENTIFIER:
    case NODE_STRING:
        return n->string;
    case NODE_NUMBER:
        snprintf(buf, sizeof(buf), "%u", n->number_value);
        return buf;
    case NODE_CHAR:
        buf[0] = n->string[0];
        buf[1] = '\0';
        return buf;
    default:
        return "?";
    }
}

static void gen_expression(node *n, const char *dest);

static void gen_expression(node *n, const char *dest)
{
    if (!n)
        return;

    switch (n->type)
    {
    case NODE_NUMBER:
        emit_line("    get %s", node_to_string(n));
        if (dest && strcmp(dest, "ACC") != 0)
            emit_line("    put %s", dest);
        break;

    case NODE_IDENTIFIER:
    {
        const char *reg = find_variable(n->string);
        if (reg)
        {
            emit_line("    get %s", reg);
            if (dest && strcmp(dest, "ACC") != 0)
                emit_line("    put %s", dest);
        }
        else
        {
            emit_line("    get %s", n->string);
            if (dest && strcmp(dest, "ACC") != 0)
                emit_line("    put %s", dest);
        }
        break;
    }

    case NODE_CHAR:
        emit_line("    get %s", n->string);
        if (dest && strcmp(dest, "ACC") != 0)
            emit_line("    put %s", dest);
        break;

    case NODE_BINARY_OPERATOR:
    {
        const char *op = n->string;
        node *left = get_node_by_index(n->children[0]);
        node *right = get_node_by_index(n->children[1]);

        if (strcmp(op, "+") == 0)
        {
            gen_expression(left, "ACC");
            emit_line("    put RG0             ; save left operand");
            gen_expression(right, "ACC");
            emit_line("    add RG0             ; ACC = ACC + saved_left");
            free_reg("RG0");
        }
        else if (strcmp(op, "-") == 0)
        {
            gen_expression(left, "ACC");
            emit_line("    put RG0             ; save left operand");
            gen_expression(right, "ACC");
            emit_line("    sub RG0             ; ACC = saved_left - ACC");
            free_reg("RG0");
        }
        else if (strcmp(op, "*") == 0)
        {
            gen_expression(left, "ACC");
            emit_line("    put RG0             ; save left operand");
            gen_expression(right, "ACC");
            emit_line("    mlt RG0             ; ACC = ACC * saved_left");
            free_reg("RG0");
        }
        else if (strcmp(op, "/") == 0)
        {
            gen_expression(left, "ACC");
            emit_line("    put RG0             ; save left operand");
            gen_expression(right, "ACC");
            emit_line("    div RG0             ; ACC = saved_left / ACC");
            free_reg("RG0");
        }
        else if (strcmp(op, "%") == 0)
        {
            gen_expression(left, "ACC");
            emit_line("    put RG0             ; save left operand");
            gen_expression(right, "ACC");
            emit_line("    mod RG0             ; ACC = saved_left mod ACC");
            free_reg("RG0");
        }
        else
        {
            ERROR_ABORT("unknown operator: %s", op);
        }

        if (dest && strcmp(dest, "ACC") != 0)
            emit_line("    put %s", dest);
        break;
    }

    case NODE_UNARY_OPERATOR:
    {
        const char *op = n->string;
        node *operand = get_node_by_index(n->children[0]);

        if (strcmp(op, "*") == 0)
        {
            gen_expression(operand, "ACC");
            emit_line("    get REF");
        }
        else if (strcmp(op, "&") == 0)
        {
        }
        else
        {
            ERROR_ABORT("unknown unary operator: %s", op);
        }

        if (dest && strcmp(dest, "ACC") != 0)
            emit_line("    put %s", dest);
        break;
    }

    case NODE_ASSIGN:
    {
        node *lhs = get_node_by_index(n->children[0]);
        node *rhs = get_node_by_index(n->children[1]);

        const char *lhs_reg = alloc_variable(lhs->string);

        if (rhs->type == NODE_BINARY_OPERATOR || rhs->type == NODE_UNARY_OPERATOR || rhs->type == NODE_FUNCTION_CALL)
        {
            gen_expression(rhs, lhs_reg);
        }
        else
        {
            const char *val = node_to_string(rhs);
            emit_line("    get %s", val);
            emit_line("    put %s", lhs_reg);
        }
        break;
    }

    case NODE_FUNCTION_CALL:
    {
        const char *fn_name = n->string;

        for (u8 i = 0; i < n->child_count; i++)
        {
            node *arg = get_node_by_index(n->children[i]);
            emit("    push %s", node_to_string(arg));
            emit_line("           ; arg %d", i + 1);
        }

        emit_line("    push CUR            ; push return address");
        emit_line("    jmp %s", fn_name);

        break;
    }

    default:
        ERROR_ABORT("unsupported expression type: %s", node_type_str(n->type));
    }
}

static void gen_statement(node *n);

static void gen_block(node *n)
{
    for (u8 i = 0; i < n->child_count; i++)
    {
        node *child = get_node_by_index(n->children[i]);
        gen_statement(child);
    }
}

static void gen_statement(node *n)
{
    if (!n)
        return;

    switch (n->type)
    {
    case NODE_ASSIGN:
    {
        node *lhs = get_node_by_index(n->children[0]);
        node *rhs = get_node_by_index(n->children[1]);

        const char *lhs_reg = alloc_variable(lhs->string);

        const char *val = node_to_string(rhs);

        if (rhs->type == NODE_BINARY_OPERATOR || rhs->type == NODE_UNARY_OPERATOR || rhs->type == NODE_FUNCTION_CALL)
        {
            gen_expression(rhs, lhs_reg);
        }
        else if (rhs->type == NODE_ARRAY)
        {
        }
        else
        {
            emit_line("    get %s", val);
            emit_line("    put %s", lhs_reg);
        }
        break;
    }

    case NODE_FUNCTION_CALL:
    {
        const char *fn_name = n->string;

        for (u8 i = 0; i < n->child_count; i++)
        {
            node *arg = get_node_by_index(n->children[i]);
            emit("    push %s", node_to_string(arg));
            emit_line("           ; arg %d", i + 1);
        }

        emit_line("    push CUR            ; push return address");
        emit_line("    jmp %s", fn_name);
        break;
    }

    case NODE_FUNCTION_DECLARATION:
    {
        const char *fn_name = n->string;
        node *body = get_node_by_index(n->children[0]);

        emit_line("%s:", fn_name);

        if (n->child_count > 1)
        {
            emit_line("    pop RG3             ; return addr");
            for (u8 i = 1; i < n->child_count; i++)
            {
                node *param = get_node_by_index(n->children[i]);
                emit_line("    pop %s             ; param %d", param->string, i);
            }
            emit_line("    push RG3            ; stash ret addr");
        }

        gen_block(body);

        emit_line("    jmp __f_ret_void");
        break;
    }

    case NODE_WHILE:
    {
        const char *loop_start = gen_label();
        const char *loop_end = gen_label();

        emit_line("%s:", loop_start);

        node *cond = get_node_by_index(n->children[0]);
        node *body = get_node_by_index(n->children[1]);

        gen_expression(cond, "ACC");
        emit_line("    jez %s", loop_end);

        gen_block(body);

        emit_line("    jmp %s", loop_start);
        emit_line("%s:", loop_end);
        break;
    }

    case NODE_EXPRESSION:
        gen_expression(n, NULL);
        break;

    default:
        break;
    }
}

const char *codegen(node root)
{
    output_pos = 0;
    label_counter = 0;
    memset(allocated_registers, 0, sizeof(allocated_registers));
    reset_variables();
    output_buffer[0] = '\0';

    for (u8 i = 0; i < root.child_count; i++)
    {
        node *child = get_node_by_index(root.children[i]);

        if (child->type == NODE_ARRAY)
        {
            emit_line("%s:", child->string);
            emit("    . [");
            for (u8 j = 0; j < child->child_count; j++)
            {
                node *elem = get_node_by_index(child->children[j]);
                emit(" %s", node_to_string(elem));
            }
            emit_line(" ]");
        }
        else
        {
            gen_statement(child);
        }
    }

    emit_line("\n__f_ret_void:");
    emit_line("    pop ACC             ; pop return address to acc");
    emit_line("    add 3               ; jmp <label> takes 2 bytes");
    emit_line("    jmp ACC");

    return output_buffer;
}
