extern int global;

int foo();
int bar(int x, int y);

int global_into_param_call(int x, int y) {
    global = bar(x, y);
    return foo();
}
