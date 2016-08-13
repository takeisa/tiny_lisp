#include <stdio.h>

#define MAX_BUF_SIZE 2048

static char input[MAX_BUF_SIZE];

int main(int argc, char *argv[]) {
  puts("TLisp Version 0.01");
  puts("Press Ctrl+c to Exit\n");

  while (1) {
    fputs("tlisp> ", stdout);
    fgets(input, MAX_BUF_SIZE, stdin);
    printf("No you're a %s", input);
  }

  return 0;
}
