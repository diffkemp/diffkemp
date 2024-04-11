#define PATTERN_NAME new_call_void
#define PATTERN_ARGS int x

void new_foo(int x);

PATTERN_OLD {
}

PATTERN_NEW {
    new_foo(x);
}