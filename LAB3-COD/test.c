#include "compiler.h"
#include <stdio.h>
#include <string.h>

// Função auxiliar para imprimir a árvore de nós
void print_node(struct node* node, int level) {
    if (!node) return;
    for (int i = 0; i < level; i++) printf("  ");

    switch (node->type) {
        case NODE_TYPE_NUMBER:
            printf("Number: %lld\n", node->llnum);
            break;
        case NODE_TYPE_IDENTIFIER:
            printf("Identifier: %s\n", node->sval);
            break;
        case NODE_TYPE_STRING:
            printf("String: \"%s\"\n", node->sval);
            break;
        case NODE_TYPE_EXPRESSION:
            printf("Expr: %s\n", node->exp.op);
            print_node(node->exp.left, level + 1);
            print_node(node->exp.right, level + 1);
            break;
        default:
            printf("Unknown node type\n");
    }
}

// Função que roda um teste com código-fonte e operador esperado
void run_test(const char* source_code, const char* expected_op) {
    printf("Testando: \"%s\"\n", source_code);

    struct compile_process* process = compile_process_create(source_code, "<test>");
    int res = compile_process_compile(process);
    if (res != PARSE_ALL_OK) {
        printf("Erro ao compilar código: %s\n", source_code);
        return;
    }

    struct node* root = vector_back(process->node_tree_vec);
    if (!root || root->type != NODE_TYPE_EXPRESSION) {
        printf("Erro: a raiz da árvore não é uma expressão.\n\n");
        return;
    }

    if (!S_EQ(root->exp.op, expected_op)) {
        printf("❌ Falhou: esperado '%s', obtido '%s'\n\n", expected_op, root->exp.op);
    } else {
        printf("✅ Sucesso: operador principal é '%s'\n\n", root->exp.op);
    }

    // Para debug visual
    print_node(root, 0);
    printf("\n");
}

// Função principal que executa os testes de precedência
int main() {
    // Testes básicos de precedência de operadores
    run_test("1 + 2 * 3", "*");     // * tem precedência sobre +
    run_test("4 * 5 + 6", "+");     // + deve ficar no topo, pois 4*5 é agrupado à esquerda
    run_test("1 + 2 + 3 * 4", "*"); // * deve ser mais interno do que os +
    
    return 0;
}
