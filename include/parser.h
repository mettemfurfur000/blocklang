#ifndef BL_AST_H
#define BL_AST_H

#include "definitions.h"

typedef enum
{
    NODE_IDENTIFIER,
    NODE_NUMBER,
    NODE_CHAR,
    NODE_STRING,
    NODE_ARRAY,
    NODE_POINTER,
    NODE_ASSIGN,
    NODE_BINARY_OPERATOR,
    NODE_UNARY_OPERATOR,
    NODE_FUNCTION_DECLARATION,
    NODE_FUNCTION_CALL,
    NODE_IF,
    NODE_ELSE,
    NODE_WHILE,
    NODE_EXPRESSION,
    NODE_PROGRAM
} node_type;

struct node
{
    node_type type;
    u16 children[32]; // indices of child nodes
    u8 child_count;
    union
    {
        u8 number_value;
        char string[64];
    };
};

typedef struct node node;

// ============================================================================
// PARSER INTERFACE - Simple Pratt Parser with Operator Precedence Climbing
// ============================================================================
// Features:
// - Pratt parsing with operator precedence
// - Clean separation of primary expressions and infix/postfix operators
// - Support for arrays, function calls, assignments, and control flow
// - Graceful error handling with recovery
// - Room for expanding operator support without major refactoring

// Main parsing function
node parse_program(const char *src);

// AST utilities
void print_ast(node *root, int depth);
void add_child(node *parent, node *child);
void free_nodes(void);

// Node management
node *new_node(node_type type);
node *get_node_by_index(u16 index);
u16 node_index(node *p);
const char *node_type_str(node_type type);

#endif