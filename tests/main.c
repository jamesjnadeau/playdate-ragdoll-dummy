#include "test.h"

int g_failures = 0;

void suite_smoke(void);

int main(void) {
    RUN(smoke);
    if (g_failures > 0) {
        printf("%d check(s) failed\n", g_failures);
        return 1;
    }
    return 0;
}
