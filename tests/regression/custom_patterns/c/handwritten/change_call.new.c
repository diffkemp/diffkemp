void foo_new(int a);
int bar();

int change_call(int a, int b) {
    if (a > b) {
        foo_new(a + b);
    } else {
        foo_new(a - b);
    }
    if (bar()) {
        return 1;
    }
    return 0;
}
