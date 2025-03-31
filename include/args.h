#ifndef ARGS_H
#define ARGS_H

struct args {
    int n;
    int t;
};

struct args *parse_args(int argc, char **argv);

#endif //ARGS_H
