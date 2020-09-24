#include <stdio.h>

const int orderCost = 150;
int orderCount = 5;
int balance = 1000;

void finish() {
    printf("Order Count: %d\nBalance: %d\n", orderCount, balance);

    orderCount = 5;
    balance = 1000;
}

void old_slide_statements() {
    int newBalance = balance; // no-basic-block start
    orderCount = 0;
    newBalance -= orderCount * orderCost;
    balance = newBalance; // no-basic-block end
}

void new_slide_statements() {
    int newBalance = balance; // no-basic-block start
    newBalance -= orderCount * orderCost;
    orderCount = 0;
    balance = newBalance; // no-basic-block end
}

int main() {
    old_slide_statements();
    finish();
    new_slide_statements();
    finish();
    
    return 0;
}
