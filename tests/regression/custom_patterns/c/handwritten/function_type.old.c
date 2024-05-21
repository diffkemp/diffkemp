unsigned foo(unsigned a);

unsigned function_type(unsigned a) {
    unsigned b = a + 1;
    return foo(b);
}
