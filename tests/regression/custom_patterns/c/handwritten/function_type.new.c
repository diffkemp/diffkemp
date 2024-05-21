unsigned foo(unsigned long a);

unsigned function_type(unsigned a) {
    unsigned b = a + 1;
    return foo(b);
}
