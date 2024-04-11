int foo(int a);
void bar_then();
void bar_else();

void condition_only(int a, int b) {
    int c = a + b;
    int d = foo(c);
    if (a > d) {
        bar_then();
    } else {
        bar_else();
    }
}
