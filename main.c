#include "mpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <editline/readline.h>
#include <editline/history.h>

int main(int argc, char *argv[]) {
  puts("TLisp Version 0.01");
  puts("Press Ctrl+c to Exit\n");

  while (1) {
    char* input = readline("tlisp> ");
    add_history(input);
    printf("No you're a %s\n", input);
    free(input);
  }

  return 0;
}
