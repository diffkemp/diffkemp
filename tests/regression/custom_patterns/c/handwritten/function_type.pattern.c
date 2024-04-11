unsigned FUNCTION_OLD(foo, unsigned x);
unsigned FUNCTION_NEW(foo, unsigned long x);

#define PATTERN_NAME function_type
#define PATTERN_ARGS unsigned x

PATTERN_OLD {
    unsigned z = FUNCTION_OLD(foo, x);
    MAPPING(z);
}

PATTERN_NEW {
    unsigned z = FUNCTION_NEW(foo, x);
    MAPPING(z);
}
