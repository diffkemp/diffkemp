#include <stdio.h>

typedef struct a { int x; double y;} A;

A f(A a) {
    return a;
}

int main() {
    A a;
    a.x = 1;
    a.y = 2.0;
    f(a);
    return 0;
}
