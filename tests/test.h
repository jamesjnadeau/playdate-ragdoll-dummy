#ifndef TEST_H
#define TEST_H

#include <math.h>
#include <stdio.h>

extern int g_failures;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_failures++; \
        } \
    } while (0)

#define CHECK_EQ_INT(a, b) \
    do { \
        long long a_ = (long long)(a), b_ = (long long)(b); \
        if (a_ != b_) { \
            printf("FAIL %s:%d: %s == %s (%lld vs %lld)\n", __FILE__, __LINE__, #a, #b, a_, b_); \
            g_failures++; \
        } \
    } while (0)

#define CHECK_NEAR(a, b, eps) \
    do { \
        float a_ = (a), b_ = (b), e_ = (eps); \
        if (!(fabsf(a_ - b_) <= e_)) { \
            printf("FAIL %s:%d: %s ~= %s (%g vs %g, eps %g)\n", __FILE__, __LINE__, #a, #b, \
                   (double)a_, (double)b_, (double)e_); \
            g_failures++; \
        } \
    } while (0)

/* Run a suite named suite_<name>; prints "<name>: ok" if it added no failures. */
#define RUN(name) \
    do { \
        int before_ = g_failures; \
        suite_##name(); \
        if (g_failures == before_) printf(#name ": ok\n"); \
        else printf(#name ": FAILED\n"); \
    } while (0)

#endif
