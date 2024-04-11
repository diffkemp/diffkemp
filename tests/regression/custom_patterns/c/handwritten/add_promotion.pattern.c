#define PATTERN_NAME add_promotion
#define PATTERN_ARGS int x

PATTERN_OLD {
    double y = x + 1.0;
    MAPPING(x, y);
}

PATTERN_NEW {
    double y = x + 2.0;
    MAPPING(x, y);
}
