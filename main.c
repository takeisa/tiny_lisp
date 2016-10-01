#include "mpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <editline/readline.h>
#include <editline/history.h>

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

typedef lval *(*lbuiltin)(lenv *, lval *);

typedef struct lval {
  int type;

  /* Basic */
  long num;
  char *err;
  char *sym;

  /* Function */
  lbuiltin builtin;
  lenv *env;
  lval *formals;
  lval *body;
  
  /* Expression */
  int count;
  struct lval **cell;
} lval;

lenv *lenv_new();
void lenv_del(lenv *e);
lenv *lenv_copy(lenv *e);

void lval_print(lval *v);
lval *lval_eval(lenv *e, lval *v);
lval *lval_add_cell(lval *v, lval *a);

lval *builtin_eval(lenv *e, lval *arg);
lval *builtin_list(lenv *e, lval *arg);

enum {
  LVAL_NUM,
  LVAL_SYM,
  LVAL_FUN,
  LVAL_SEXPR,
  LVAL_QEXPR,
  LVAL_ERR,
};

typedef struct lenv {
  lenv *parent;
  int count;
  char **syms;
  lval **vals;
} lenv;

char *ltype_name(int t) {
  switch (t) {
  case LVAL_NUM: return "Number";
  case LVAL_SYM: return "Symbol";
  case LVAL_FUN: return "Function";
  case LVAL_SEXPR: return "S-Expression";
  case LVAL_QEXPR: return "Q-Expression";
  default: return "Unknown";
  }
}

lval *lval_num(long num) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = num;
  return v;
}

lval *lval_err(char const *format, ...) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_ERR;

  va_list va;
  va_start(va, format);

  v->err = malloc(512);
  vsnprintf(v->err, 512 - 1, format, va);
  v->err = realloc(v->err, strlen(v->err) + 1);

  va_end(va);

  return v;
}

lval *lval_sym(char const *sym) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(sym) + 1);
  strcpy(v->sym, sym);
  return v;
}

lval *lval_fun(lbuiltin builtin) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->builtin = builtin;
  return v;
}

lval *lval_sexpr(void) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval *lval_qexpr(void) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval *lval_lambda(lval *formals, lval *body) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->builtin = NULL;
  v->env = lenv_new();
  v->formals = formals;
  v->body = body;
  return v;
}

void lval_del(lval *v) {
  switch (v->type) {
  case LVAL_NUM:
    break;
  case LVAL_FUN:
    if (!(v->builtin)) {
      lenv_del(v->env);
      lval_del(v->formals);
      lval_del(v->body);
    }
    break;
  case LVAL_ERR:
    free(v->err);
    break;
  case LVAL_SYM:
    free(v->sym);
    break;
  case LVAL_SEXPR:
  case LVAL_QEXPR:
    for (int i = 0; i < v->count; i++) {
      lval_del(v->cell[i]);
    }
    free(v->cell);
    break;
  }
  free(v);
}

lval *lval_read_num(mpc_ast_t *t) {
  long num = strtol(t->contents, NULL, 10);
  return errno == ERANGE
    ? lval_err("invalid number")
    : lval_num(num);
}

lval *lval_add_cell(lval *v, lval *a) {
  v->cell = realloc(v->cell, sizeof(lval) * (v->count + 1));
  v->cell[v->count] = a;
  v->count++;
  return v;
}

lval *lval_read(mpc_ast_t *t) {
  if (strstr(t->tag, "number")) {
    return lval_read_num(t);
  }
  if (strstr(t->tag, "symbol")) {
    return lval_sym(t->contents);
  }

  lval *v = NULL;
  if (strcmp(t->tag, ">") == 0
      || strstr(t->tag, "sexpr")) {
    v = lval_sexpr();
  }

  if (strstr(t->tag, "qexpr")) {
    v = lval_qexpr();
  }

  for (int i = 0; i < t->children_num; i++) {
    mpc_ast_t *child = t->children[i];
    if (strstr(child->tag, "number")
	|| strstr(child->tag, "symbol")
	|| strstr(child->tag, "sexpr")
	|| strstr(child->tag, "expr")) {
      v = lval_add_cell(v, lval_read(t->children[i]));
    }
  }

  return v;
}

void lval_expr_print(lval const *v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {
    lval_print(v->cell[i]);
    if (i < v->count - 1) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_print(lval *v) {
  switch (v->type) {
  case LVAL_NUM:
    printf("%li", v->num);
    break;
  case LVAL_ERR:
    printf("Error: %s", v->err);
    break;
  case LVAL_SYM:
    printf("%s", v->sym);
    break;
  case LVAL_FUN:
    if (v->builtin) {
      printf("<function>");
    } else {
      printf("(\\ ");
      lval_print(v->formals);
      putchar(' ');
      lval_print(v->body);
      putchar(')');
    }
    break;
  case LVAL_SEXPR:
    lval_expr_print(v, '(', ')');
    break;
  case LVAL_QEXPR:
    lval_expr_print(v, '{', '}');
    break;
  }
}

void lval_println(lval *v) {
  lval_print(v);
  putchar('\n');
}

lval *lval_pop(lval *v, int i) {
  lval *x = v->cell[i];

  memmove(&v->cell[i], &v->cell[i + 1],
	  sizeof(lval*) * (v->count - (i + 1)));

  v->count--;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  return x;
}

lval *lval_take(lval *v, int i) {
  lval *x = lval_pop(v, i);
  lval_del(v);
  return x;
}

lval *lval_copy(lval *v) {
  lval *x = malloc(sizeof(lval));
  x->type = v->type;
  
  switch(v->type) {
  case LVAL_NUM:
    x->num = v->num;
    break;
  case LVAL_SYM:
    x->sym = malloc(strlen(v->sym) + 1);
    strcpy(x->sym, v->sym);
    break;
  case LVAL_FUN:
    if (v->builtin) {
      x->builtin = v->builtin;
    } else {
      x->builtin = NULL;
      x->env = lenv_copy(v->env);
      x->formals = lval_copy(v->formals);
      x->body = lval_copy(v->body);
    }
    break;
  case LVAL_SEXPR:
  case LVAL_QEXPR:
    x->count = v->count;
    x->cell = malloc(sizeof(lval *) * x->count);
    for (int i = 0; i < x->count; i++) {
      x->cell[i] = lval_copy(v->cell[i]);
    }
    break;
  case LVAL_ERR:
    x->err = malloc(sizeof(v->err) + 1);
    strcpy(x->err, v->err);
    break;
  }

  return x;
}

lenv *lenv_new(void) {
  lenv *e = malloc(sizeof(lenv));
  e->parent = NULL;
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

void lenv_del(lenv *e) {
  for (int i = 0; i < e->count; i++) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }
  free(e->syms);
  free(e->vals);
  free(e);
}

lval *lenv_get(lenv *e, lval *k) {
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      return lval_copy(e->vals[i]);
    }
  }
  if (e->parent) {
    return lenv_get(e->parent, k);
  } else {
    return lval_err("unbound symbol '%s'", k->sym);
  }
}

void lenv_put(lenv *e, lval *k, lval *v) {
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(v);
      return;
    }
  }

  e->count++;
  e->syms = realloc(e->syms, sizeof(char *) * e->count);
  e->vals = realloc(e->vals, sizeof(lval *) * e->count);

  e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
  strcpy(e->syms[e->count - 1], k->sym);

  e->vals[e->count - 1] = lval_copy(v);
}

void lenv_def(lenv *e, lval *k, lval *v) {
  while (e->parent) {
    e = e->parent;
  }

  lenv_put(e, k, v);
}

lenv *lenv_copy(lenv *e) {
  lenv *n = malloc(sizeof(lenv));
  n->parent = e->parent;
  n->count = e->count;
  n->syms = malloc(sizeof(char *) * n->count);
  n->vals = malloc(sizeof(lval *) * n->count);

  for (int i = 0; i < e->count; i++) {
    n->syms[i] = malloc(strlen(e->syms[i]) + 1);
    strcpy(n->syms[i], e->syms[i]);
    n->vals[i] = lval_copy(e->vals[i]);
  }
  
  return n;
}

#define LASSERT(arg, cond, format, ...) \
  if (!(cond)) { \
    lval_del(arg); \
    return lval_err(format, ##__VA_ARGS__); \
  }

#define LASSERT_NUM(func, arg, num) \
  LASSERT(arg, \
    arg->count == num, \
    "Function '%s' passed incorrect number of arguments. " \
    "Got %i, Expected %i." \
    func, arg->count, num)

#define LASSERT_TYPE(func, arg, index, expect_type)        \
  LASSERT(arg, \
    arg->cell[index]->type == expect_type, \
    "Function '%s' passed incorrect type for arguments. " \
    "Got %s, Expected %s.", \
    ltype_name(arg->cell[index]->type), ltype_name(expect_type))

lval *lval_call(lenv *e, lval *fun, lval *arg) {
  if (fun->builtin) {
    return fun->builtin(e, arg);
  }

  int given_count = arg->count;
  int total_count = fun->formals->count;

  while (arg->count) {
    if (fun->formals->count == 0) {
      lval_del(arg);
      return lval_err("Function passed too many arguments. "
                      "Got %i, Expect %i.",
                      given_count, total_count);
    }

    lval *sym = lval_pop(fun->formals, 0);

    if (strcmp(sym->sym, "&") == 0) {
      if (fun->formals-> count != 1) {
        lval_del(arg);
        return lval_err("Function format invalid."
                        "Symbol '&' not followed by single symbol.");
      }

      lval *next_sym = lval_pop(fun->formals, 0);
      lenv_put(fun->env, next_sym, builtin_list(e, arg));
      lval_del(next_sym);
      break;
    }
    
    lval *val = lval_pop(arg, 0);

    lenv_put(fun->env, sym, val);

    lval_del(sym);
    lval_del(val);
  }
  
  lval_del(arg);

  if (fun->formals->count == 0) {
    fun->env->parent = e;
    return builtin_eval(fun->env,
                        lval_add_cell(lval_sexpr(), lval_copy(fun->body)));
  } else {
    return lval_copy(fun);
  }
}

lval *lval_join(lval *x, lval *y) {
  while (y->count > 0) {
    lval_add_cell(x, lval_pop(y, 0));
  }
  lval_del(y);
  return x;
}

void lenv_add_builtin(lenv *e, char const *name, lbuiltin fun) {
  lval *k = lval_sym(name);
  lval *v = lval_fun(fun);
  lenv_put(e, k, v);
  lval_del(k);
  lval_del(v);
}

lval *lval_eval_sexpr(lenv *e, lval *v) {
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(e, v->cell[i]);
    if (v->cell[i]->type == LVAL_ERR) {
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

  lval *fun = lval_pop(v, 0);
  if (fun->type != LVAL_FUN) {
    lval *err = lval_err("S-Expression starts with incorrect type. "
                         "Got %s, Expected %s.",
                         ltype_name(fun->type), ltype_name(LVAL_FUN));
    lval_del(fun);
    lval_del(v);
    return err;
  }

  lval *result = lval_call(e, fun, v);
  lval_del(fun);

  return result;
}

lval *lval_eval(lenv *e, lval *v) {
  if (v->type == LVAL_SYM) {
    lval *x = lenv_get(e, v);
    lval_del(v);
    return x;
  }
  if (v->type == LVAL_SEXPR) {
    return lval_eval_sexpr(e, v);
  }
  return v;
}

lval *builtin_list(lenv *e, lval *arg) {
  arg->type = LVAL_QEXPR;
  return arg;
}

lval *builtin_head(lenv *e, lval *arg) {
  LASSERT(arg, arg->count <= 1,
	  "Function 'head' passed too many arguments. "
	  "Got %i, Expected %i.",
	  arg->count, 1);
  LASSERT(arg, arg->cell[0]->type == LVAL_QEXPR,
	  "Function 'head' passed incorrect type for argument 0. "
	  "Got %s, Expected %s.",
	  ltype_name(arg->cell[0]->type), ltype_name(LVAL_QEXPR));
  LASSERT(arg, arg->cell[0]->count > 0,
	  "Function 'head' passed {}");

  lval *first = lval_take(arg, 0);

  while (first->count > 1) {
    lval_pop(first, 1);
  }

  return first;
}

lval *builtin_tail(lenv *e, lval *arg) {
  LASSERT(arg, arg->count <= 1,
	  "Function 'tail' passed too many arguments. "
	  "Got %i, Expected %i.",
	  arg->count, 1);
  LASSERT(arg, arg->cell[0]->type == LVAL_QEXPR,
	  "Function 'tail' passed incorrect type for argument 0. "
	  "Got %s, Expected %s.",
	  ltype_name(arg->cell[0]->type), ltype_name(LVAL_QEXPR));
  LASSERT(arg, arg->cell[0]->count > 0,
	  "Function 'tail' passed {}");

  printf("tail: arg="); lval_print(arg); putchar('\n');
  lval* arg0 = lval_take(arg, 0);
  printf("tail: arg0="); lval_print(arg0); putchar('\n');
  lval_del(lval_pop(arg0, 0));
  printf("tail: arg="); lval_print(arg); putchar('\n');
  return arg0;
}

lval *builtin_join(lenv *e, lval *arg) {
  for (int i = 0; i < arg->count; i++) {
    LASSERT(arg, arg->cell[i]->type == LVAL_QEXPR,
	    "Function 'join' passed incorrect types");
  }
  lval *x = lval_pop(arg, 0);

  while (arg->count > 0) {
    x = lval_join(x, lval_pop(arg, 0));
  }

  lval_del(arg);

  return x;
}

lval *builtin_eval(lenv *e, lval *arg) {
  LASSERT(arg, arg->count == 1,
    "Function 'eval' passed too many arguments");

  lval *arg0 = arg->cell[0];
  LASSERT(arg, arg0->type == LVAL_QEXPR,
    "Function 'eval' passed incorrect type");

  arg0->type = LVAL_SEXPR;
  return lval_eval(e, arg0);
}

lval *builtin_var(lenv *e, lval *arg, char *func) {
  LASSERT(arg, arg->cell[0]->type == LVAL_QEXPR,
	  "Function 'def' passed incorrect type");

  lval *syms = arg->cell[0];

  for (int i = 0; i < syms->count; i++) {
    LASSERT(arg, syms->cell[i]->type == LVAL_SYM,
	    "Function 'def' cannot define non-symbol");
  }

  LASSERT(arg, syms->count == arg->count - 1,
	  "Function 'def' cannot define incorrect "
	  "number of values to symbols");

  for (int i = 0; i < syms->count; i++) {
    if (strcmp(func, "def")) {
      lenv_def(e, syms->cell[i], arg->cell[i + 1]);
    } else if (strcmp(func, "=")) {
      lenv_put(e, syms->cell[i], arg->cell[i + 1]);
    }
  }

  lval_del(arg);
  return lval_sexpr();
}

lval *builtin_def(lenv *e, lval *arg) {
  return builtin_var(e, arg, "def");
}

lval *builtin_put(lenv *e, lval *arg) {
  return builtin_var(e, arg, "=");
}

lval *builtin_op(lenv *e, lval *arg, char* op) {
  // Ensure all arguments are numbers
  for (int i = 0; i < arg->count; i++) {
    LASSERT(arg, arg->cell[i]->type == LVAL_NUM, "Cannot operate on non-number");
  }

  lval *x = lval_pop(arg, 0);

  if ((strcmp(op, "-") == 0) && (arg->count == 0)) {
    x->num = - x->num;
  }

  while (arg->count > 0) {
    lval *y = lval_pop(arg, 0);
    if (strcmp(op, "+") == 0) {
      x->num += y->num;
    }
    if (strcmp(op, "-") == 0) {
      x->num -= y->num;
    }
    if (strcmp(op, "*") == 0) {
      x->num *= y->num;
    }
    if (strcmp(op, "/") == 0) {
      if (y->num == 0) {
	lval_del(x);
	lval_del(y);
	x = lval_err("Division by zero");
	break;
      }
      x->num /= y->num;
    }
    lval_del(y);
  }

  lval_del(arg);
  return x;
}

lval *builtin_add(lenv *e, lval *arg) {
  return builtin_op(e, arg, "+");
}

lval *builtin_sub(lenv *e, lval *arg) {
  return builtin_op(e, arg, "-");
}

lval *builtin_mul(lenv *e, lval *arg) {
  return builtin_op(e, arg, "*");
}

lval *builtin_div(lenv *e, lval *arg) {
  return builtin_op(e, arg, "/");
}

lval *builtin_lambda(lenv *e, lval *arg) {
  LASSERT_NUM("\\", arg, 2);
  LASSERT_TYPE("\\", arg, 0, LVAL_QEXPR);
  LASSERT_TYPE("\\", arg, 1, LVAL_QEXPR);

  lval *syms = arg->cell[0];
  for (int i = 0; i < syms->count; i++) {
    LASSERT(arg,
            syms->cell[i]->type == LVAL_SYM,
            "Cannot define non-symbol. Got %s, Expected %s.",
            ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
  }

  lval *formals = lval_pop(arg, 0);
  lval *body = lval_pop(arg, 0);
  lval_del(arg);

  return lval_lambda(formals, body);
}

void lenv_add_builtins(lenv *e) {
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "join", builtin_join);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "def", builtin_def);
  lenv_add_builtin(e, "=", builtin_put);
  lenv_add_builtin(e, "\\", builtin_lambda);

  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);
}

int main(int argc, char *argv[]) {
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr = mpc_new("sexpr");
  mpc_parser_t* Qexpr = mpc_new("qexpr");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Program = mpc_new("program");

  mpca_lang(MPCA_LANG_DEFAULT,
	    "\
number   : /-?[0-9]+/ ; \
symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ; \
sexpr    : '(' <expr>* ')' ; \
qexpr    : '{' <expr>* '}' ; \
expr     : <number> | <symbol> | <sexpr> | <qexpr> ; \
program	 : /^/ <expr>* /$/ ; \
",
	    Number, Symbol, Sexpr, Qexpr, Expr, Program);

  puts("TLisp Version 0.01");
  puts("Press Ctrl+c to Exit\n");

  lenv *e = lenv_new();
  lenv_add_builtins(e);
  
  while (1) {
    char* input = readline("tlisp> ");
    add_history(input);

    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Program, &r)) {
      lval *result = lval_eval(e, lval_read(r.output));
      lval_println(result);
      lval_del(result);
      //      mpc_ast_print(r.output);
      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    
    free(input);
  }

  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Program);

  return 0;
}
