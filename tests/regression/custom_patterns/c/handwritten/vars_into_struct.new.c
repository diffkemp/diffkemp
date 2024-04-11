struct AB {
    int a;
    int b;
};

void foo(struct AB ab);
int bar(int seed);

void vars_into_struct(int seed) {
    int a = bar(seed);
    int b = bar(seed);
    if (a > b) {
        struct AB ab = {.a = a, .b = b};
        foo(ab);
    } else {
        struct AB ba = {.a = b, .b = a};
        foo(ba);
    }
}

