int foo(int a);

void new_call_void(int a) {
    int b = a;
    while (foo(b)) {
        b = foo(b);
    }
    foo(b);
}
