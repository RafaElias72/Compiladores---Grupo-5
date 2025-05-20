#include "compiler.h"
#include "helpers/vector.h"
#include <assert.h> // LAB4

// Variáveis globais
static struct compile_process* current_process;
static struct token* parser_last_token;

// Declarações externas e estruturas
extern struct expressionable_op_precedence_group op_precedence[TOTAL_OPERADOR_GROUPS]; // LAB4

struct history {
    int flags;
};

// Declaração de funções
void parse_expressionable(struct history* history);
int parse_expressionable_single(struct history* history);
static bool parser_left_op_has_priority(const char* op_left, const char* op_right); // LAB4

// Funções auxiliares de controle da estrutura history
struct history* history_begin(int flags) {
    struct history* history = calloc(1, sizeof(struct history));
    history->flags = flags;
    return history;
}

struct history* history_down(struct history* history, int flags) {
    struct history* new_history = calloc(1, sizeof(struct history));
    memcpy(new_history, history, sizeof(struct history));
    new_history->flags = flags;
    return new_history;
}

// Funções auxiliares para lidar com tokens
static void parser_ignore_nl_or_comment(struct token* token) {
    while (token && discart_token(token)) {
        vector_peek(current_process->token_vec);
        token = vector_peek_no_increment(current_process->token_vec);
    }
}

static struct token* token_next() {
    struct token* next_token = vector_peek_no_increment(current_process->token_vec);
    parser_ignore_nl_or_comment(next_token);
    current_process->pos = next_token->pos;
    parser_last_token = next_token;
    return vector_peek(current_process->token_vec);
}

static struct token* token_peek_next() {
    struct token* next_token = vector_peek_no_increment(current_process->token_vec);
    parser_ignore_nl_or_comment(next_token);
    return vector_peek_no_increment(current_process->token_vec);
}

// Conversão de token único para node
void parse_single_token_to_node() {
    struct token* token = token_next();
    struct node* node = NULL;

    switch (token->type) {
        case TOKEN_TYPE_NUMBER:
            node = node_create(&(struct node){.type = NODE_TYPE_NUMBER, .llnum = token->llnum});
            break;
        case TOKEN_TYPE_IDENTIFIER:
            node = node_create(&(struct node){.type = NODE_TYPE_IDENTIFIER, .sval = token->sval});
            break;
        case TOKEN_TYPE_STRING:
            node = node_create(&(struct node){.type = NODE_TYPE_STRING, .sval = token->sval});
            break;
        default:
            compiler_error(current_process, "Esse token nao pode ser convertido para node!\n");
            break;
    }
}

// Lógica principal de expressão
void parse_expressionable_for_op(struct history* history, const char* op) {
    parse_expressionable(history);
}

void parse_exp_normal(struct history* history) {
    struct token* op_token = token_peek_next();
    char* op = op_token->sval;
    struct node* node_left = node_peek_expressionable_or_null();
    if (!node_left) return;

    token_next(); // Consome operador
    node_pop();   // Remove node da esquerda

    node_left->flags |= NODE_FLAG_INSIDE_EXPRESSION;

    parse_expressionable_for_op(history_down(history, history->flags), op);
    struct node* node_right = node_pop();
    node_right->flags |= NODE_FLAG_INSIDE_EXPRESSION;

    make_exp_node(node_left, node_right, op);
    struct node* exp_node = node_pop();

    parser_reorder_expression(&exp_node); // LAB4

    node_push(exp_node);
}

int parse_exp(struct history* history) {
    parse_exp_normal(history);
    return 0;
}

// LAB4 - Reorganizador de árvore de expressão com precedência
void parser_reorder_expression(struct node** node_out) {
    struct node* node = *node_out;

    if (node->type != NODE_TYPE_EXPRESSION) return;
    if (node->exp.left != NODE_TYPE_EXPRESSION && (!node->exp.right || node->exp.right->type != NODE_TYPE_EXPRESSION)) return;

    if (node->exp.right && node->exp.right->type == NODE_TYPE_EXPRESSION) {
        const char* right_op = node->exp.right->exp.op;

        if (parser_left_op_has_priority(node->exp.op, right_op)) {
            parser_node_shift_children_left(node);
            parser_reorder_expression(&node->exp.left);
            parser_reorder_expression(&node->exp.right);
        }
    }
}

// Funções de precedência e associatividade
static bool parser_get_precedence_for_operator(const char* op, struct expressionable_op_precedence_group** group_out) {
    *group_out = NULL;
    for (int i = 0; i < TOTAL_OPERADOR_GROUPS; i++) {
        for (int j = 0; op_precedence[i].operators[j]; j++) {
            if (S_EQ(op, op_precedence[i].operators[j])) {
                *group_out = &op_precedence[i];
                return i;
            }
        }
    }
    return -1;
}

static bool parser_left_op_has_priority(const char* op_left, const char* op_right) {
    if (S_EQ(op_left, op_right)) return false;

    struct expressionable_op_precedence_group* group_left = NULL;
    struct expressionable_op_precedence_group* group_right = NULL;

    int precedence_left = parser_get_precedence_for_operator(op_left, &group_left);
    int precedence_right = parser_get_precedence_for_operator(op_right, &group_right);

    if (group_left->associativity == ASSOCIATIVITY_RIGHT_TO_LEFT) return false;

    return precedence_left <= precedence_right;
}

void parser_node_shift_children_left(struct node* node) {
    assert(node->type == NODE_TYPE_EXPRESSION);
    assert(node->exp.right->type == NODE_TYPE_EXPRESSION);

    const char* right_op = node->exp.right->exp.op;

    struct node* new_exp_left_node = node->exp.left;
    struct node* new_exp_right_node = node->exp.right->exp.left;

    make_exp_node(new_exp_left_node, new_exp_right_node, node->exp.op);

    struct node* new_left_operand = node_pop();
    struct node* new_right_operand = node->exp.right->exp.right;

    node->exp.left = new_left_operand;
    node->exp.right = new_right_operand;
    node->exp.op = right_op;
}

// Expressão recursiva
int parse_expressionable_single(struct history* history) {
    struct token* token = token_peek_next();
    if (!token) return -1;

    history->flags |= NODE_FLAG_INSIDE_EXPRESSION;
    int res = -1;

    switch (token->type) {
        case TOKEN_TYPE_NUMBER:
            parse_single_token_to_node();
            res = 0;
            break;
        case TOKEN_TYPE_OPERATOR:
            parse_exp(history);
            res = 0;
            break;
        default:
            break;
    }

    return res;
}

void parse_expressionable(struct history* history) {
    while (parse_expressionable_single(history) == 0) {}
}

// Lógica de parsing geral
int parse_next() {
    struct token* token = token_peek_next();
    if (!token) return -1;

    switch (token->type) {
        case TOKEN_TYPE_NUMBER:
        case TOKEN_TYPE_IDENTIFIER:
        case TOKEN_TYPE_STRING:
            parse_expressionable(history_begin(0));
            break;
        default:
            break;
    }
    return 0;
}

int parse(struct compile_process* process) {
    current_process = process;
    parser_last_token = NULL;
    struct node* node = NULL;
    node_set_vector(process->node_vec, process->node_tree_vec);
    vector_set_peek_pointer(process->token_vec, 0);

    while (parse_next() == 0) {
        node = node_peek();
        vector_push(process->node_tree_vec, &node);
    }

    return PARSE_ALL_OK;
}
