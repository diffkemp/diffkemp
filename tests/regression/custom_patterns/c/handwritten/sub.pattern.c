#define PATTERN_NAME sub
#define PATTERN_ARGS int x, int y, int z

PATTERN_OLD {
    int f = x - y;
    MAPPING(f);
}

PATTERN_NEW {
    int f = x - z;
    MAPPING(f);
}
