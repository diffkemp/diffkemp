int foo(int x);

#define PATTERN_NAME condition_only
#define PATTERN_ARGS int x, int y

CONDITION_PATTERN_OLD {
    int z = foo(y);
    return x > z;
}

CONDITION_PATTERN_NEW {
    int z = foo(y);
    return x < z;
}
