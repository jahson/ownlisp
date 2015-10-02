#include "mpc.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <editline/readline.h>

typedef struct lval {
    int type;
    union {
        long integer;
        double decimal;
        char* error;
        char* symbol;
    } val;
    int count;
    struct lval** cell;
} lval;

// possible lval types
enum { LVAL_INTEGER, LVAL_DECIMAL, LVAL_ERROR, LVAL_SYMBOL, LVAL_SEXPR };
// possible error types
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

lval* lval_integer(long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_INTEGER;
    v->val.integer = x;
    return v;
}

lval* lval_decimal(double x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_DECIMAL;
    v->val.decimal = x;
    return v;
}

lval* lval_error(char* m) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERROR;
    v->val.error = malloc(strlen(m) + 1);
    strcpy(v->val.error, m);
    return v;
}

lval* lval_symbol(char* m) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYMBOL;
    v->val.symbol = malloc(strlen(m) + 1);
    strcpy(v->val.error, m);
    return v;
}

lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void lval_delete(lval* v) {
    switch (v->type) {
        case LVAL_INTEGER:
        case LVAL_DECIMAL:
            break;

        case LVAL_ERROR:
            free(v->val.error);
            break;

        case LVAL_SYMBOL:
            free(v->val.symbol);
            break;

        case LVAL_SEXPR:
            // free memory for all elements inside
            for (int i = 0; i < v->count; i++) {
                lval_delete(v->cell[i]);
            }
            // free memory allocated to contain the pointers
            free(v->cell);
            break;
    }

    // free memory for 'lval' structure itself
    free(v);
}

lval* lval_read_number(mpc_ast_t* t) {
    errno = 0;
    if (strstr(t->tag, "integer")) {
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE
            ? lval_integer(x)
            : lval_error("Invalid integer");
    }
    if (strstr(t->tag, "decimal")) {
        double x = strtod(t->contents, NULL);
        return errno != ERANGE
            ? lval_decimal(x)
            : lval_error("Invalid decimal");
    }
    return lval_error("Unknown number type");
}

lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

lval* lval_read(mpc_ast_t* t) {
    if (strstr(t->tag, "number")) {
        return lval_read_number(t);
    }
    if (strstr(t->tag, "symbol")) {
        return lval_symbol(t->contents);
    }

    // if root or sexpr then create empty list
    lval* x = NULL;
    if (
        strcmp(t->tag, ">") == 0 ||
        strstr(t->tag, "sexpr")
        ) {
        x = lval_sexpr();
    }

    // fill the list with any valid expression contained within
    for (int i = 0; i < t->children_num; i++) {
        if (
            strcmp(t->children[i]->contents, "(") == 0 ||
            strcmp(t->children[i]->contents, ")") == 0 ||
            strcmp(t->children[i]->contents, "{") == 0 ||
            strcmp(t->children[i]->contents, "}") == 0 ||
            strcmp(t->children[i]->tag, "regex") == 0
           ) {
            continue;
        }
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

void lval_print(lval* v); // forward declare to use in 'lval_expr_print'
void lval_expr_print(lval* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        // print value contained within
        lval_print(v->cell[i]);

        // don't print trailing space if last element
        if (i != (v->count - 1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_print(lval* v) {
    switch (v->type) {
        case LVAL_INTEGER:
            printf("%li", v->val.integer);
            break;
        case LVAL_DECIMAL:
            printf("%f", v->val.decimal);
            break;
        case LVAL_SYMBOL:
            printf("%s", v->val.symbol);
            break;
        case LVAL_SEXPR:
            lval_expr_print(v, '(', ')');
            break;
    }
}

void lval_println(lval* v) {
    lval_print(v);
    putchar('\n');
}

lval* eval_unary_op(char* operator, lval* x) {
    if (strcmp(operator, "-") == 0) {
        switch(x->type) {
            case LVAL_INTEGER:
                return lval_integer(-x->val.integer);
                break;
            case LVAL_DECIMAL:
                return lval_decimal(-x->val.decimal);
                break;
        }
    }
    return lval_error("Wrong unary operator");
}

lval* eval_op(char* operator, lval* x, lval* y) {
    if (x->type == LVAL_ERROR) {
        return x;
    }
    if (y->type == LVAL_ERROR) {
        return y;
    }

    if (strcmp(operator, "+") == 0 || strcmp(operator, "add") == 0) {
        if (x->type == LVAL_DECIMAL && y->type == LVAL_DECIMAL) {
            return lval_decimal(x->val.decimal + y->val.decimal);
        } else if (x->type == LVAL_DECIMAL && y->type == LVAL_INTEGER) {
            return lval_decimal(x->val.decimal + (double)y->val.integer);
        } else if (x->type == LVAL_INTEGER && y->type == LVAL_DECIMAL) {
            return lval_integer(x->val.integer + (int)y->val.decimal);
        } else {
            return lval_integer(x->val.integer + y->val.integer);
        }
    }
    if (strcmp(operator, "-") == 0 || strcmp(operator, "sub") == 0) {
        if (x->type == LVAL_DECIMAL && y->type == LVAL_DECIMAL) {
            return lval_decimal(x->val.decimal - y->val.decimal);
        } else if (x->type == LVAL_DECIMAL && y->type == LVAL_INTEGER) {
            return lval_decimal(x->val.decimal - y->val.integer);
        } else if (x->type == LVAL_INTEGER && y->type == LVAL_DECIMAL) {
            return lval_integer(x->val.integer - (int)y->val.decimal);
        } else {
            return lval_integer(x->val.integer - y->val.integer);
        }
    }
    if (strcmp(operator, "*") == 0 || strcmp(operator, "mul") == 0) {
        if (x->type == LVAL_DECIMAL && y->type == LVAL_DECIMAL) {
            return lval_decimal(x->val.decimal * y->val.decimal);
        } else if (x->type == LVAL_DECIMAL && y->type == LVAL_INTEGER) {
            return lval_decimal(x->val.decimal * y->val.integer);
        } else if (x->type == LVAL_INTEGER && y->type == LVAL_DECIMAL) {
            return lval_integer(x->val.integer * (int)y->val.decimal);
        } else {
            return lval_integer(x->val.integer * y->val.integer);
        }
    }
    if (strcmp(operator, "/") == 0 || strcmp(operator, "div") == 0) {
        if (x->type == LVAL_DECIMAL && y->type == LVAL_DECIMAL) {
            return y->val.decimal == 0.0
                ? lval_error("Division by zero")
                : lval_decimal(x->val.decimal / y->val.decimal);
        } else if (x->type == LVAL_DECIMAL && y->type == LVAL_INTEGER) {
            return y->val.integer == 0
                ? lval_error("Division by zero")
                : lval_decimal(x->val.decimal / y->val.integer);
        } else if (x->type == LVAL_INTEGER && y->type == LVAL_DECIMAL) {
            return y->val.decimal == 0.0
                ? lval_error("Division by zero")
                : lval_decimal(x->val.integer / (int)y->val.decimal);
        } else {
            return y->val.integer == 0
                ? lval_error("Division by zero")
                : lval_decimal(x->val.integer / y->val.integer);
        }
    }
    if (strcmp(operator, "%") == 0 || strcmp(operator, "mod") == 0) {
        if (x->type == LVAL_DECIMAL && y->type == LVAL_DECIMAL) {
            return lval_decimal(fmod(x->val.decimal, y->val.decimal));
        } else if (x->type == LVAL_DECIMAL && y->type == LVAL_INTEGER) {
            return lval_decimal(fmod(x->val.decimal, y->val.integer));
        } else if (x->type == LVAL_INTEGER && y->type == LVAL_DECIMAL) {
            return lval_integer(fmod(x->val.integer, (int)y->val.decimal));
        } else {
            return lval_integer((int)fmod(x->val.integer, y->val.integer));
        }
    }
    if (strcmp(operator, "^") == 0) {
        if (x->type == LVAL_DECIMAL && y->type == LVAL_DECIMAL) {
            return lval_decimal(pow(x->val.decimal, y->val.decimal));
        } else if (x->type == LVAL_DECIMAL && y->type == LVAL_INTEGER) {
            return lval_decimal(pow(x->val.decimal, y->val.integer));
        } else if (x->type == LVAL_INTEGER && y->type == LVAL_DECIMAL) {
            return lval_integer(pow(x->val.integer, y->val.decimal));
        } else if (x->type == LVAL_INTEGER && y->type == LVAL_DECIMAL) {
            return lval_integer(pow(x->val.integer, (int)y->val.decimal));
        } else {
            return lval_integer(pow(x->val.integer, y->val.integer));
        }
    }
    if (strcmp(operator, "min") == 0) {
        if (x->type == LVAL_DECIMAL && y->type == LVAL_DECIMAL) {
            return x->val.decimal < y->val.decimal ? x : y;
        } else if (x->type == LVAL_DECIMAL && y->type == LVAL_INTEGER) {
            return x->val.decimal < y->val.integer ? x : y;
        } else if (x->type == LVAL_INTEGER && y->type == LVAL_DECIMAL) {
            return x->val.integer < y->val.decimal ? x : y;
        } else {
            return x->val.integer < y->val.integer ? x : y;
        }
    }
    if (strcmp(operator, "max") == 0) {
        if (x->type == LVAL_DECIMAL && y->type == LVAL_DECIMAL) {
            return x->val.decimal > y->val.decimal ? x : y;
        } else if (x->type == LVAL_DECIMAL && y->type == LVAL_INTEGER) {
            return x->val.decimal > y->val.integer ? x : y;
        } else if (x->type == LVAL_INTEGER && y->type == LVAL_DECIMAL) {
            return x->val.integer > y->val.decimal ? x : y;
        } else {
            return x->val.integer > y->val.integer ? x : y;
        }
        return x->val.integer > y->val.integer ? x : y;
    }

    return lval_error("Unknown operator");
}

int main(int argc, char** argv) {
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Decimal = mpc_new("decimal");
    mpc_parser_t* Integer = mpc_new("integer");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
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

    mpca_lang(MPCA_LANG_DEFAULT, grammar, Number, Decimal, Integer, Symbol, Sexpr, Expr, Lliisspp);

    puts("lliisspp version 0.0.1");
    puts("Press Ctrl+C to Exit\n");

    while (1) {
        char* input = readline("lliisspp> ");

        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lliisspp, &r)) {
            lval* x = lval_read(r.output);
            lval_println(x);

            lval_delete(x);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    free(grammar);
    mpc_cleanup(7, Number, Decimal, Integer, Symbol, Sexpr, Expr, Lliisspp);

    return 0;
}
