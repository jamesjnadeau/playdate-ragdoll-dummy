#include "test.h"
#include "json.h"
#include <string.h>

#define PARSE(str, toks, n) json_parse((str), (int)strlen(str), (toks), (n))

void suite_json(void) {
    JsonTok t[64];
    char out[16];
    float f = 0.0f;
    int iv = 0;
    int n, b, c, d, e;

    {
        const char* s = "{\"a\":1,\"b\":[2,3],\"c\":{\"d\":\"x\"}}";
        n = PARSE(s, t, 64);
        CHECK(n > 0);
        CHECK_EQ_INT(t[0].type, JT_OBJ);
        CHECK_EQ_INT(t[0].size, 3);
        b = json_obj_get(s, t, n, 0, "b");
        CHECK(b >= 0);
        CHECK_EQ_INT(t[b].type, JT_ARR);
        CHECK_EQ_INT(t[b].size, 2);
        e = json_arr_at(t, n, b, 1);
        CHECK(e >= 0);
        CHECK(json_num_i(s, t, e, &iv));
        CHECK_EQ_INT(iv, 3);
        CHECK_EQ_INT(json_arr_at(t, n, b, 2), -1);
        c = json_obj_get(s, t, n, 0, "c");
        CHECK(c >= 0);
        d = json_obj_get(s, t, n, c, "d");
        CHECK(d >= 0);
        CHECK(json_str(s, t, d, out, 16));
        CHECK(strcmp(out, "x") == 0);
        CHECK_EQ_INT(json_obj_get(s, t, n, 0, "zz"), -1);
        /* key lookup must skip nested subtrees */
        CHECK(json_obj_get(s, t, n, 0, "d") == -1);
        CHECK(json_num_i(s, t, json_obj_get(s, t, n, 0, "a"), &iv));
        CHECK_EQ_INT(iv, 1);
    }

    {
        const char* s1 = "-12.5";
        const char* s2 = "0.25";
        const char* s3 = "1e2";
        const char* s4 = "[1.5e-3, 2E+2]";
        n = PARSE(s1, t, 64);
        CHECK_EQ_INT(n, 1);
        CHECK(json_num_f(s1, t, 0, &f));
        CHECK_NEAR(f, -12.5f, 1e-5f);
        n = PARSE(s2, t, 64);
        CHECK(json_num_f(s2, t, 0, &f));
        CHECK_NEAR(f, 0.25f, 1e-5f);
        n = PARSE(s3, t, 64);
        CHECK(json_num_f(s3, t, 0, &f));
        CHECK_NEAR(f, 100.0f, 1e-5f);
        n = PARSE(s4, t, 64);
        CHECK_EQ_INT(n, 3);
        CHECK(json_num_f(s4, t, 1, &f));
        CHECK_NEAR(f, 0.0015f, 1e-6f);
        CHECK(json_num_f(s4, t, 2, &f));
        CHECK_NEAR(f, 200.0f, 1e-3f);
    }

    CHECK_EQ_INT(PARSE("{\"a\":}", t, 64), -1);
    CHECK_EQ_INT(PARSE("{\"a\":1", t, 64), -1);
    CHECK_EQ_INT(PARSE("[1,2,,]", t, 64), -1);
    CHECK_EQ_INT(PARSE("[1,2,]", t, 64), -1);
    CHECK_EQ_INT(PARSE("\"unterminated", t, 64), -1);
    CHECK_EQ_INT(PARSE("", t, 64), -1);
    CHECK_EQ_INT(PARSE("{} x", t, 64), -1);
    CHECK_EQ_INT(PARSE("{\"a\" 1}", t, 64), -1);
    CHECK_EQ_INT(PARSE("{1:2}", t, 64), -1);
    CHECK_EQ_INT(PARSE("tru", t, 64), -1);
    CHECK_EQ_INT(PARSE("-", t, 64), -1);
    CHECK_EQ_INT(PARSE("[1}", t, 64), -1);
    CHECK_EQ_INT(PARSE("{\"a\":1}}", t, 64), -1);
    CHECK_EQ_INT(json_parse(NULL, 0, t, 64), -1);
    CHECK(PARSE("[true,false,null]", t, 64) == 4);
    CHECK_EQ_INT(t[1].type, JT_TRUE);
    CHECK_EQ_INT(t[2].type, JT_FALSE);
    CHECK_EQ_INT(t[3].type, JT_NULL);
    CHECK_EQ_INT(t[3].parent, 0);

    {
        char deep[200];
        int i;
        for (i = 0; i < 64; i++) deep[i] = '[';
        for (i = 0; i < 64; i++) deep[64 + i] = ']';
        deep[128] = 0;
        CHECK_EQ_INT(PARSE(deep, t, 64), -1);
        memset(deep, '[', 32);
        memset(deep + 32, ']', 32);
        deep[64] = 0;
        CHECK_EQ_INT(PARSE(deep, t, 64), 32);
        memset(deep, '[', 33);
        memset(deep + 33, ']', 33);
        deep[66] = 0;
        CHECK_EQ_INT(PARSE(deep, t, 64), -1); /* 33 deep */
    }
    {
        char deep[256];
        int i, p = 0;
        for (i = 0; i < 33; i++) {
            memcpy(deep + p, "{\"a\":", 5);
            p += 5;
        }
        deep[p++] = '1';
        for (i = 0; i < 33; i++) deep[p++] = '}';
        deep[p] = 0;
        CHECK_EQ_INT(PARSE(deep, t, 64), -1); /* 33 nested objects */
        p = 0;
        for (i = 0; i < 32; i++) {
            memcpy(deep + p, "{\"a\":", 5);
            p += 5;
        }
        deep[p++] = '1';
        for (i = 0; i < 32; i++) deep[p++] = '}';
        deep[p] = 0;
        CHECK_EQ_INT(PARSE(deep, t, 70), 65); /* 32 nested objects ok */
    }

    {
        const char* nums[] = {"2147483647", "2147483648", "-2147483648", "-2147483649",
                              "99999999999", "9999999999"};
        const int ok[] = {1, 0, 1, 0, 0, 0};
        int k;
        for (k = 0; k < 6; k++) {
            n = PARSE(nums[k], t, 4);
            CHECK_EQ_INT(n, 1);
            CHECK_EQ_INT(json_num_i(nums[k], t, 0, &iv), ok[k]);
            if (k == 0) CHECK_EQ_INT(iv, 2147483647);
            if (k == 2) CHECK_EQ_INT(iv, -2147483647 - 1);
        }
    }

    {
        const char* nums[] = {"9e38", "123456789e38", "-9e38",
                              "123456789012345678901234567890123456789012", "3e38"};
        const int ok[] = {0, 0, 0, 0, 1};
        int k;
        for (k = 0; k < 5; k++) {
            n = PARSE(nums[k], t, 4);
            CHECK_EQ_INT(n, 1);
            CHECK_EQ_INT(json_num_f(nums[k], t, 0, &f), ok[k]);
            if (ok[k]) CHECK(f > 2.9e38f && f < 3.1e38f);
        }
    }

    {
        /* Every proper prefix of a pack.json-like document must be rejected. */
        const char* doc =
            "{\"spec\":1,\"name\":\"Pa\\\"ck\\n\",\"parts\":[{\"id\":\"head\",\"w\":-1.5e1,"
            "\"ok\":true,\"x\":null},{\"id\":\"arm\",\"w\":0.25,\"tags\":[1,2,[3]]}],"
            "\"meta\":{\"a\":{\"b\":\"\\u0041\"},\"c\":false}}";
        int dl = (int)strlen(doc), k;
        int bad = 0;
        CHECK(json_parse(doc, dl, t, 64) > 0);
        for (k = 0; k < dl; k++) {
            int r = json_parse(doc, k, t, 64);
            if (r != -1 && r != -2) bad++;
        }
        CHECK_EQ_INT(bad, 0);
    }

    {
        JsonTok big[301];
        char doc[1024];
        int i, p = 0;
        doc[p++] = '[';
        for (i = 0; i < 299; i++) {
            doc[p++] = '1';
            doc[p++] = ',';
        }
        doc[p++] = '1';
        doc[p++] = ']';
        doc[p] = 0;
        CHECK_EQ_INT(json_parse(doc, p, big, 256), -2);
        CHECK_EQ_INT(json_parse(doc, p, big, 301), 301);
    }

    {
        const char* s = "\"hello\"";
        n = PARSE(s, t, 64);
        CHECK_EQ_INT(n, 1);
        CHECK(json_str(s, t, 0, out, 4));
        CHECK(strcmp(out, "hel") == 0);
        CHECK(json_str(s, t, 0, out, 1));
        CHECK_EQ_INT(out[0], 0);
        CHECK(!json_str(s, t, 0, out, 0));
    }

    {
        const char* s = "\"a\\\"b\\\\c\\nd\"";
        n = PARSE(s, t, 64);
        CHECK_EQ_INT(n, 1);
        CHECK(json_str(s, t, 0, out, 16));
        CHECK(strcmp(out, "a\"b\\c\nd") == 0);
    }

    {
        const char* s = "[1, 2.5]";
        n = PARSE(s, t, 64);
        CHECK(!json_num_f(s, t, 0, &f)); /* token 0 is the array */
        CHECK(!json_str(s, t, 1, out, 16));
        CHECK(json_num_i(s, t, 2, &iv) == false);
    }
}
