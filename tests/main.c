#include "test.h"

int g_failures = 0;

void suite_smoke(void);
void suite_rmath(void);
void suite_rng(void);
void suite_json(void);

int main(void) {
    RUN(smoke);
    RUN(rmath);
    RUN(rng);
    RUN(json);
    if (g_failures > 0) {
        printf("%d check(s) failed\n", g_failures);
        return 1;
    }
    return 0;
}
