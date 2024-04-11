// test manually with `--disable-pattern "inverse-conditions"` for a proper test

void foo();
void bar();

#define PATTERN_NAME inverse_condition
#define PATTERN_ARGS int x, int y

PATTERN_OLD {
    if (x > y) {
        foo();
    } else {
        bar();
    }
}

PATTERN_NEW {
    if (x <= y) {
        bar();
    } else {
        foo();
    }
}