void foo(int a);
int new_foo(int a);

void new_call_value(int a, int b) {
    int c = a + b;
    foo(new_foo(c));
}
