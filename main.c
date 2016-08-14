#include "mpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <editline/readline.h>
#include <editline/history.h>

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
