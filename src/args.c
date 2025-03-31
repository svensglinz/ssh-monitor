#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include "args.h"


int arg_to_int(char *arg) {
  char *end;
  long out = strtol(arg, &end, 10);
  if (*end != '\0') {
    printf("Invalid argument %s\n", arg);
    exit(EXIT_FAILURE);
  }
  return out;
}

struct args *parse_args(int argc, char **argv) {
  int c;
  struct args *args = calloc(sizeof(struct args), 1);

    while ((c = getopt(argc, argv, "n:t:"))) {
      switch (c) {
        case 'n': args->n = arg_to_int(optarg); break;
        case 't': args->t = arg_to_int(optarg); break;
      }
    }
    return args;
}