#include <stdio.h>
#include <stdbool.h>

int int_pow(int x, int y) {
    int z = 1;
    for (int i = 0; i < y; i++) {
        z *= x;
    }
    return z;
}

int left(int x, int y) {
    return (5 * x + 4 * y) * (5 * x + 4 * y) * (5 * x + 4 * y);
}

int right(int x, int y) {
    return int_pow(((x << 2) + x) + (y << 2), 3);
}

bool is_prepared(int i) {
    return i == 3;
}

void perform_action() {
    return;
}

void flag_example_original() {
    for (int i = 0; i < 5; i++) {
        if (is_prepared(i)) {
            perform_action();
            break;
        }
	}
}

void flag_example_modified() {
    bool flag = false;
    for (int i = 0; i < 5; i++) {
        if (is_prepared(i)) {
            flag = true;
            break;
        }
	}

    if (flag) {
        perform_action();
    }
}

int main() {
    printf("SAME: %d\n", left(15, 3) == right(15, 3));
    printf("SAME: %d\n", left(10, 2) == right(10, 2));
    printf("SAME: %d\n", left(24, 13) == right(24, 13));
    printf("SAME: %d\n", left(4, 20) == right(4, 20));
    printf("SAME: %d\n", left(6, 9) == right(6, 9));
    printf("SAME: %d\n", left(50, 4) == right(50, 4));
    printf("SAME: %d\n", left(11, 11) == right(11, 11));
    return 0;
}
