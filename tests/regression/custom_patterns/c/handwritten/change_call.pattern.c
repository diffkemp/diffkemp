void foo_old(int x);
void foo_new(int x);

#define PATTERN_NAME change_call
#define PATTERN_ARGS int x

PATTERN_OLD {
    foo_old(x);
}

PATTERN_NEW {
    foo_new(x);
}

