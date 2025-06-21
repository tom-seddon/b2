#include <shared/system.h>
#include <shared/debug.h>
#include <stdio.h>
#include <stdlib.h>
#include <shared/system_specific.h>

static void f9(void) {
    void *buffer[100];
    int n = backtrace(buffer, sizeof buffer / sizeof buffer[0]);

    char **symbols = GetBacktraceSymbols(buffer, n);
    if (symbols) {
        for (int i = 0; i < n; ++i) {
            printf("%d. %s\n", i, symbols[i]);
        }

        free(symbols);
        symbols = NULL;
    }
}
static void f8(void) {
    f9();
}
static void f7(void) {
    f8();
}
static void f6(void) {
    f7();
}
static void f5(void) {
    f6();
}
static void f4(void) {
    f5();
}
static void f3(void) {
    f4();
}
static void f2(void) {
    f3();
}
static void f1(void) {
    f2();
}
static void f0(void) {
    f1();
}

int main(void) {
    f0();
}
