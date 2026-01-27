#ifndef BL_AST_H
#define BL_AST_H

#include "definitions.h"

typedef struct {

} ast_node_base;

typedef struct {
    const char identifier[64];
    u32 length;
} ast_node_var;

typedef struct {
    
} ast_node_expression;

typedef struct {
    ast_node_var *var;
    // right side as an expression
} ast_node_assign;

#endif