#include "mpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <editline/readline.h>
#include <editline/history.h>

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

typedef lval *(*lbuiltin)(lenv *, lval *);

typedef struct lval {
  int type;
  long num;
  char *err;
  char *sym;
  lbuiltin fun;
  int count;
  struct lval **cell;
} lval;

enum {
  LVAL_NUM,
  LVAL_SYM,
  LVAL_FUN,
  LVAL_SEXPR,
  LVAL_QEXPR,
  LVAL_ERR,
};

typedef struct lenv {
  int count;
  char **syms;
  lval **vals;
} lenv;

lval *lval_num(long num) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = num;
  return v;
}

lval *lval_err(char const *err) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(err) + 1);
  strcpy(v->err, err);
  return v;
}

lval *lval_sym(char const *sym) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(sym) + 1);
  strcpy(v->sym, sym);
  return v;
}

lval *lval_fun(lbuiltin fun) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->fun = fun;
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

void lval_del(lval *v) {
  switch (v->type) {
  case LVAL_NUM:
  case LVAL_FUN:
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

lval *lval_add_cell(lval *v, lval *cell) {
  v->cell = realloc(v->cell, sizeof(lval) * (v->count + 1));
  v->cell[v->count] = cell;
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

void lval_print(lval const *v);

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

void lval_print(lval const *v) {
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
    printf("<function>");
    break;
  case LVAL_SEXPR:
    lval_expr_print(v, '(', ')');
    break;
  case LVAL_QEXPR:
    lval_expr_print(v, '{', '}');
    break;
  }
}

void lval_println(lval const *v) {
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
    x->fun = v->fun;
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
  return lval_err("unbound symbol");
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

#define LASSERT(args, cond, err) \
  if (!(cond)) { lval_del(args); return lval_err(err); }

lval *lval_eval(lenv *e, lval *v);

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
    lval_del(fun);
    lval_del(v);
    return lval_err("first element is not a function");
  }

  lval *result = fun->fun(e, v);
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
	  "Function 'head' passed too many arguments");
  LASSERT(arg, arg->cell[0]->type == LVAL_QEXPR,
	  "Function 'head' passed incorrect types");
  LASSERT(arg, arg->cell[0]->count > 0,
	  "Function 'head' passed {}");

  printf("head: arg="); lval_print(arg); putchar('\n');
  lval* arg0 = lval_take(arg, 0);
  printf("head: arg0="); lval_print(arg0); putchar('\n');
  lval* head = lval_take(arg0, 0);
  printf("head: head="); lval_print(head); putchar('\n');
  return head;
}

lval *builtin_tail(lenv *e, lval *arg) {
  LASSERT(arg, arg->count <= 1,
	  "Function 'tail' passed too many arguments");
  LASSERT(arg, arg->cell[0]->type == LVAL_QEXPR,
	  "Function 'tail' passed incorrect types");
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

void lenv_add_builtins(lenv *e) {
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "join", builtin_join);
  lenv_add_builtin(e, "eval", builtin_eval);

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
symbol   : \"list\" | \"head\" | \"tail\" | \"join\" | \"eval\" \
           | '+' | '-' | '*' | '/' ;					\
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
      mpc_ast_print(r.output);
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
