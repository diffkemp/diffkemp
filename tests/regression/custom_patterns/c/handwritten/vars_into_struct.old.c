void foo(int a, int b);
int bar(int seed);

void vars_into_struct(int seed) {
    int a = bar(seed);
    int b = bar(seed);
    if (a > b) {
        foo(a, b);
    } else {
        foo(b, a);
    }
}

