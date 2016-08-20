#include "mpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <editline/readline.h>
#include <editline/history.h>

typedef struct {
  int type;
  long num;
  int err;
} lval;

enum {
  LVAL_NUM,
  LVAL_ERR };

enum {
  LERR_DIV_ZERO,
  LERR_BAD_OP,
  LERR_BAD_NUM };

char *ERR_MESSAGES[] = {
  "Division by zero",
  "Invalid operator",
  "Invalid number"
};

lval lval_num(long num) {
  return (lval) {.type = LVAL_NUM, .num = num};
}

lval lval_err(int err) {
  return (lval) {.type = LVAL_ERR, .err = err};
}

void lval_print(lval const *v) {
  switch (v->type) {
  case LVAL_NUM:
    printf("%li", v->num);
    break;
  case LVAL_ERR:
    printf("Error: %s", ERR_MESSAGES[v->err]);
    break;
  }
}

void lval_println(lval const *v) {
  lval_print(v);
  putchar('\n');
}

int number_of_nodes(mpc_ast_t *t) {
  if (t->children_num == 0) {
    return 1;
  } else {
    int total = 1;
    for (int i = 0; i < t->children_num; i++) {
      total += number_of_nodes(t->children[i]);
    }
    return total;
  }
}

lval eval_op(char *op, lval x, lval y) {
  printf("eval_op: op=%s", op);
  printf(" x="); lval_print(&x);
  printf(" y="); lval_println(&y);

  if (x.type == LVAL_ERR) { return x; }
  if (y.type == LVAL_ERR) { return y; }

  if (strcmp(op, "+") == 0) { return lval_num(x.num + y.num); }
  if (strcmp(op, "-") == 0) { return lval_num(x.num - y.num); }
  if (strcmp(op, "*") == 0) { return lval_num(x.num * y.num); }
  if (strcmp(op, "/") == 0) {
    return y.num == 0
      ? lval_err(LERR_DIV_ZERO)
      : lval_num(x.num / y.num);
  }

  return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t *t) {
  if (strstr(t->tag, "number")) {
    long x = strtol(t->contents, NULL, 10);
    return errno == ERANGE
      ? lval_err(LERR_BAD_NUM)
      : lval_num(x);
  }

  char* op = t->children[1]->contents;

  lval x = eval(t->children[2]);

  if ((strcmp(op, "-") == 0)
      && (t->children_num == 4)) {
    return lval_num(-x.num);
  }
  
  int i = 3;
  while (strstr(t->children[i]->tag, "expr")) {
    x = eval_op(op, x, eval(t->children[i]));
    i++;
  }
  
  return x;
}

int main(int argc, char *argv[]) {
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Operator = mpc_new("operator");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Program = mpc_new("program");

  mpca_lang(MPCA_LANG_DEFAULT,
	    "\
number   : /-?[0-9]+/ ; \
operator : '+' | '-' | '*' | '/' ; \
expr     : <number> | '(' <operator> <expr>+ ')' ; \
program	 : /^/ <operator> <expr>+ /$/ ; \
",
	    Number, Operator, Expr, Program);

  puts("TLisp Version 0.01");
  puts("Press Ctrl+c to Exit\n");

  while (1) {
    char* input = readline("tlisp> ");
    add_history(input);

    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Program, &r)) {
      lval result = eval(r.output);
      lval_println(&result);
      mpc_ast_print(r.output);
      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    
    free(input);
  }

  mpc_cleanup(4, Number, Operator, Expr, Program);

  return 0;
}
