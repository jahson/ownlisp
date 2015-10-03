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
enum { LVAL_INTEGER, LVAL_DECIMAL, LVAL_ERROR, LVAL_SYMBOL, LVAL_SEXPRESSION, LVAL_QEXPRESSION };
// possible error types
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

lval* lval_qepression(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPRESSION;
    v->count = 0;
    v->cell = NULL;
    return v;
}

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

lval* lval_sexpression(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPRESSION;
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

        case LVAL_QEXPRESSION:
        case LVAL_SEXPRESSION:
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
        x = lval_sexpression();
    }
    if (strstr(t->tag, "qexpr")) {
        x = lval_qepression();
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
        case LVAL_SEXPRESSION:
            lval_expr_print(v, '(', ')');
            break;
        case LVAL_QEXPRESSION:
            lval_expr_print(v, '{', '}');
            break;
        case LVAL_ERROR:
            printf("Error: %s", v->val.error);
            break;
        default:
            printf("Error: Unknown value type %d", v->type);
            break;
    }
}

void lval_println(lval* v) {
    lval_print(v);
    putchar('\n');
}

lval* lval_pop(lval* v, int i) {
    lval* x = v->cell[i];

    // shift memory after the item at 'i' over the top and reallocate memory used
    memmove(&v->cell[i], &v->cell[i + 1], sizeof(lval*) * (v->count - i - 1));
    v->count--;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);

    return x;
}

lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_delete(v);
    return x;
}

lval* builtin_op(char* operator, lval* a) {
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != LVAL_INTEGER && a->cell[i]->type != LVAL_DECIMAL) {
            lval_delete(a);
            return lval_error("Cannot operate on non-number");
        }
    }

    lval* x = lval_pop(a, 0);

    // unary negation
    if ((strcmp(operator, "-") == 0) && a->count == 0) {
        switch(x->type) {
            case LVAL_INTEGER:
                x->val.integer = -x->val.integer;
                break;
            case LVAL_DECIMAL:
                x->val.decimal = -x->val.decimal;
                break;
        }
    }

    while (a->count > 0) {
        lval* y = lval_pop(a, 0);

        if (strcmp(operator, "+") == 0 || strcmp(operator, "add") == 0) {
            if (x->type == LVAL_DECIMAL && y->type == LVAL_DECIMAL) {
                x->val.decimal += y->val.decimal;
            } else if (x->type == LVAL_DECIMAL && y->type == LVAL_INTEGER) {
                x->val.decimal += y->val.integer;
            } else if (x->type == LVAL_INTEGER && y->type == LVAL_DECIMAL) {
                x->val.integer += (int)y->val.decimal;
            } else {
                x->val.integer += y->val.integer;
            }
        }

        if (strcmp(operator, "-") == 0 || strcmp(operator, "sub") == 0) {
            if (x->type == LVAL_DECIMAL && y->type == LVAL_DECIMAL) {
                x->val.decimal -= y->val.decimal;
            } else if (x->type == LVAL_DECIMAL && y->type == LVAL_INTEGER) {
                x->val.decimal -= y->val.integer;
            } else if (x->type == LVAL_INTEGER && y->type == LVAL_DECIMAL) {
                x->val.integer -= (int)y->val.decimal;
            } else {
                x->val.integer -= y->val.integer;
            }
        }

        if (strcmp(operator, "*") == 0 || strcmp(operator, "mul") == 0) {
            if (x->type == LVAL_DECIMAL && y->type == LVAL_DECIMAL) {
                x->val.decimal *= y->val.decimal;
            } else if (x->type == LVAL_DECIMAL && y->type == LVAL_INTEGER) {
                x->val.decimal *= y->val.integer;
            } else if (x->type == LVAL_INTEGER && y->type == LVAL_DECIMAL) {
                x->val.integer *= (int)y->val.decimal;
            } else {
                x->val.integer *= y->val.integer;
            }
        }

        if (strcmp(operator, "/") == 0 || strcmp(operator, "div") == 0) {
            switch (y->type) {
                case LVAL_DECIMAL:
                    if (y->val.decimal == 0.0) {
                        lval_delete(x);
                        lval_delete(y);
                        lval_delete(a);
                        return lval_error("Division by zero");
                    }
                    break;
                case LVAL_INTEGER:
                    if (y->val.integer == 0) {
                        lval_delete(x);
                        lval_delete(y);
                        lval_delete(a);
                        return lval_error("Division by zero");
                    }
                    break;
            }

            if (x->type == LVAL_DECIMAL && y->type == LVAL_DECIMAL) {
                x->val.decimal /= y->val.decimal;
            } else if (x->type == LVAL_DECIMAL && y->type == LVAL_INTEGER) {
                x->val.decimal /= y->val.integer;
            } else if (x->type == LVAL_INTEGER && y->type == LVAL_DECIMAL) {
                x->val.integer /= y->val.decimal;
            } else {
                x->val.integer /= y->val.integer;
            }
        }

        if (strcmp(operator, "%") == 0 || strcmp(operator, "mod") == 0) {
            switch (y->type) {
                case LVAL_DECIMAL:
                    if (y->val.decimal == 0.0) {
                        lval_delete(x);
                        lval_delete(y);
                        lval_delete(a);
                        return lval_error("Division by zero");
                    }
                    break;
                case LVAL_INTEGER:
                    if (y->val.integer == 0) {
                        lval_delete(x);
                        lval_delete(y);
                        lval_delete(a);
                        return lval_error("Division by zero");
                    }
                    break;
            }

            if (x->type == LVAL_DECIMAL && y->type == LVAL_DECIMAL) {
                x->val.decimal = fmod(x->val.decimal, y->val.decimal);
            } else if (x->type == LVAL_DECIMAL && y->type == LVAL_INTEGER) {
                x->val.decimal = fmod(x->val.decimal, y->val.integer);
            } else if (x->type == LVAL_INTEGER && y->type == LVAL_DECIMAL) {
                x->val.integer = fmod(x->val.integer, (int)y->val.decimal);
            } else {
                x->val.integer = (int)fmod(x->val.integer, y->val.integer);
            }
        }

        if (strcmp(operator, "^") == 0) {
            if (x->type == LVAL_DECIMAL && y->type == LVAL_DECIMAL) {
                x->val.decimal = pow(x->val.decimal, y->val.decimal);
            } else if (x->type == LVAL_DECIMAL && y->type == LVAL_INTEGER) {
                x->val.decimal = pow(x->val.decimal, y->val.integer);
            } else if (x->type == LVAL_INTEGER && y->type == LVAL_DECIMAL) {
                x->val.integer = (int)pow(x->val.integer, y->val.decimal);
            } else {
                x->val.integer = pow(x->val.integer, y->val.integer);
            }
        }

        if (strcmp(operator, "min") == 0) {
            if (x->type == LVAL_DECIMAL && y->type == LVAL_DECIMAL) {
                x->val.decimal = x->val.decimal < y->val.decimal
                    ? x->val.decimal
                    : y->val.decimal;
            } else if (x->type == LVAL_DECIMAL && y->type == LVAL_INTEGER) {
                x->val.decimal = x->val.decimal < y->val.integer
                    ? x->val.decimal
                    : (double)y->val.integer;
            } else if (x->type == LVAL_INTEGER && y->type == LVAL_DECIMAL) {
                x->val.integer = x->val.integer < y->val.decimal
                    ? x->val.integer
                    : (int)y->val.decimal;
            } else {
                x->val.integer = x->val.integer < y->val.integer
                    ? x->val.integer
                    : y->val.integer;
            }
        }
        if (strcmp(operator, "max") == 0) {
            if (x->type == LVAL_DECIMAL && y->type == LVAL_DECIMAL) {
                x->val.decimal = x->val.decimal > y->val.decimal
                    ? x->val.decimal
                    : y->val.decimal;
            } else if (x->type == LVAL_DECIMAL && y->type == LVAL_INTEGER) {
                x->val.decimal = x->val.decimal > y->val.integer
                    ? x->val.decimal
                    : (double)y->val.integer;
            } else if (x->type == LVAL_INTEGER && y->type == LVAL_DECIMAL) {
                x->val.integer = x->val.integer > y->val.decimal
                    ? x->val.integer
                    : (int)y->val.decimal;
            } else {
                x->val.integer = x->val.integer > y->val.integer
                    ? x->val.integer
                    : y->val.integer;
            }
        }

        lval_delete(y);
    }

    lval_delete(a);
    return x;
}

lval* lval_eval(lval* v); // forward declare to use in 'lval_eval_sexpr'
lval* lval_eval_sexpr(lval* v) {
    // evaluate children
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    // check errors
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERROR) {
            return lval_take(v, i);
        }
    }

    // empty expression
    if (v->count == 0) {
        return v;
    }

    // single expression
    if (v->count == 1) {
        return lval_take(v, 0);
    }

    // first element should be Symbol
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_SYMBOL) {
        lval_delete(f);
        lval_delete(v);
        return lval_error("S-expression does not start with symbol");
    }

    lval* result = builtin_op(f->val.symbol, v);
    lval_delete(f);

    return result;
}

lval* lval_eval(lval* v) {
    // only evaluate s-expressions
    if (v->type == LVAL_SEXPRESSION) {
        return lval_eval_sexpr(v);
    }
    return v;
}

int main(int argc, char** argv) {
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Decimal = mpc_new("decimal");
    mpc_parser_t* Integer = mpc_new("integer");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Qexpr = mpc_new("qexpr");
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

    mpca_lang(MPCA_LANG_DEFAULT,
              grammar,
              Number,
              Decimal,
              Integer,
              Symbol,
              Sexpr,
              Qexpr,
              Expr,
              Lliisspp);

    puts("lliisspp version 0.0.1");
    puts("Press Ctrl+C to Exit\n");

    while (1) {
        char* input = readline("lliisspp> ");

        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lliisspp, &r)) {
            lval* x = lval_eval(lval_read(r.output));
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
    mpc_cleanup(8,
                Number,
                Decimal,
                Integer,
                Symbol,
                Sexpr,
                Qexpr,
                Expr,
                Lliisspp);

    return 0;
}
