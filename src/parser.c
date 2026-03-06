#include "../include/parser.h"
#include "../include/tokenizer.h"

#include <stdio.h>
#include <string.h>

// ============================================================================
// NODE MANAGEMENT
// ============================================================================

static node parse_nodes[1024] = {};
static u32 last_node = 0;
static u32 error_count = 0;

const char *node_type_str(node_type type)
{
    switch (type)
    {
        CASE(NODE_IDENTIFIER)
        CASE(NODE_NUMBER)
        CASE(NODE_CHAR)
        CASE(NODE_STRING)
        CASE(NODE_ARRAY)
        CASE(NODE_POINTER)
        CASE(NODE_ASSIGN)
        CASE(NODE_BINARY_OPERATOR)
        CASE(NODE_UNARY_OPERATOR)
        CASE(NODE_FUNCTION_DECLARATION)
        CASE(NODE_FUNCTION_CALL)
        CASE(NODE_IF)
        CASE(NODE_ELSE)
        CASE(NODE_WHILE)
        CASE(NODE_EXPRESSION)
        CASE(NODE_PROGRAM)
    default:
        return "UNKNOWN_NODE_TYPE";
    }
}

void free_nodes()
{
    last_node = 0;
    error_count = 0;
    memset(parse_nodes, 0, sizeof(parse_nodes));
}

node *new_node(node_type type)
{
    assert(last_node < sizeof(parse_nodes) / sizeof(parse_nodes[0]));
    assert(type >= 0 && type < NODE_PROGRAM + 1);

    node *retptr = parse_nodes + last_node;
    *retptr = (node){};
    retptr->type = type;
    retptr->child_count = 0;
    last_node++;

    assert(retptr);

    return retptr;
}

u16 node_index(node *p)
{
    if (!p || p < parse_nodes || p > parse_nodes + last_node)
        return 0;
    return (u16)(p - parse_nodes);
}

node *get_node_by_index(u16 index)
{
    assert(index < last_node);
    return &parse_nodes[index];
}

void add_child(node *parent, node *child)
{
    if (!parent || !child)
        return;

    const u32 MAX_CHILDREN = sizeof(parent->children) / sizeof(parent->children[0]);

    if (parent->child_count >= MAX_CHILDREN)
    {
        fprintf(stderr, "Warning: Node has too many children (max %d)\n", MAX_CHILDREN);
        return;
    }

    parent->children[parent->child_count] = node_index(child);
    parent->child_count++;
}

// ============================================================================
// PARSER STATE & UTILITIES
// ============================================================================

typedef struct
{
    token current;
    const char *src;
    const char *read_ptr;
    i32 line;
    bool had_error;
} parse_state;

#define __FILENAME__ (strrchr(__FILE__, SEPARATOR) ? strrchr(__FILE__, SEPARATOR) + 1 : __FILE__)

#define COMPILER_ERROR(format, ...)                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        fprintf(stderr, "%s:%d - Line %d: " format "\n", __FILENAME__, __LINE__, st->line, ##__VA_ARGS__);             \
        error_count++;                                                                                                 \
        st->had_error = true;                                                                                          \
    } while (0)

void advance_token(parse_state *st)
{
    do
    {
        st->current = next_token(&st->read_ptr, &st->line);
    } while (st->current.type == TOK_COMMENT);
}

bool is_type(parse_state *st, token_type type)
{
    return st->current.type == type;
}

bool match(parse_state *st, token_type type)
{
    bool result = is_type(st, type);
    if (result)
        advance_token(st);
    return result;
}

node *create_node_from_token(parse_state *st, node_type type)
{
    node *n = new_node(type);

    if (type == NODE_NUMBER)
    {
        n->number_value = st->current.value;
    }
    else
    {
        strncpy(n->string, st->current.text, sizeof(n->string) - 1);
        n->string[sizeof(n->string) - 1] = '\0';
    }
    advance_token(st);
    return n;
}

// ============================================================================
// PRATT PARSER - PRECEDENCE CLIMBING
// ============================================================================

typedef enum
{
    PREC_NONE = 0,
    PREC_ASSIGN = 1,         // =, +=, -=, etc.
    PREC_LOGICAL_OR = 2,     // ||
    PREC_LOGICAL_AND = 3,    // &&
    PREC_EQUALITY = 4,       // ==, !=
    PREC_COMPARISON = 5,     // <, >, <=, >=
    PREC_ADDITIVE = 6,       // +, -
    PREC_MULTIPLICATIVE = 7, // *, /, %
    PREC_UNARY = 8,          // !, -, ~, ++, --, *
    PREC_POSTFIX = 9,        // (), [], .
    PREC_PRIMARY = 10
} precedence;

// Forward declarations
static node parse_expression(parse_state *st, precedence min_prec);
static node parse_statement(parse_state *st);
static node parse_block(parse_state *st);

// Get precedence for a token type
static precedence get_precedence(token_type type)
{
    switch (type)
    {
    case TOK_EQUAL:
    case TOK_PLUS_EQUAL:
    case TOK_MINUS_EQUAL:
    case TOK_ASTERISK_EQUAL:
    case TOK_FORWARDSLASH_EQUAL:
        return PREC_ASSIGN;

    case TOK_PLUS:
    case TOK_MINUS:
        return PREC_ADDITIVE;

    case TOK_ASTERISK:
    case TOK_FORWARDSLASH:
    case TOK_PERCENT:
        return PREC_MULTIPLICATIVE;

    case TOK_LESSER:
    case TOK_GREATER:
    case TOK_LESSER_OR_EQUAL:
    case TOK_GREATER_OR_EQUAL:
        return PREC_COMPARISON;

    case TOK_NOT_EQUAL:
        return PREC_EQUALITY;

    case TOK_BRACKET_LEFT:
    case TOK_SQUARE_BRACKET_LEFT:
    case TOK_DOT:
        return PREC_POSTFIX;

    default:
        return PREC_NONE;
    }
}

#define PARSE_EXPECT(tok_type, message)                                                                                \
    if (!match(st, tok_type))                                                                                          \
    {                                                                                                                  \
        printf("expected %s, got %s\n", tok_to_str(tok_type), tok_to_str(st->current.type));                           \
        COMPILER_ERROR(message);                                                                                       \
        return (node){};                                                                                               \
    }

#define PARSE_EXPECT_DONT_EAT(type, message)                                                                           \
    if (!is_type(st, type))                                                                                            \
    {                                                                                                                  \
        COMPILER_ERROR(message);                                                                                       \
        return (node){};                                                                                               \
    }

// Null denotation: prefix operators and primary expressions
static node parse_primary(parse_state *st)
{
    if (st->had_error)
        return (node){};

    // Numbers
    if (is_type(st, TOK_NUMBER))
        return *create_node_from_token(st, NODE_NUMBER);

    // Character literals
    if (is_type(st, TOK_CHAR_LITERAL))
        return *create_node_from_token(st, NODE_CHAR);

    // String literals
    if (is_type(st, TOK_STRING))
        return *create_node_from_token(st, NODE_STRING);

    // Array literal: [1, 2, 3]
    if (is_type(st, TOK_SQUARE_BRACKET_LEFT))
    {
        advance_token(st);
        node *array = new_node(NODE_ARRAY);

        if (!is_type(st, TOK_SQUARE_BRACKET_RIGHT))
        {
            do
            {
                node elem = parse_expression(st, PREC_NONE);
                if (st->had_error)
                    return (node){};

                node *elem_node = new_node(elem.type);
                *elem_node = elem;
                add_child(array, elem_node);

            } while (match(st, TOK_COMMA));
        }

        PARSE_EXPECT(TOK_SQUARE_BRACKET_RIGHT, "Expected ']' to close array literal")

        return *array;
    }

    // Parenthesized expression
    if (is_type(st, TOK_BRACKET_LEFT))
    {
        advance_token(st);
        node expr = parse_expression(st, PREC_NONE);
        if (st->had_error)
            return (node){};

        PARSE_EXPECT(TOK_BRACKET_RIGHT, "Expected ')' to close expression");
        return expr;
    }

    // Unary operators: -, !, ~, *, &
    if (is_type(st, TOK_MINUS) || is_type(st, TOK_EXCLAMATION_MARK) || is_type(st, TOK_TILDA) ||
        is_type(st, TOK_ASTERISK) || is_type(st, TOK_AMPERSAND))
    {
        token op = st->current;
        advance_token(st);

        node *unary = new_node(NODE_UNARY_OPERATOR);

        strncpy(unary->string, op.text, sizeof(unary->string) - 1);
        unary->string[sizeof(unary->string) - 1] = '\0';

        node operand = parse_expression(st, PREC_UNARY);
        if (st->had_error)
            return (node){};

        node *operand_node = new_node(operand.type);
        *operand_node = operand;
        add_child(unary, operand_node);

        return *unary;
    }

    // Identifiers and keywords
    if (is_type(st, TOK_LABEL))
    {
        return *create_node_from_token(st, NODE_IDENTIFIER);
    }

    COMPILER_ERROR("Unexpected token '%s' in expression, type '%s'", st->current.text, tok_to_str(st->current.type));
    st->had_error = true;
    return (node){};
}

// Left denotation: infix and postfix operators
static node parse_infix(parse_state *st, node left)
{
    if (st->had_error)
        return (node){};

    token_type op_type = st->current.type;
    token op = st->current;
    precedence prec = get_precedence(op_type);

    // Function call: identifier(args)
    if (op_type == TOK_BRACKET_LEFT)
    {
        advance_token(st);

        node *call = new_node(NODE_FUNCTION_CALL);

        // Function name
        node *name = new_node(left.type);
        *name = left;
        add_child(call, name);

        // Parse arguments
        if (!is_type(st, TOK_BRACKET_RIGHT))
        {
            do
            {
                node arg = parse_expression(st, PREC_NONE);
                if (st->had_error)
                    return (node){};

                node *arg_node = new_node(arg.type);
                *arg_node = arg;
                add_child(call, arg_node);

            } while (match(st, TOK_COMMA));
        }

        PARSE_EXPECT(TOK_BRACKET_RIGHT, "Expected ')' to close function call");

        return *call;
    }

    // Array subscript: expr[index]
    if (op_type == TOK_SQUARE_BRACKET_LEFT)
    {
        advance_token(st);

        node *subscript = new_node(NODE_ARRAY);
        node *base = new_node(left.type);

        *base = left;
        add_child(subscript, base);

        node index = parse_expression(st, PREC_NONE);
        if (st->had_error)
            return (node){};

        node *index_node = new_node(index.type);
        *index_node = index;
        add_child(subscript, index_node);

        PARSE_EXPECT(TOK_SQUARE_BRACKET_RIGHT, "Expected ']' to close subscript")

        return *subscript;
    }

    // Assignment operators: =, +=, -=, etc.
    if (op_type == TOK_EQUAL || op_type == TOK_PLUS_EQUAL || op_type == TOK_MINUS_EQUAL ||
        op_type == TOK_ASTERISK_EQUAL || op_type == TOK_FORWARDSLASH_EQUAL)
    {
        advance_token(st);

        node *assign = new_node(NODE_ASSIGN);

        strncpy(assign->string, op.text, sizeof(assign->string) - 1);
        assign->string[sizeof(assign->string) - 1] = '\0';

        node *lhs = new_node(left.type);
        *lhs = left;
        add_child(assign, lhs);

        node rhs = parse_expression(st, prec);
        if (st->had_error)
            return (node){};

        node *rhs_node = new_node(rhs.type);
        *rhs_node = rhs;
        add_child(assign, rhs_node);

        return *assign;
    }

    // Binary operators: +, -, *, /, <, >, ==, !=, etc.
    advance_token(st);

    node *binary = new_node(NODE_BINARY_OPERATOR);

    strncpy(binary->string, op.text, sizeof(binary->string) - 1);
    binary->string[sizeof(binary->string) - 1] = '\0';

    node *lhs = new_node(left.type);
    *lhs = left;
    add_child(binary, lhs);

    // For left-associative operators, use prec + 1
    node rhs = parse_expression(st, prec + 1);
    if (st->had_error)
        return (node){};

    node *rhs_node = new_node(rhs.type);
    *rhs_node = rhs;
    add_child(binary, rhs_node);

    return *binary;
}

// Expression parsing with operator precedence
static node parse_expression(parse_state *st, precedence min_prec)
{
    if (st->had_error)
        return (node){};

    // Parse left-hand side (primary)
    node left = parse_primary(st);
    if (st->had_error)
        return (node){};

    // Climb the precedence tower
    while (!st->had_error && get_precedence(st->current.type) > min_prec)
    {
        // Prevent infinite loops: stop on EOF
        if (is_type(st, TOK_EOF))
            break;

        left = parse_infix(st, left);
    }

    return left;
}

// ============================================================================
// STATEMENT PARSING
// ============================================================================

static node parse_block(parse_state *st)
{
    if (st->had_error)
        return (node){};

    PARSE_EXPECT(TOK_CURLY_BRACKET_LEFT, "Expected '{' to start block")

    node *block = new_node(NODE_PROGRAM);

    while (!is_type(st, TOK_CURLY_BRACKET_RIGHT) && !is_type(st, TOK_EOF) && !st->had_error)
    {
        node stmt = parse_statement(st);
        if (!st->had_error && stmt.type != (node_type)0)
        {
            node *stmt_node = new_node(stmt.type);
            *stmt_node = stmt;
            add_child(block, stmt_node);
        }
    }

    PARSE_EXPECT(TOK_CURLY_BRACKET_RIGHT, "Expected '}' to close block");

    return *block;
}

static node parse_if_statement(parse_state *st)
{
    if (st->had_error)
        return (node){};

    advance_token(st); // consume 'if'

    PARSE_EXPECT(TOK_BRACKET_LEFT, "Expected '(' after 'if'");

    node *if_node = new_node(NODE_IF);

    // Condition
    node cond = parse_expression(st, PREC_NONE);
    if (st->had_error)
        return (node){};

    node *cond_node = new_node(cond.type);
    *cond_node = cond;
    add_child(if_node, cond_node);

    PARSE_EXPECT(TOK_BRACKET_RIGHT, "Expected ')' after if condition");

    // Body
    node body = parse_block(st);
    if (st->had_error)
        return (node){};

    node *body_node = new_node(body.type);
    *body_node = body;
    add_child(if_node, body_node);

    return *if_node;
}

static node parse_while_statement(parse_state *st)
{
    if (st->had_error)
        return (node){};

    advance_token(st); // consume 'while'

    PARSE_EXPECT(TOK_BRACKET_LEFT, "Expected '(' after 'while'");

    node *while_node = new_node(NODE_WHILE);

    // Condition
    node cond = parse_expression(st, PREC_NONE);
    if (st->had_error)
        return (node){};

    node *cond_node = new_node(cond.type);
    *cond_node = cond;
    add_child(while_node, cond_node);

    PARSE_EXPECT(TOK_BRACKET_RIGHT, "Expected ')' after while condition");

    // Body
    node body = parse_block(st);
    if (st->had_error)
        return (node){};

    node *body_node = new_node(body.type);
    *body_node = body;
    add_child(while_node, body_node);

    return *while_node;
}

static node parse_function_declaration(parse_state *st)
{
    if (st->had_error)
        return (node){};

    node *func = new_node(NODE_FUNCTION_DECLARATION);

    // Optional return type (identifier before name)
    // For now, we just parse the function name
    PARSE_EXPECT_DONT_EAT(TOK_LABEL, "Expected function name");

    add_child(func, create_node_from_token(st, NODE_IDENTIFIER));

    // Parameters
    PARSE_EXPECT(TOK_BRACKET_LEFT, "Expected '(' for function parameters");

    do {
        PARSE_EXPECT_DONT_EAT(TOK_LABEL, "Expected parameter name");

        add_child(func, create_node_from_token(st, NODE_IDENTIFIER));

    }while(match(st, TOK_COMMA));

    PARSE_EXPECT(TOK_BRACKET_RIGHT, "Expected ')' after parameters");

    // Function body
    node body = parse_block(st);
    if (st->had_error)
        return (node){};

    node *body_node = new_node(body.type);
    *body_node = body;
    add_child(func, body_node);

    return *func;
}

static node parse_statement(parse_state *st)
{
    if (st->had_error)
        return (node){};

    // Control flow keywords
    if (is_type(st, TOK_LABEL))
    {
        const char *text = st->current.text;

        if (strcmp(text, "if") == 0)
            return parse_if_statement(st);

        if (strcmp(text, "while") == 0)
            return parse_while_statement(st);

        if (strcmp(text, "void") == 0)
        {
            advance_token(st);
            return parse_function_declaration(st);
        }

        // Check for "main" keyword - special case for parameterless entry point
        if (strcmp(text, "main") == 0)
        {
            node *func = new_node(NODE_FUNCTION_DECLARATION);

            node *name = create_node_from_token(st, NODE_IDENTIFIER);
            if (!name)
                return (node){};
            add_child(func, name);

            // Parse body directly
            node body = parse_block(st);
            if (st->had_error)
                return (node){};

            node *body_node = new_node(body.type);
            *body_node = body;
            add_child(func, body_node);

            return *func;
        }

        // Try parsing as expression (could be assignment, function call, etc.)
        node expr = parse_expression(st, PREC_NONE);
        if (st->had_error)
            return (node){};

        // Optional semicolon for expression statements
        match(st, TOK_SEMICOLON);

        return expr;
    }

    // Bare block
    if (is_type(st, TOK_CURLY_BRACKET_LEFT))
        return parse_block(st);

    COMPILER_ERROR("Unexpected token in statement: '%s'", st->current.text);
    st->had_error = true;
    return (node){};
}

// ============================================================================
// TOP-LEVEL PARSING
// ============================================================================

node parse_program(const char *input)
{
    parse_state st = {.src = input, .read_ptr = input, .line = 1, .had_error = false};
    advance_token(&st);

    node *program_node = new_node(NODE_PROGRAM);

    // Parse top-level declarations and statements
    while (!is_type(&st, TOK_EOF) && !st.had_error)
    {
        node decl = parse_statement(&st);

        if (!st.had_error && decl.type != (node_type)0)
        {
            node *decl_copy = new_node(decl.type);
            *decl_copy = decl;
            add_child(program_node, decl_copy);
        }

        // If we hit an error, stop to prevent cascading failures
        if (st.had_error)
            break;
    }

    return *program_node;
}

void print_ast(node *root, int depth)
{
    if (!root)
        return;

    // Print indentation
    for (int i = 0; i < depth; i++)
        printf("  ");

    // Print node information
    printf("[%s]", node_type_str(root->type));

    // Print additional info based on type
    if (root->type == NODE_NUMBER)
    {
        printf(" value=%d", root->number_value);
    }
    else if (root->string[0] != '\0')
    {
        printf(" \"%s\"", root->string);
    }

    if (root->child_count > 0)
    {
        printf(" (%d children)", root->child_count);
    }

    printf("\n");

    // Recursively print children
    for (u32 i = 0; i < root->child_count; i++)
    {
        node *child_node = get_node_by_index(root->children[i]);
        print_ast(child_node, depth + 1);
    }
}