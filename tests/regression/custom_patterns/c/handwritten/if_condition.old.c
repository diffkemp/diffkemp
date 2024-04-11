int foo(int a);

extern int res;

int if_condition(int a, int b) {
    int c = a + b;
    if ((c && foo(a)) && b) {
        foo(c);
    }
    return res;
}
