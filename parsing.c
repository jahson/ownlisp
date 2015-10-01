#include "mpc.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <editline/readline.h>

typedef struct {
    int type;
    union {
        long integer;
        double decimal;
        int error;
    } val;
} lval;

// possible lval types
enum { LVAL_INTEGER, LVAL_DECIMAL, LVAL_ERROR };
// possible error types
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

lval lval_integer(long x) {
    lval v;
    v.type = LVAL_INTEGER;
    v.val.integer = x;
    return v;
}

lval lval_decimal(double x) {
    lval v;
    v.type = LVAL_DECIMAL;
    v.val.decimal = x;
    return v;
}

lval lval_err(int x) {
    lval v;
    v.type = LVAL_ERROR;
    v.val.error = x;
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
        case LVAL_INTEGER:
            printf("%li", v.val.integer);
            break;
        case LVAL_DECIMAL:
            printf("%f", v.val.decimal);
            break;
        case LVAL_ERROR:
            print_error(v.val.error);
            break;
    }
}

void lval_println(lval v) {
    lval_print(v);
    putchar('\n');
}

lval eval_unary_op(char* operator, lval x) {
    if (strcmp(operator, "-") == 0) {
        switch(x.type) {
            case LVAL_INTEGER:
                return lval_integer(-x.val.integer);
                break;
            case LVAL_DECIMAL:
                return lval_decimal(-x.val.decimal);
                break;
        }
    }
    return lval_err(LERR_BAD_OP);
}

lval eval_op(char* operator, lval x, lval y) {
    if (x.type == LVAL_ERROR) {
        return x;
    }
    if (y.type == LVAL_ERROR) {
        return y;
    }

    if (strcmp(operator, "+") == 0 || strcmp(operator, "add") == 0) {
        if (x.type == LVAL_DECIMAL && y.type == LVAL_DECIMAL) {
            return lval_decimal(x.val.decimal + y.val.decimal);
        } else if (x.type == LVAL_DECIMAL && y.type == LVAL_INTEGER) {
            return lval_decimal(x.val.decimal + (double)y.val.integer);
        } else if (x.type == LVAL_INTEGER && y.type == LVAL_DECIMAL) {
            return lval_integer(x.val.integer + (int)y.val.decimal);
        } else {
            return lval_integer(x.val.integer + y.val.integer);
        }
    }
    if (strcmp(operator, "-") == 0 || strcmp(operator, "sub") == 0) {
        if (x.type == LVAL_DECIMAL && y.type == LVAL_DECIMAL) {
            return lval_decimal(x.val.decimal - y.val.decimal);
        } else if (x.type == LVAL_DECIMAL && y.type == LVAL_INTEGER) {
            return lval_decimal(x.val.decimal - y.val.integer);
        } else if (x.type == LVAL_INTEGER && y.type == LVAL_DECIMAL) {
            return lval_integer(x.val.integer - (int)y.val.decimal);
        } else {
            return lval_integer(x.val.integer - y.val.integer);
        }
    }
    if (strcmp(operator, "*") == 0 || strcmp(operator, "mul") == 0) {
        if (x.type == LVAL_DECIMAL && y.type == LVAL_DECIMAL) {
            return lval_decimal(x.val.decimal * y.val.decimal);
        } else if (x.type == LVAL_DECIMAL && y.type == LVAL_INTEGER) {
            return lval_decimal(x.val.decimal * y.val.integer);
        } else if (x.type == LVAL_INTEGER && y.type == LVAL_DECIMAL) {
            return lval_integer(x.val.integer * (int)y.val.decimal);
        } else {
            return lval_integer(x.val.integer * y.val.integer);
        }
    }
    if (strcmp(operator, "/") == 0 || strcmp(operator, "div") == 0) {
        if (x.type == LVAL_DECIMAL && y.type == LVAL_DECIMAL) {
            return y.val.decimal == 0.0
                ? lval_err(LERR_DIV_ZERO)
                : lval_decimal(x.val.decimal / y.val.decimal);
        } else if (x.type == LVAL_DECIMAL && y.type == LVAL_INTEGER) {
            return y.val.integer == 0
                ? lval_err(LERR_DIV_ZERO)
                : lval_decimal(x.val.decimal / y.val.integer);
        } else if (x.type == LVAL_INTEGER && y.type == LVAL_DECIMAL) {
            return y.val.decimal == 0.0
                ? lval_err(LERR_DIV_ZERO)
                : lval_decimal(x.val.integer / (int)y.val.decimal);
        } else {
            return y.val.integer == 0
                ? lval_err(LERR_DIV_ZERO)
                : lval_decimal(x.val.integer / y.val.integer);
        }
    }
    if (strcmp(operator, "%") == 0 || strcmp(operator, "mod") == 0) {
        if (x.type == LVAL_DECIMAL && y.type == LVAL_DECIMAL) {
            return lval_decimal(fmod(x.val.decimal, y.val.decimal));
        } else if (x.type == LVAL_DECIMAL && y.type == LVAL_INTEGER) {
            return lval_decimal(fmod(x.val.decimal, y.val.integer));
        } else if (x.type == LVAL_INTEGER && y.type == LVAL_DECIMAL) {
            return lval_integer(fmod(x.val.integer, (int)y.val.decimal));
        } else {
            return lval_integer((int)fmod(x.val.integer, y.val.integer));
        }
    }
    if (strcmp(operator, "^") == 0) {
        if (x.type == LVAL_DECIMAL && y.type == LVAL_DECIMAL) {
            return lval_decimal(pow(x.val.decimal, y.val.decimal));
        } else if (x.type == LVAL_DECIMAL && y.type == LVAL_INTEGER) {
            return lval_decimal(pow(x.val.decimal, y.val.integer));
        } else if (x.type == LVAL_INTEGER && y.type == LVAL_DECIMAL) {
            return lval_integer(pow(x.val.integer, y.val.decimal));
        } else if (x.type == LVAL_INTEGER && y.type == LVAL_DECIMAL) {
            return lval_integer(pow(x.val.integer, (int)y.val.decimal));
        } else {
            return lval_integer(pow(x.val.integer, y.val.integer));
        }
    }
    if (strcmp(operator, "min") == 0) {
        if (x.type == LVAL_DECIMAL && y.type == LVAL_DECIMAL) {
            return x.val.decimal < y.val.decimal ? x : y;
        } else if (x.type == LVAL_DECIMAL && y.type == LVAL_INTEGER) {
            return x.val.decimal < y.val.integer ? x : y;
        } else if (x.type == LVAL_INTEGER && y.type == LVAL_DECIMAL) {
            return x.val.integer < y.val.decimal ? x : y;
        } else {
            return x.val.integer < y.val.integer ? x : y;
        }
    }
    if (strcmp(operator, "max") == 0) {
        if (x.type == LVAL_DECIMAL && y.type == LVAL_DECIMAL) {
            return x.val.decimal > y.val.decimal ? x : y;
        } else if (x.type == LVAL_DECIMAL && y.type == LVAL_INTEGER) {
            return x.val.decimal > y.val.integer ? x : y;
        } else if (x.type == LVAL_INTEGER && y.type == LVAL_DECIMAL) {
            return x.val.integer > y.val.decimal ? x : y;
        } else {
            return x.val.integer > y.val.integer ? x : y;
        }
        return x.val.integer > y.val.integer ? x : y;
    }

    return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t* t) {
    if (strstr(t->tag, "integer")) {
        errno = 0;
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE
            ? lval_integer(x)
            : lval_err(LERR_BAD_NUM);
    }
    if (strstr(t->tag, "decimal")) {
        errno = 0;
        double x = strtod(t->contents, NULL);
        return errno != ERANGE
            ? lval_decimal(x)
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
    mpc_parser_t* Decimal = mpc_new("decimal");
    mpc_parser_t* Integer = mpc_new("integer");
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

    mpca_lang(MPCA_LANG_DEFAULT, grammar, Number, Decimal, Integer,  Operator, Expr, Lliisspp);

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
    mpc_cleanup(4, Number, Decimal, Integer, Operator, Expr, Lliisspp);

    return 0;
}
