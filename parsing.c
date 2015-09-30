#include "mpc.h"

#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>

typedef struct {
    int type;
    long num;
    int err;
} lval;

// possible lval types
enum { LVAL_NUM, LVAL_ERR };
// possible error types
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

lval lval_num(long x) {
    lval v;
    v.type = LVAL_NUM;
    v.num = x;
    return v;
}

lval lval_err(int x) {
    lval v;
    v.type = LVAL_ERR;
    v.err = x;
    return v;
}

void print_error(int error_code) {
    switch (error_code) {
        case LERR_DIV_ZERO:
            printf("Error: Division by zero!");
            break;
        case LERR_BAD_OP:
            printf("Error: Invalid operator!");
            break;
        case LERR_BAD_NUM:
            printf("Error: Invalid number!");
            break;
        default:
            printf("Error: Unknown error code %d", error_code);
    }
}

void lval_print(lval v) {
    switch (v.type) {
        case LVAL_NUM:
            printf("%li", v.num);
            break;
        case LVAL_ERR:
            print_error(v.err);
            break;
    }
}

void lval_println(lval v) {
    lval_print(v);
    putchar('\n');
}

lval eval_unary_op(char* operator, lval x) {
    if (strcmp(operator, "-") == 0) {
        return lval_num(-x.num);
    }
    return lval_err(LERR_BAD_OP);
}

lval eval_op(char* operator, lval x, lval y) {
    if (x.type == LVAL_ERR) {
        return x;
    }
    if (y.type == LVAL_ERR) {
        return y;
    }

    if (strcmp(operator, "+") == 0 || strcmp(operator, "add") == 0) {
        return lval_num(x.num + y.num);
    }
    if (strcmp(operator, "-") == 0 || strcmp(operator, "sub") == 0) {
        return lval_num(x.num - y.num);
    }
    if (strcmp(operator, "*") == 0 || strcmp(operator, "mul") == 0) {
        return lval_num(x.num * y.num);
    }
    if (strcmp(operator, "/") == 0 || strcmp(operator, "div") == 0) {
        return y.num == 0
            ? lval_err(LERR_DIV_ZERO)
            : lval_num(x.num / y.num);
    }
    if (strcmp(operator, "%") == 0 || strcmp(operator, "mod") == 0) {
        return lval_num(x.num % y.num);
    }
    if (strcmp(operator, "^") == 0) {
        return lval_num(pow(x.num, y.num));
    }
    if (strcmp(operator, "min") == 0) {
        return x.num > y.num ? y : x;
    }
    if (strcmp(operator, "max") == 0) {
        return x.num > y.num ? x : y;
    }

    return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t* t) {
    if (strstr(t->tag, "number")) {
        errno = 0;
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE
            ? lval_num(x)
            : lval_err(LERR_BAD_NUM);
    }

    char* operator = t->children[1]->contents;
    lval x = eval(t->children[2]);

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
            lval result = eval(r.output);
            lval_println(result);

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
