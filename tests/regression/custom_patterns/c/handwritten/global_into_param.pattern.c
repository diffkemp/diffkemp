extern int global;

int FUNCTION_OLD(foo, );
int FUNCTION_NEW(foo, int param);
int foobar(int param);

#define PATTERN_NAME global_into_param
#define PATTERN_ARGS int x

PATTERN_OLD {
    global = x;
    FUNCTION_OLD(foo, );
}

PATTERN_NEW {
    FUNCTION_NEW(foo, x);
}
