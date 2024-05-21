int foo(int *a, int *b, unsigned long flags);

int value(int a, int b) {
    foo(&a, &b, (0b110UL << 8));
    return a + b;
}
