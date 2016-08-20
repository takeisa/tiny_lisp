#include "mpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <editline/readline.h>
#include <editline/history.h>

int number_of_nodes(mpc_ast_t* t) {
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

long eval_op(char* op, long x, long y) {
  printf("eval_op: op=%s x=%li y=%li\n", op, x, y);
  if (strcmp(op, "+") == 0) { return x + y; };
  if (strcmp(op, "-") == 0) { return x - y; };
  if (strcmp(op, "*") == 0) { return x * y; };
  if (strcmp(op, "/") == 0) { return x / y; };
  return 0;
}

long eval(mpc_ast_t* t) {
  if (strstr(t->tag, "number")) {
    return atol(t->contents);
  }

  char* op = t->children[1]->contents;

  long x = eval(t->children[2]);

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
      long result = eval(r.output);
      printf("%li\n", result);
      printf("number_of_nodes: %d\n", number_of_nodes(r.output));
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
