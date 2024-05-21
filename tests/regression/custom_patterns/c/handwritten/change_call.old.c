void foo_old(int a);
int bar();

int change_call(int a, int b) {
    if (a > b) {
        foo_old(a + b);
    } else {
        foo_old(a - b);
    }
    if (bar()) {
        return 1;
    }
    return 0;
}
