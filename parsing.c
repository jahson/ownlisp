#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#include "mpc.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <editline/readline.h>


// possible lval types
enum {
    LVAL_INTEGER, // 0
    LVAL_DECIMAL, // 1
    LVAL_ERROR, // 2
    LVAL_SYMBOL, // 3
    LVAL_SEXPRESSION, // 4
    LVAL_QEXPRESSION, // 5
    LVAL_FUNCTION // 5
};


#define STR_EQ(A, B) strcmp((A), (B)) == 0
#define STR_CONTAIN(A, B) strstr((A), (B))

// accessors for lval
#define L_COUNT(lval)    (lval)->count
#define L_CELL(lval)     (lval)->cell
#define L_ENV(lval)     (lval)->env
#define L_TYPE(lval)     (lval)->type
#define L_BUILTIN(lval) (lval)->val.builtin
#define L_FORMALS(lval) (lval)->formals
#define L_BODY(lval)    (lval)->body
#define L_INTEGER(lval)  (lval)->val.integer
#define L_DECIMAL(lval)  (lval)->val.decimal
#define L_ERROR(lval)    (lval)->val.error
#define L_SYMBOL(lval)   (lval)->val.symbol
#define L_CELL_N(lval, n) (lval)->cell[(n)]
#define L_COUNT_N(lval, n) L_CELL_N(lval, n)->count
#define L_TYPE_N(lval, n) L_CELL_N(lval, n)->type
#define L_FORMALS_N(lval, n) L_CELL_N(L_FORMALS(lval), n)
#define L_FORMALS_COUNT(lval) L_COUNT(L_FORMALS(lval))

// accessors for lenv
#define E_PARENT(lenv) (lenv)->parent
#define E_COUNT(lenv) (lenv)->count
#define E_NAMES(lenv) (lenv)->names
#define E_VALUES(lenv) (lenv)->values
#define E_NAMES_N(lenv, i) (lenv)->names[(i)]
#define E_VALUES_N(lenv, i) (lenv)->values[(i)]

// loop
#define FOREACH_SEXP(i, e) for (int i = 0, lim = L_COUNT(e); i < lim; ++i)
#define E_FOREACH(i, e) for (int i = 0, lim = E_COUNT(e); i < lim; ++i)

// utils
#define BUILTIN(n) lval *builtin_##n(lenv *env, lval *a)

#define LASSERT(args, cond, format, ...) \
    if (!(cond)) { \
        lval *err = lval_error(format, ##__VA_ARGS__); \
        lval_delete(args); \
        return err; \
    } \

#define LASSERT_NOT_EMPTY_QEXPR(arguments, function_name) \
    if (L_COUNT_N(arguments, 0) == 0) { \
        lval *err = lval_error("Empty Q-Expression for '%s'.", function_name); \
        lval_delete(arguments); \
        return err; \
    } \

#define LASSERT_ARGUMENT_NUMBER(arguments, expected_number, function_name) \
    if (L_COUNT(arguments) != expected_number) { \
        lval *err= lval_error("Wrong number of arguments for '%s'. Got %i, expected %i.", \
                              function_name, \
                              L_COUNT(arguments), \
                              expected_number); \
        lval_delete(arguments); \
        return err; \
    } \

#define LASSERT_ARGUMENT_TYPE(arguments, argument_number, expected_type, function_name) \
    if ((L_TYPE_N(arguments, argument_number) != expected_type)) { \
        lval *err = lval_error("Incorrect type of argument #%d for '%s'. Got %s, expected %s.", \
                               argument_number + 1, \
                               function_name, \
                               ltype_name(L_TYPE_N(arguments, argument_number)), \
                               ltype_name(expected_type)); \
        lval_delete(arguments); \
        return err; \
    } \

// forward declarations
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

typedef lval *(*lbuiltin)(lenv*, lval*);
struct lval {
    int type;
    union {
        long integer;
        double decimal;
        char *error;
        char *symbol;
        lbuiltin builtin;
    } val;
    lenv *env;
    lval *formals;
    lval *body;
    int count;
    struct lval** cell;
};
struct lenv {
    lenv *parent;

    int count;
    char **names;
    lval **values;
};

// forward declarations
char *ltype_name(int t);
lenv *lenv_new(void);
lenv *lenv_copy(lenv *env);
lval *lval_call(lenv *env, lval *func, lval *a);
void lval_delete(lval *v);
void lenv_delete(lenv *env);
lval *lval_copy(lval *a);
lval *lval_error(char *format, ...);
lval *lenv_get(lenv *env, lval *key);
void lenv_def(lenv *env, lval *key, lval *value);
void lenv_put(lenv *env, lval *key, lval *value);
lval *lval_lambda(lval *formals, lval *body);
lval *lval_function(lbuiltin fn);
lval *lval_qexpression(void);
lval *lval_integer(long x);
lval *lval_decimal(double x);
lval *lval_symbol(char *m);
lval *lval_sexpression(void);
lval *lval_read_number(mpc_ast_t *t);
lval *lval_add(lval *v, lval *x);
lval *lval_read(mpc_ast_t *t);
void lval_expr_print(lenv *env, lval *v, char open, char close);
void lval_print(lenv *env, lval *v);
void lval_println(lenv *env, lval *v);
lval *lval_pop(lval *v, int i);
lval *lval_take(lval *v, int i);
lval *lval_join(lval *x, lval *y);
lval *builtin_gt(lenv *env, lval *a);
lval *builtin_lt(lenv *env, lval *a);
lval *builtin_ge(lenv *env, lval *a);
lval *builtin_le(lenv *env, lval *a);
lval *builtin_ord(lenv *env, lval *a, char *operator);
lval *builtin_lambda(lenv *env, lval *a);
lval *builtin_op(char *operator, lenv *env, lval *a);
lval *builtin_add(lenv *env, lval *a);
lval *builtin_sub(lenv *env, lval *a);
lval *builtin_mul(lenv *env, lval *a);
lval *builtin_div(lenv *env, lval *a);
lval *builtin_mod(lenv *env, lval *a);
lval *builtin_pow(lenv *env, lval *a);
lval *builtin_min(lenv *env, lval *a);
lval *builtin_max(lenv *env, lval *a);
lval *builtin_cons(lenv *env, lval *a);
lval *builtin_init(lenv *env, lval *a);
lval *builtin_head(lenv *env, lval *a);
lval *builtin_tail(lenv *env, lval *a);
lval *builtin_len(lenv *env, lval *a);
lval *builtin_list(lenv *env, lval *a);
lval *builtin_var(lenv *env, lval *a, char *func);
lval *builtin_def(lenv *env, lval *a);
lval *builtin_put(lenv *env, lval *a);
lval *builtin_exit(lenv *env, lval *a);
lval *lval_eval_sexpr(lenv *env, lval *v);
lval *lval_eval(lenv *env, lval *v);
lval *builtin_eval(lenv *env, lval *a);
lval *builtin_join(lenv *env, lval *a);
void lenv_add_builtin(lenv *env, char *name, lbuiltin fn);
void lenv_add_builtins(lenv *env);
int main(int argc, char **argv);

char* ltype_name(int t) {
    switch (t) {
        case LVAL_INTEGER:
            return "Integer";
        case LVAL_DECIMAL:
            return "Decimal";
        case LVAL_ERROR:
            return "Error";
        case LVAL_SYMBOL:
            return "Symbol";
        case LVAL_SEXPRESSION:
            return "S-Expression";
        case LVAL_QEXPRESSION:
            return "Q-Expression";
        case LVAL_FUNCTION:
            return "Function";
        default:
            return "Unknown";
    }
}

lenv *lenv_new(void) {
    lenv *env = malloc(sizeof(lenv));
    E_PARENT(env) = NULL;
    E_COUNT(env) = 0;
    E_NAMES(env) = NULL;
    E_VALUES(env) = NULL;
    return env;
}

lenv *lenv_copy(lenv *env) {
    lenv *new_env = malloc(sizeof(lenv));
    E_PARENT(new_env) = E_PARENT(env);
    E_COUNT(new_env) = E_COUNT(env);
    E_VALUES(new_env) = malloc(sizeof(lval*) * E_COUNT(new_env));
    E_NAMES(new_env) = malloc(sizeof(char*) * E_COUNT(new_env));
    E_FOREACH(i, env) {
        E_NAMES_N(new_env, i) = malloc(strlen(E_NAMES_N(env, i)) + 1);
        strcpy(E_NAMES_N(new_env, i), E_NAMES_N(env, i));
        E_VALUES_N(new_env, i) = lval_copy(E_VALUES_N(env, i));
    }

    return new_env;
}

lval* lval_call(lenv *env, lval *func, lval *a) {
    // apply immediately if builtin
    if (L_BUILTIN(func) != NULL) {
        return L_BUILTIN(func)(env, a);
    }

    int given = L_COUNT(a);
    int total = L_FORMALS_COUNT(func);

    // while there are arguments to bind
    while (L_COUNT(a)) {
        // if we've ran out of formal arguments to bind
        if (L_FORMALS_COUNT(func) == 0) {
            lval_delete(a);
            return lval_error("Function passed too many arguments. Got %i, expected %i",
                              given, total);
        }

        lval *symbol = lval_pop(L_FORMALS(func), 0);

        // support for variable arguments in user-defined functions
        if (STR_EQ(L_SYMBOL(symbol), "&")) {
            // '&' should be followed by another symbol
            if (L_FORMALS_COUNT(func) != 1) {
                lval_delete(a);
                return lval_error("Function format invalid. "
                                  "Symbol '&' not followed by single symbol.");
            }

            lval *nsymbol = lval_pop(L_FORMALS(func), 0);
            lenv_put(L_ENV(func), nsymbol, builtin_list(env, a));
            lval_delete(symbol);
            lval_delete(nsymbol);

            break;
        }

        lval *value = lval_pop(a, 0);

        lenv_put(L_ENV(func), symbol, value);

        lval_delete(symbol);
        lval_delete(value);
    }

    lval_delete(a);

    if (L_FORMALS_COUNT(func) > 0
        && STR_EQ(L_SYMBOL(L_FORMALS_N(func, 0)), "&")) {
        if (L_FORMALS_COUNT(func) != 2) {
            return lval_error("Function format invalid. "
                              "Symbol '&' not followed by single symbol.");
        }

        lval_delete(lval_pop(L_FORMALS(func), 0));

        lval *symbol = lval_pop(L_FORMALS(func), 0);
        lval *value = lval_qexpression();

        lenv_put(L_ENV(func), symbol, value);
        lval_delete(symbol);
        lval_delete(value);
    }

    if (L_FORMALS_COUNT(func) == 0) {
        // if all formal have been bound => evaluate
        E_PARENT(L_ENV(func)) = env;
        return builtin_eval(
                            L_ENV(func),
                            lval_add(lval_sexpression(), lval_copy(L_BODY(func)))
                           );
    } else {
        // otherwise return partially evaluated function
        return lval_copy(func);
    }
}

void lval_delete(lval* v) {
    switch (L_TYPE(v)) {
        case LVAL_INTEGER:
        case LVAL_DECIMAL:
            break;

        case LVAL_ERROR:
            free(L_ERROR(v));
            break;

        case LVAL_SYMBOL:
            free(L_SYMBOL(v));
            break;

        case LVAL_QEXPRESSION:
        case LVAL_SEXPRESSION:
            // free memory for all elements inside
            FOREACH_SEXP(i, v) {
                lval_delete(L_CELL_N(v, i));
            }
            // free memory allocated to contain the pointers
            free(L_CELL(v));
            break;
        case LVAL_FUNCTION:
            if (!L_BUILTIN(v)) {
                lenv_delete(L_ENV(v));
                lval_delete(L_FORMALS(v));
                lval_delete(L_BODY(v));
            }
            break;
    }

    // free memory for 'lval' structure itself
    free(v);
}

void lenv_delete(lenv *env) {
    E_FOREACH(i, env) {
        free(E_NAMES_N(env, i));
        lval_delete(E_VALUES_N(env, i));
    }
    free(E_NAMES(env));
    free(E_VALUES(env));
    free(env);
}

lval* lval_copy(lval *a) {
    lval *x = malloc(sizeof(lval));
    L_TYPE(x) = L_TYPE(a);

    switch (L_TYPE(a)) {
        case LVAL_FUNCTION:
            if (L_BUILTIN(a)) {
                L_BUILTIN(x) = L_BUILTIN(a);
            } else {
                L_BUILTIN(x) = NULL;
                L_ENV(x) = lenv_copy(L_ENV(a));
                L_FORMALS(x) = lval_copy(L_FORMALS(a));
                L_BODY(x) = lval_copy(L_BODY(a));
            }
            break;
        case LVAL_INTEGER:
            L_INTEGER(x) = L_INTEGER(a);
            break;
        case LVAL_DECIMAL:
            L_DECIMAL(x) = L_DECIMAL(a);
            break;
        case LVAL_ERROR:
            L_ERROR(x) = malloc(strlen(L_ERROR(a)) + 1);
            strcpy(L_ERROR(x), L_ERROR(a));
            break;
        case LVAL_SYMBOL:
            L_SYMBOL(x) = malloc(strlen(L_SYMBOL(a)) + 1);
            strcpy(L_SYMBOL(x), L_SYMBOL(a));
            break;
        case LVAL_SEXPRESSION:
        case LVAL_QEXPRESSION:
            L_COUNT(x) = L_COUNT(a);
            L_CELL(x) = malloc(sizeof(lval*) * L_COUNT(x));
            FOREACH_SEXP(i, x) {
                L_CELL_N(x, i) = lval_copy(L_CELL_N(a, i));
            }
            break;
    }
    return x;
}

lval* lval_error(char* format, ...) {
    lval* v = malloc(sizeof(lval));
    L_TYPE(v) = LVAL_ERROR;

    va_list va;
    va_start(va, format);

    L_ERROR(v) = malloc(512);

    vsnprintf(L_ERROR(v), 511, format, va);

    L_ERROR(v) = realloc(L_ERROR(v), strlen(L_ERROR(v)) + 1);

    va_end(va);

    return v;
}

lval* lenv_get(lenv* env, lval *key) {
    E_FOREACH(i, env) {
        if (STR_EQ(E_NAMES_N(env, i), L_SYMBOL(key))) {
            return lval_copy(E_VALUES_N(env, i));
        }
    }

    if (E_PARENT(env)) {
        return lenv_get(E_PARENT(env), key);
    } else {
        return lval_error("Unbound symbol '%s'", L_SYMBOL(key));
    }
}

void lenv_def(lenv *env, lval *key, lval *value) {
    while (E_PARENT(env) != NULL) { env = E_PARENT(env); }
    lenv_put(env, key, value);
}

void lenv_put(lenv *env, lval *key, lval *value) {
    // replace existing variables
    E_FOREACH(i, env) {
        if (STR_EQ(E_NAMES_N(env, i), L_SYMBOL(key))) {
            lval_delete(E_VALUES_N(env, i));
            E_VALUES_N(env, i) = lval_copy(value);
            return;
        }
    }

    E_COUNT(env)++;
    E_VALUES(env) = realloc(E_VALUES(env), sizeof(lval*) * E_COUNT(env));
    E_NAMES(env) = realloc(E_NAMES(env), sizeof(char*) * E_COUNT(env));

    E_VALUES_N(env, E_COUNT(env) - 1) = lval_copy(value);
    E_NAMES_N(env, E_COUNT(env) - 1) = malloc(strlen(L_SYMBOL(key)) + 1);
    strcpy(E_NAMES_N(env, E_COUNT(env) - 1), L_SYMBOL(key));
}

lval* lval_lambda(lval *formals, lval *body) {
    lval *v = malloc(sizeof(lval));
    L_TYPE(v) = LVAL_FUNCTION;
    L_BUILTIN(v) = NULL;
    L_ENV(v) = lenv_new();
    L_FORMALS(v) = formals;
    L_BODY(v) = body;

    return v;
}

lval* lval_function(lbuiltin fn) {
    lval* v = malloc(sizeof(lval));
    L_TYPE(v) = LVAL_FUNCTION;
    L_BUILTIN(v) = fn;
    return v;
}

lval* lval_qexpression(void) {
    lval* v = malloc(sizeof(lval));
    L_TYPE(v) = LVAL_QEXPRESSION;
    L_COUNT(v) = 0;
    L_CELL(v) = NULL;
    return v;
}

lval* lval_integer(long x) {
    lval* v = malloc(sizeof(lval));
    L_TYPE(v) = LVAL_INTEGER;
    L_INTEGER(v) = x;
    return v;
}

lval* lval_decimal(double x) {
    lval* v = malloc(sizeof(lval));
    L_TYPE(v) = LVAL_DECIMAL;
    L_DECIMAL(v) = x;
    return v;
}

lval* lval_symbol(char* m) {
    lval* v = malloc(sizeof(lval));
    L_TYPE(v) = LVAL_SYMBOL;
    L_SYMBOL(v) = malloc(strlen(m) + 1);
    strcpy(L_ERROR(v), m);
    return v;
}

lval* lval_sexpression(void) {
    lval* v = malloc(sizeof(lval));
    L_TYPE(v) = LVAL_SEXPRESSION;
    L_COUNT(v) = 0;
    L_CELL(v) = NULL;
    return v;
}


lval* lval_read_number(mpc_ast_t* t) {
    errno = 0;
    if (STR_CONTAIN(t->tag, "integer")) {
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE
            ? lval_integer(x)
            : lval_error("Invalid integer");
    }
    if (STR_CONTAIN(t->tag, "decimal")) {
        double x = strtod(t->contents, NULL);
        return errno != ERANGE
            ? lval_decimal(x)
            : lval_error("Invalid decimal");
    }
    return lval_error("Unknown number type");
}

lval* lval_add(lval* v, lval* x) {
    L_COUNT(v)++;
    L_CELL(v) = realloc(L_CELL(v), sizeof(lval*) * L_COUNT(v));
    v->cell[L_COUNT(v) - 1] = x;
    return v;
}

lval* lval_read(mpc_ast_t* t) {
    if (STR_CONTAIN(t->tag, "number")) {
        return lval_read_number(t);
    }
    if (STR_CONTAIN(t->tag, "symbol")) {
        return lval_symbol(t->contents);
    }

    // if root or sexpr then create empty list
    lval* x = NULL;
    if (STR_EQ(t->tag, ">")) {
        x = lval_sexpression();
    }

    if (STR_CONTAIN(t->tag, "sexpr")) {
        x = lval_sexpression();
    }

    if (STR_CONTAIN(t->tag, "qexpr")) {
        x = lval_qexpression();
    }

    // fill the list with any valid expression contained within
    for (int i = 0; i < t->children_num; i++) {
        if (STR_EQ(t->children[i]->contents, "(")) { continue; }
        if (STR_EQ(t->children[i]->contents, ")")) { continue; }
        if (STR_EQ(t->children[i]->contents, "{")) { continue; }
        if (STR_EQ(t->children[i]->contents, "}")) { continue; }
        if (STR_EQ(t->children[i]->tag, "regex")) { continue; }

        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}


void lval_expr_print(lenv *env, lval *v, char open, char close) {
    putchar(open);
    FOREACH_SEXP(i, v) {
        // print value contained within
        lval_print(env, L_CELL_N(v, i));

        // don't print trailing space if last element
        if (i != (L_COUNT(v) - 1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_print(lenv *env, lval *v) {
    switch (L_TYPE(v)) {
        case LVAL_INTEGER:
            printf("%li", L_INTEGER(v));
            break;
        case LVAL_DECIMAL:
            printf("%f", L_DECIMAL(v));
            break;
        case LVAL_SYMBOL:
            printf("%s", L_SYMBOL(v));
            break;
        case LVAL_SEXPRESSION:
            lval_expr_print(env, v, '(', ')');
            break;
        case LVAL_QEXPRESSION:
            lval_expr_print(env, v, '{', '}');
            break;
        case LVAL_ERROR:
            printf("Error: %s", L_ERROR(v));
            break;
        case LVAL_FUNCTION:
            if (L_BUILTIN(v)) {
                E_FOREACH(i, env) {
                    if (L_BUILTIN(E_VALUES_N(env, i)) == L_BUILTIN(v)) {
                        printf("<builtin function '%s'>", E_NAMES_N(env, i));
                    }
                }
            } else {
                printf("(\\ ");
                lval_print(env, L_FORMALS(v));
                putchar(' ');
                lval_print(env, L_BODY(v));
                putchar(')');
            }

            break;
        default:
            printf("Error: Unknown value type %d", L_TYPE(v));
            break;
    }
}

void lval_println(lenv *env, lval *v) {
    lval_print(env, v);
    putchar('\n');
}

lval* lval_pop(lval* v, int i) {
    lval* x = L_CELL_N(v, i);

    // shift memory after the item at 'i' over the top and reallocate memory used
    memmove(&L_CELL_N(v, i), &v->cell[i + 1], sizeof(lval*) * (L_COUNT(v) - i - 1));
    L_COUNT(v)--;
    L_CELL(v) = realloc(L_CELL(v), sizeof(lval*) * L_COUNT(v));

    return x;
}

lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_delete(v);
    return x;
}

lval* lval_join(lval* x, lval* y) {
    while (L_COUNT(y)) {
        x = lval_add(x, lval_pop(y, 0));
    }
    lval_delete(y);
    return x;
}

BUILTIN(gt) {
    return builtin_ord(env, a, ">");
}

BUILTIN(lt) {
    return builtin_ord(env, a, "<");
}

BUILTIN(ge) {
    return builtin_ord(env, a, ">=");
}

BUILTIN(le) {
    return builtin_ord(env, a, "<=");
}

lval *builtin_ord(lenv *env, lval *a, char *operator) {
    LASSERT_ARGUMENT_NUMBER(a, 2, operator);
    LASSERT(a,
            L_TYPE_N(a, 0) == LVAL_INTEGER || L_TYPE_N(a, 0) == LVAL_DECIMAL,
            "Incorrect type of argument #%d for '%s'. Got %s, expected %s or %s.",
            0,
            operator,
            ltype_name(L_TYPE_N(a, 0)),
            ltype_name(LVAL_INTEGER),
            ltype_name(LVAL_DECIMAL)
            );
    LASSERT(a,
            L_TYPE_N(a, 1) == LVAL_INTEGER || L_TYPE_N(a, 1) == LVAL_DECIMAL,
            "Incorrect type of argument #%d for '%s'. Got %s, expected %s or %s.",
            1,
            operator,
            ltype_name(L_TYPE_N(a, 1)),
            ltype_name(LVAL_INTEGER),
            ltype_name(LVAL_DECIMAL)
            );

    int result;
    if (STR_EQ(operator, ">")) {
        result = (L_CELL_N(a, 0) > L_CELL_N(a, 1));
    }
    if (STR_EQ(operator, "<")) {
        result = (L_CELL_N(a, 0) < L_CELL_N(a, 1));
    }
    if (STR_EQ(operator, ">=")) {
        result = (L_CELL_N(a, 0) >= L_CELL_N(a, 1));
    }
    if (STR_EQ(operator, "<=")) {
        result = (L_CELL_N(a, 0) <= L_CELL_N(a, 1));
    }

    lval_delete(a);
    return lval_integer(result);
}

BUILTIN(lambda) {
    LASSERT_ARGUMENT_NUMBER(a, 2, "\\");
    LASSERT_ARGUMENT_TYPE(a, 0, LVAL_QEXPRESSION, "\\");
    LASSERT_ARGUMENT_TYPE(a, 1, LVAL_QEXPRESSION, "\\");

    // First Q-Expression should contain only symbols
    FOREACH_SEXP(i, a) {
        LASSERT(a, L_TYPE_N(L_CELL_N(a, 0), i) == LVAL_SYMBOL,
                "Cannot define non-symbol. Got %s, expected %s.",
                ltype_name(L_TYPE_N(L_CELL_N(a, 0), i)), ltype_name(LVAL_SYMBOL));
    }

    lval *formals = lval_pop(a, 0);
    lval *body = lval_pop(a, 0);
    lval_delete(a);

    return lval_lambda(formals, body);
}

lval* builtin_op(char *operator, lenv *env, lval *a) {
    FOREACH_SEXP(i, a) {
        if (L_TYPE_N(a, i) != LVAL_INTEGER && L_TYPE_N(a, i) != LVAL_DECIMAL) {
            lval_delete(a);
            return lval_error("Cannot operate on non-number");
        }
    }

    lval* x = lval_pop(a, 0);

    // unary negation
    if (STR_EQ(operator, "-") && L_COUNT(a) == 0) {
        switch(L_TYPE(x)) {
            case LVAL_INTEGER:
                L_INTEGER(x) = -L_INTEGER(x);
                break;
            case LVAL_DECIMAL:
                L_DECIMAL(x) = -L_DECIMAL(x);
                break;
        }
    }

    while (L_COUNT(a) > 0) {
        lval* y = lval_pop(a, 0);

        if (STR_EQ(operator, "+") || STR_EQ(operator, "add")) {
            if (L_TYPE(x) == LVAL_DECIMAL && L_TYPE(y) == LVAL_DECIMAL) {
                L_DECIMAL(x) += L_DECIMAL(y);
            } else if (L_TYPE(x) == LVAL_DECIMAL && L_TYPE(y) == LVAL_INTEGER) {
                L_DECIMAL(x) += L_INTEGER(y);
            } else if (L_TYPE(x) == LVAL_INTEGER && L_TYPE(y) == LVAL_DECIMAL) {
                L_INTEGER(x) += (int)L_DECIMAL(y);
            } else {
                L_INTEGER(x) += L_INTEGER(y);
            }
        }

        if (STR_EQ(operator, "-") || STR_EQ(operator, "sub")) {
            if (L_TYPE(x) == LVAL_DECIMAL && L_TYPE(y) == LVAL_DECIMAL) {
                L_DECIMAL(x) -= L_DECIMAL(y);
            } else if (L_TYPE(x) == LVAL_DECIMAL && L_TYPE(y) == LVAL_INTEGER) {
                L_DECIMAL(x) -= L_INTEGER(y);
            } else if (L_TYPE(x) == LVAL_INTEGER && L_TYPE(y) == LVAL_DECIMAL) {
                L_INTEGER(x) -= (int)L_DECIMAL(y);
            } else {
                L_INTEGER(x) -= L_INTEGER(y);
            }
        }

        if (STR_EQ(operator, "*") || STR_EQ(operator, "mul")) {
            if (L_TYPE(x) == LVAL_DECIMAL && L_TYPE(y) == LVAL_DECIMAL) {
                L_DECIMAL(x) *= L_DECIMAL(y);
            } else if (L_TYPE(x) == LVAL_DECIMAL && L_TYPE(y) == LVAL_INTEGER) {
                L_DECIMAL(x) *= L_INTEGER(y);
            } else if (L_TYPE(x) == LVAL_INTEGER && L_TYPE(y) == LVAL_DECIMAL) {
                L_INTEGER(x) *= (int)L_DECIMAL(y);
            } else {
                L_INTEGER(x) *= L_INTEGER(y);
            }
        }

        if (STR_EQ(operator, "/") || STR_EQ(operator, "div")) {
            switch (L_TYPE(y)) {
                case LVAL_DECIMAL:
                    if (L_DECIMAL(y) == 0.0) {
                        lval_delete(x);
                        lval_delete(y);
                        lval_delete(a);
                        return lval_error("Division by zero");
                    }
                    break;
                case LVAL_INTEGER:
                    if (L_INTEGER(y) == 0) {
                        lval_delete(x);
                        lval_delete(y);
                        lval_delete(a);
                        return lval_error("Division by zero");
                    }
                    break;
            }

            if (L_TYPE(x) == LVAL_DECIMAL && L_TYPE(y) == LVAL_DECIMAL) {
                L_DECIMAL(x) /= L_DECIMAL(y);
            } else if (L_TYPE(x) == LVAL_DECIMAL && L_TYPE(y) == LVAL_INTEGER) {
                L_DECIMAL(x) /= L_INTEGER(y);
            } else if (L_TYPE(x) == LVAL_INTEGER && L_TYPE(y) == LVAL_DECIMAL) {
                L_INTEGER(x) /= L_DECIMAL(y);
            } else {
                L_INTEGER(x) /= L_INTEGER(y);
            }
        }

        if (STR_EQ(operator, "%") || STR_EQ(operator, "mod")) {
            switch (L_TYPE(y)) {
                case LVAL_DECIMAL:
                    if (L_DECIMAL(y) == 0.0) {
                        lval_delete(x);
                        lval_delete(y);
                        lval_delete(a);
                        return lval_error("Division by zero");
                    }
                    break;
                case LVAL_INTEGER:
                    if (L_INTEGER(y) == 0) {
                        lval_delete(x);
                        lval_delete(y);
                        lval_delete(a);
                        return lval_error("Division by zero");
                    }
                    break;
            }

            if (L_TYPE(x) == LVAL_DECIMAL && L_TYPE(y) == LVAL_DECIMAL) {
                L_DECIMAL(x) = fmod(L_DECIMAL(x), L_DECIMAL(y));
            } else if (L_TYPE(x) == LVAL_DECIMAL && L_TYPE(y) == LVAL_INTEGER) {
                L_DECIMAL(x) = fmod(L_DECIMAL(x), L_INTEGER(y));
            } else if (L_TYPE(x) == LVAL_INTEGER && L_TYPE(y) == LVAL_DECIMAL) {
                L_INTEGER(x) = fmod(L_INTEGER(x), (int)L_DECIMAL(y));
            } else {
                L_INTEGER(x) = (int)fmod(L_INTEGER(x), L_INTEGER(y));
            }
        }

        if (STR_EQ(operator, "^")) {
            if (L_TYPE(x) == LVAL_DECIMAL && L_TYPE(y) == LVAL_DECIMAL) {
                L_DECIMAL(x) = pow(L_DECIMAL(x), L_DECIMAL(y));
            } else if (L_TYPE(x) == LVAL_DECIMAL && L_TYPE(y) == LVAL_INTEGER) {
                L_DECIMAL(x) = pow(L_DECIMAL(x), L_INTEGER(y));
            } else if (L_TYPE(x) == LVAL_INTEGER && L_TYPE(y) == LVAL_DECIMAL) {
                L_INTEGER(x) = (int)pow(L_INTEGER(x), L_DECIMAL(y));
            } else {
                L_INTEGER(x) = pow(L_INTEGER(x), L_INTEGER(y));
            }
        }

        if (STR_EQ(operator, "min")) {
            if (L_TYPE(x) == LVAL_DECIMAL && L_TYPE(y) == LVAL_DECIMAL) {
                L_DECIMAL(x) = L_DECIMAL(x) < L_DECIMAL(y)
                    ? L_DECIMAL(x)
                    : L_DECIMAL(y);
            } else if (L_TYPE(x) == LVAL_DECIMAL && L_TYPE(y) == LVAL_INTEGER) {
                L_DECIMAL(x) = L_DECIMAL(x) < L_INTEGER(y)
                    ? L_DECIMAL(x)
                    : (double)L_INTEGER(y);
            } else if (L_TYPE(x) == LVAL_INTEGER && L_TYPE(y) == LVAL_DECIMAL) {
                L_INTEGER(x) = L_INTEGER(x) < L_DECIMAL(y)
                    ? L_INTEGER(x)
                    : (int)L_DECIMAL(y);
            } else {
                L_INTEGER(x) = L_INTEGER(x) < L_INTEGER(y)
                    ? L_INTEGER(x)
                    : L_INTEGER(y);
            }
        }
        if (STR_EQ(operator, "max")) {
            if (L_TYPE(x) == LVAL_DECIMAL && L_TYPE(y) == LVAL_DECIMAL) {
                L_DECIMAL(x) = L_DECIMAL(x) > L_DECIMAL(y)
                    ? L_DECIMAL(x)
                    : L_DECIMAL(y);
            } else if (L_TYPE(x) == LVAL_DECIMAL && L_TYPE(y) == LVAL_INTEGER) {
                L_DECIMAL(x) = L_DECIMAL(x) > L_INTEGER(y)
                    ? L_DECIMAL(x)
                    : (double)L_INTEGER(y);
            } else if (L_TYPE(x) == LVAL_INTEGER && L_TYPE(y) == LVAL_DECIMAL) {
                L_INTEGER(x) = L_INTEGER(x) > L_DECIMAL(y)
                    ? L_INTEGER(x)
                    : (int)L_DECIMAL(y);
            } else {
                L_INTEGER(x) = L_INTEGER(x) > L_INTEGER(y)
                    ? L_INTEGER(x)
                    : L_INTEGER(y);
            }
        }

        lval_delete(y);
    }

    lval_delete(a);
    return x;
}

BUILTIN(add) {
    return builtin_op("+", env, a);
}

BUILTIN(sub) {
    return builtin_op("-", env, a);
}

BUILTIN(mul) {
    return builtin_op("*", env, a);
}

BUILTIN(div) {
    return builtin_op("/", env, a);
}

BUILTIN(mod) {
    return builtin_op("%", env, a);
}

BUILTIN(pow) {
    return builtin_op("^", env, a);
}

BUILTIN(min) {
    FOREACH_SEXP(i, a) {
        if (L_TYPE_N(a, i) != LVAL_INTEGER && L_TYPE_N(a, i) != LVAL_DECIMAL) {
            lval_delete(a);
            return lval_error("Cannot operate on non-number");
        }
    }

    lval* x = lval_pop(a, 0);

    while (L_COUNT(a) > 0) {
        lval* y = lval_pop(a, 0);

        if (L_TYPE(x) == LVAL_DECIMAL && L_TYPE(y) == LVAL_DECIMAL) {
            L_DECIMAL(x) = L_DECIMAL(x) < L_DECIMAL(y)
                ? L_DECIMAL(x)
                : L_DECIMAL(y);
        } else if (L_TYPE(x) == LVAL_DECIMAL && L_TYPE(y) == LVAL_INTEGER) {
            L_DECIMAL(x) = L_DECIMAL(x) < L_INTEGER(y)
                ? L_DECIMAL(x)
                : (double)L_INTEGER(y);
        } else if (L_TYPE(x) == LVAL_INTEGER && L_TYPE(y) == LVAL_DECIMAL) {
            L_INTEGER(x) = L_INTEGER(x) < L_DECIMAL(y)
                ? L_INTEGER(x)
                : (int)L_DECIMAL(y);
        } else {
            L_INTEGER(x) = L_INTEGER(x) < L_INTEGER(y)
                ? L_INTEGER(x)
                : L_INTEGER(y);
        }

        lval_delete(y);
    }

    lval_delete(a);
    return x;
}

BUILTIN(max) {
    FOREACH_SEXP(i, a) {
        if (L_TYPE_N(a, i) != LVAL_INTEGER && L_TYPE_N(a, i) != LVAL_DECIMAL) {
            lval_delete(a);
            return lval_error("Cannot operate on non-number");
        }
    }

    lval* x = lval_pop(a, 0);

    while (L_COUNT(a) > 0) {
        lval* y = lval_pop(a, 0);

        if (L_TYPE(x) == LVAL_DECIMAL && L_TYPE(y) == LVAL_DECIMAL) {
            L_DECIMAL(x) = L_DECIMAL(x) > L_DECIMAL(y)
                ? L_DECIMAL(x)
                : L_DECIMAL(y);
        } else if (L_TYPE(x) == LVAL_DECIMAL && L_TYPE(y) == LVAL_INTEGER) {
            L_DECIMAL(x) = L_DECIMAL(x) > L_INTEGER(y)
                ? L_DECIMAL(x)
                : (double)L_INTEGER(y);
        } else if (L_TYPE(x) == LVAL_INTEGER && L_TYPE(y) == LVAL_DECIMAL) {
            L_INTEGER(x) = L_INTEGER(x) > L_DECIMAL(y)
                ? L_INTEGER(x)
                : (int)L_DECIMAL(y);
        } else {
            L_INTEGER(x) = L_INTEGER(x) > L_INTEGER(y)
                ? L_INTEGER(x)
                : L_INTEGER(y);
        }

        lval_delete(y);
    }

    lval_delete(a);
    return x;
}

BUILTIN(cons) {
    LASSERT_ARGUMENT_NUMBER(a, 2, "cons");
    LASSERT_ARGUMENT_TYPE(a, 1, LVAL_QEXPRESSION, "cons");

    lval* x = lval_pop(a, 0);
    lval* v = lval_pop(a, 0);
    lval_delete(a);

    L_COUNT(v)++;
    // realloc and unshift memory
    L_CELL(v) = realloc(L_CELL(v), sizeof(lval*) * L_COUNT(v));
    memmove(&L_CELL_N(v, 1), &L_CELL_N(v, 0), sizeof(lval*) * (L_COUNT(v) - 1));
    L_CELL_N(v, 0) = x;

    return v;
}

BUILTIN(init) {
    LASSERT_ARGUMENT_NUMBER(a, 1, "init");
    LASSERT_ARGUMENT_TYPE(a, 0, LVAL_QEXPRESSION, "init");
    LASSERT_NOT_EMPTY_QEXPR(a, "init");

    lval* v = lval_take(a, 0);
    // delete last element and return
    lval_delete(lval_pop(v, (L_COUNT(v) - 1)));
    return v;
}

BUILTIN(head) {
    LASSERT_ARGUMENT_NUMBER(a, 1, "head");
    LASSERT_ARGUMENT_TYPE(a, 0, LVAL_QEXPRESSION, "head");
    LASSERT_NOT_EMPTY_QEXPR(a, "head");

    lval* v = lval_take(a, 0);
    // delete all elements that are not head and return
    while (L_COUNT(v) > 1) {
        lval_delete(lval_pop(v, 1));
    }
    return v;
}

BUILTIN(tail) {
    LASSERT_ARGUMENT_NUMBER(a, 1, "tail");
    LASSERT_ARGUMENT_TYPE(a, 0, LVAL_QEXPRESSION, "tail");
    LASSERT_NOT_EMPTY_QEXPR(a, "tail");

    lval* v = lval_take(a, 0);
    // delete first element and return
    lval_delete(lval_pop(v, 0));
    return v;
}

BUILTIN(len) {
    LASSERT_ARGUMENT_NUMBER(a, 1, "len");
    LASSERT_ARGUMENT_TYPE(a, 0, LVAL_QEXPRESSION, "len");

    lval* x = lval_take(a, 0);
    int len = L_COUNT(x);
    lval_delete(x);

    return lval_integer(len);
}

BUILTIN(list) {
    L_TYPE(a) = LVAL_QEXPRESSION;
    return a;
}

lval* builtin_var(lenv *env, lval *a, char *func) {
    LASSERT_ARGUMENT_TYPE(a, 0, LVAL_QEXPRESSION, func);

    lval *symbols = L_CELL_N(a, 0);
    FOREACH_SEXP(i, symbols) {
        LASSERT(a, L_TYPE_N(symbols, i) == LVAL_SYMBOL,
                "Function '%s' cannot define non-symbol. Gor %s, expected %s.",
                func, ltype_name(L_TYPE_N(symbols, i)), ltype_name(LVAL_SYMBOL));
    }

    LASSERT(a, L_COUNT(symbols) == L_COUNT(a) - 1,
            "Function '%s' passed too many arguments for symbols. Got %i, expected %i.",
            func, L_COUNT(symbols), L_COUNT(a) - 1);

    FOREACH_SEXP(i, symbols) {
        if (STR_EQ(func, "def")) {
            lenv_def(env, L_CELL_N(symbols, i), L_CELL_N(a, i + 1));
        }

        if (STR_EQ(func, "=")) {
            lenv_put(env, L_CELL_N(symbols, i), L_CELL_N(a, i + 1));
        }
    }

    lval_delete(a);
    return lval_sexpression();
}

BUILTIN(def) {
    return builtin_var(env, a, "def");
}

BUILTIN(put) {
    return builtin_var(env, a, "=");
}

BUILTIN(exit) {
    exit(0);
}

lval* lval_eval_sexpr(lenv *env, lval *v) {
    // evaluate children
    FOREACH_SEXP(i, v) {
        L_CELL_N(v, i) = lval_eval(env, L_CELL_N(v, i));
    }

    // check errors
    FOREACH_SEXP(i, v) {
        if (L_TYPE_N(v, i) == LVAL_ERROR) {
            return lval_take(v, i);
        }
    }

    // empty expression
    if (L_COUNT(v) == 0) {
        return v;
    }

    // single expression
    if (L_COUNT(v) == 1) {
        return lval_take(v, 0);
    }

    // first element should be Function
    lval *f = lval_pop(v, 0);
    if (L_TYPE(f) != LVAL_FUNCTION) {
        lval_delete(f);
        lval_delete(v);
        return lval_error("First element is not a function!");
    }

    lval *result = lval_call(env, f, v);
    lval_delete(f);

    return result;
}

lval* lval_eval(lenv *env, lval *v) {
    if (L_TYPE(v) == LVAL_SYMBOL) {
        lval *x = lenv_get(env, v);
        // shortcut for exit function
        if (L_TYPE(x) == LVAL_FUNCTION && STR_EQ(L_SYMBOL(v), "exit")) {
            builtin_exit(env, v);
        }
        lval_delete(v);
        return x;
    }
    // only evaluate s-expressions
    if (L_TYPE(v) == LVAL_SEXPRESSION) {
        return lval_eval_sexpr(env, v);
    }
    return v;
}

BUILTIN(eval) {
    LASSERT_ARGUMENT_NUMBER(a, 1, "eval");
    LASSERT_ARGUMENT_TYPE(a, 0, LVAL_QEXPRESSION, "eval");

    lval* x = lval_take(a, 0);
    L_TYPE(x) = LVAL_SEXPRESSION;
    return lval_eval(env, x);
}

BUILTIN(join) {
    FOREACH_SEXP(i, a) {
        LASSERT_ARGUMENT_TYPE(a, i, LVAL_QEXPRESSION, "join");
    }

    lval* x = lval_pop(a, 0);
    while (L_COUNT(a)) {
        x = lval_join(x, lval_pop(a, 0));
    }
    lval_delete(a);
    return x;
}

void lenv_add_builtin(lenv *env, char *name, lbuiltin fn) {
    lval *key = lval_symbol(name);
    lval *value = lval_function(fn);
    lenv_put(env, key, value);
    lval_delete(key);
    lval_delete(value);
}

void lenv_add_builtins(lenv *env) {
    /* List Functions */
    lenv_add_builtin(env, "list", builtin_list);
    lenv_add_builtin(env, "head", builtin_head);
    lenv_add_builtin(env, "tail", builtin_tail);
    lenv_add_builtin(env, "eval", builtin_eval);
    lenv_add_builtin(env, "join", builtin_join);
    lenv_add_builtin(env, "cons", builtin_cons);
    lenv_add_builtin(env, "len", builtin_len);
    lenv_add_builtin(env, "init", builtin_init);
    lenv_add_builtin(env, "min", builtin_min);
    lenv_add_builtin(env, "min", builtin_max);

    /* Mathematical Functions */
    lenv_add_builtin(env, "+", builtin_add);
    lenv_add_builtin(env, "-", builtin_sub);
    lenv_add_builtin(env, "*", builtin_mul);
    lenv_add_builtin(env, "/", builtin_div);
    lenv_add_builtin(env, "%", builtin_div);
    lenv_add_builtin(env, "^", builtin_pow);

    /* Variable Functions */
    lenv_add_builtin(env, "def", builtin_def);
    lenv_add_builtin(env, "=", builtin_put);

    /* Other */
    lenv_add_builtin(env, "exit", builtin_exit);
    lenv_add_builtin(env, "\\", builtin_lambda);
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

    // todo [10.10.2015]: use mpca_lang_file and handle errors
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

    lenv *env = lenv_new();
    lenv_add_builtins(env);

    puts("lliisspp version 0.0.1");
    puts("Press Ctrl+C to Exit\n");

    while (1) {
        char* input = readline("lliisspp> ");

        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lliisspp, &r)) {
            lval* x = lval_eval(env, lval_read(r.output));
            lval_println(env, x);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    lenv_delete(env);
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
