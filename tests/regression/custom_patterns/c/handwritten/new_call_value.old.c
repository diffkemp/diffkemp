void foo(int a);

void new_call_value(int a, int b) {
    int c = a + b;
    foo(2*c);
}
