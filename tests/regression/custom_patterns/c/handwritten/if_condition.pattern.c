// SimpLL library is bugged, doesn't work at the moment

int foo(int x);

#define PATTERN_NAME if_condition
#define PATTERN_ARGS int x, int y, int z

PATTERN_OLD {
    if ((x && foo(y)) && z) {
        foo(x);
    }
}

PATTERN_NEW {
    if ((x || foo(y)) && z) {
        foo(x);
    }
}
