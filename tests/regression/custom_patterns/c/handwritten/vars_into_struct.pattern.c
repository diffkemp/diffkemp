// SimpLL library is bugged, doesn't work at the moment

struct AB {
    int a;
    int b;
};

void FUNCTION_OLD(foo, int a, int b);

void FUNCTION_NEW(foo, struct AB ab);

#define PATTERN_NAME vars_into_struct
#define PATTERN_ARGS int a, int b

PATTERN_OLD {
    FUNCTION_OLD(foo, a, b);
}

PATTERN_NEW {
    struct AB ab = {.a = a, .b = b};
    FUNCTION_NEW(foo, ab);
}
