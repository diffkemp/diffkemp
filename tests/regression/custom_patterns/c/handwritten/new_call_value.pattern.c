// It is not possible to define a pattern with simply x in the old version,
// instead of the current 2*x, because of how SimpLL variable mapping works at
// the moment.

#define PATTERN_NAME new_call_value
#define PATTERN_ARGS int x

int new_foo(int x);

PATTERN_OLD { MAPPING(2 * x); }

PATTERN_NEW { MAPPING(new_foo(x)); }
