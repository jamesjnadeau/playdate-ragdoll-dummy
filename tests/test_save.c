#include <string.h>
#include "test.h"
#include "save.h"

static void test_save_roundtrip(void) {
    SaveData a, b;
    char buf[1024];
    int n;
    save_defaults(&a);
    a.best_bags = 42;
    a.best_distance_m = 317;
    strcpy(a.sel[0], "neon-small");
    strcpy(a.sel[6], "gold-sledge");
    strcpy(a.sel[8], "chiptune");
    n = save_write(&a, buf, (int)sizeof(buf));
    CHECK(n > 0 && n < (int)sizeof(buf));
    CHECK(save_read(buf, n, &b));
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);
    CHECK(strstr(buf, "sel.dummy-small=neon-small\n") != NULL);
    CHECK(strstr(buf, "sel.weapon-sledge=gold-sledge\n") != NULL);
    CHECK(strstr(buf, "sel.sounds=chiptune\n") != NULL);
    CHECK(save_write(&a, buf, 10) == 0);   /* too small: nothing written */
}

static void test_save_missing_and_empty(void) {
    SaveData d, s;
    save_defaults(&d);
    CHECK_EQ_INT(d.best_bags, 0);
    CHECK_EQ_INT(d.best_distance_m, 0);
    CHECK_EQ_INT(d.sel[3][0], 0);
    memset(&s, 0x55, sizeof(s));
    CHECK(save_read("", 0, &s));
    CHECK(memcmp(&d, &s, sizeof(d)) == 0);
    CHECK(!save_read(NULL, 0, &s));
}

static void test_save_corrupt(void) {
    SaveData d, s;
    const char* junk = "best_bags=abc\nbest_distance_m=-5\nzzz\n";
    const char* longsel = "sel.stuffing=0123456789012345678901234567890123456789\n";
    const char* ver = "version=99\nbest_bags=7\nbest_distance_m=12\nsel.sounds=x\n";
    const char* neg = "best_bags=-3\nbest_distance_m=-5\n";
    save_defaults(&d);
    CHECK(save_read(junk, (int)strlen(junk), &s));
    CHECK_EQ_INT(s.best_bags, 0);
    CHECK_EQ_INT(s.best_distance_m, 0);
    CHECK(save_read(neg, (int)strlen(neg), &s));
    CHECK_EQ_INT(s.best_bags, 0);
    CHECK_EQ_INT(s.best_distance_m, 0);
    CHECK(save_read(longsel, (int)strlen(longsel), &s));
    CHECK_EQ_INT((int)strlen(s.sel[7]), 23);
    CHECK(strncmp(s.sel[7], "01234567890123456789012", 23) == 0);
    CHECK(save_read(ver, (int)strlen(ver), &s));
    CHECK_EQ_INT(s.best_bags, 7);
    CHECK_EQ_INT(s.best_distance_m, 12);
    CHECK(strcmp(s.sel[8], "x") == 0);
    /* no trailing newline, CRLF, truncated mid-line, huge number */
    CHECK(save_read("best_bags=9", 11, &s));
    CHECK_EQ_INT(s.best_bags, 9);
    CHECK(save_read("best_bags=5\r\n", 13, &s));
    CHECK_EQ_INT(s.best_bags, 5);
    CHECK(save_read("best_bags=99999999999999999999\n", 31, &s));
    CHECK_EQ_INT(s.best_bags, 0);
    CHECK(save_read("best_bags=12", 8, &s));   /* len cuts the value: "best_bag" is an unknown key */
    CHECK_EQ_INT(s.best_bags, 0);
}

void suite_save(void) {
    test_save_roundtrip();
    test_save_missing_and_empty();
    test_save_corrupt();
}
