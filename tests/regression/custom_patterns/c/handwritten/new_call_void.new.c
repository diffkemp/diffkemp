int foo(int a);
void new_foo(int a);

void new_call_void(int a) {
    int b = a;
    new_foo(b);
    while (foo(b)) {
        b = foo(b);
        new_foo(b);
    }
    b = foo(b);
}
