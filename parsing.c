#include "mpc.h"

#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>

long eval_unary_op(char* operator, long x) {
    if (strcmp(operator, "-") == 0) {
        return x = -x;
    }

    fprintf(stderr, "WARNING: not supported unary operator '%s'\n", operator);

    return x;
}

long eval_op(char* operator, long x, long y) {
    if (strcmp(operator, "+") == 0 || strcmp(operator, "add") == 0) {
        return x + y;
    }
    if (strcmp(operator, "-") == 0 || strcmp(operator, "sub") == 0) {
        return x - y;
    }
    if (strcmp(operator, "*") == 0 || strcmp(operator, "mul") == 0) {
        return x * y;
    }
    if (strcmp(operator, "/") == 0 || strcmp(operator, "div") == 0) {
        return x / y;
    }
    if (strcmp(operator, "%") == 0 || strcmp(operator, "mod") == 0) {
        return x % y;
    }
    if (strcmp(operator, "^") == 0) {
        return pow(x, y);
    }
    if (strcmp(operator, "min") == 0) {
        return x > y ? y : x;
    }
    if (strcmp(operator, "max") == 0) {
        return x > y ? x : y;
    }

    return 0;
}

long eval(mpc_ast_t* t) {
    if (strstr(t->tag, "number")) {
        return atoi(t->contents);
    }

    char* operator = t->children[1]->contents;
    long x = eval(t->children[2]);

    int i = 3;
    if (!strstr(t->children[3]->tag, "expr")) {
        x = eval_unary_op(operator, x);
    } else {
        while (strstr(t->children[i]->tag, "expr")) {
            x = eval_op(operator, x, eval(t->children[i]));
            i++;
        }
    }

    return x;
}

int main(int argc, char** argv) {

    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lliisspp = mpc_new("lispy");

    FILE *f = fopen("grammar", "rb");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *grammar = malloc(fsize + 1);
    fread(grammar, fsize, 1, f);
    fclose(f);

    grammar[fsize] = 0;

    mpca_lang(MPCA_LANG_DEFAULT, grammar, Number, Operator, Expr, Lliisspp);

    puts("lliisspp version 0.0.1");
    puts("Press Ctrl+C to Exit\n");

    while (1) {
        char* input = readline("lliisspp> ");

        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lliisspp, &r)) {
            long result = eval(r.output);
            printf("%li\n", result);

            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    free(grammar);
    mpc_cleanup(4, Number, Operator, Expr, Lliisspp);

    return 0;
}
