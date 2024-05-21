int foo(int param);
int bar(int x, int y);

int global_into_param_call(int x, int y) {
    return foo(bar(x,y));
}
