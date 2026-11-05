#include <string.h>
#include "test.h"
#include "render_list.h"

void suite_render_list(void) {
    static RenderList rl;
    float xy[8] = {0.0f, 0.0f, 10.0f, 0.0f, 10.0f, 10.0f, 0.0f, 10.0f};
    int i;

    /* A full list keeps 1024 commands and counts the 76 it dropped. */
    rl_clear(&rl);
    rl_hud_begin(&rl);
    for (i = 0; i < 1100; i++) rl_line(&rl, 0.0f, 0.0f, 1.0f, 1.0f, 2, PAT_BLACK);
    CHECK_EQ_INT(rl.n, 1024);
    CHECK_EQ_INT(rl.dropped, 76);

    /* Clearing resets count, text and drop counter. */
    rl_clear(&rl);
    CHECK_EQ_INT(rl.n, 0);
    CHECK_EQ_INT(rl.text_n, 0);
    CHECK_EQ_INT(rl.dropped, 0);

    /* Two strings are stored NUL-terminated at distinct offsets. */
    rl_text(&rl, "hello", 1.0f, 2.0f);
    rl_text(&rl, "world", 3.0f, 4.0f);
    CHECK_EQ_INT(rl.n, 2);
    CHECK_EQ_INT(rl.cmd[0].type, RC_TEXT);
    CHECK_EQ_INT(rl.cmd[1].type, RC_TEXT);
    CHECK(rl.cmd[0].text_off != rl.cmd[1].text_off);
    CHECK(strcmp(rl.text + rl.cmd[0].text_off, "hello") == 0);
    CHECK(strcmp(rl.text + rl.cmd[1].text_off, "world") == 0);

    /* Text that would overflow the 1024-byte buffer is dropped whole. */
    rl_clear(&rl);
    {
        char big[600];
        memset(big, 'a', sizeof(big) - 1);
        big[sizeof(big) - 1] = '\0';
        rl_text(&rl, big, 0.0f, 0.0f);
        rl_text(&rl, big, 0.0f, 0.0f);
    }
    CHECK_EQ_INT(rl.n, 1);
    CHECK_EQ_INT(rl.dropped, 1);
    CHECK(strlen(rl.text) == 599);

    /* Polygon and rect commands record their type and geometry. */
    rl_clear(&rl);
    rl_poly(&rl, xy, 4, PAT_D25);
    rl_rect(&rl, 5.0f, 6.0f, 7.0f, 8.0f, PAT_D50, true);
    rl_bitmap(&rl, 3, 1.0f, 2.0f, 0.0f, 0.5f, 0.5f, 1.0f, 1.0f, true);
    CHECK_EQ_INT(rl.cmd[0].type, RC_POLY);
    CHECK_EQ_INT(rl.cmd[0].npts, 4);
    CHECK_EQ_INT(rl.cmd[0].pts[2], 10);
    CHECK_EQ_INT(rl.cmd[1].type, RC_RECT);
    CHECK_EQ_INT(rl.cmd[2].type, RC_BITMAP);
    CHECK_EQ_INT(rl.cmd[2].image, 3);
    CHECK_EQ_INT(rl.cmd[2].flip, 1);

    /* The last RL_HUD_RESERVE commands are kept for the HUD until rl_hud_begin. */
    rl_clear(&rl);
    for (int i = 0; i < 1100; i++) rl_rect(&rl, 0.0f, 0.0f, 1.0f, 1.0f, PAT_BLACK, false);
    CHECK_EQ_INT(rl.n, 1024 - RL_HUD_RESERVE);
    CHECK_EQ_INT(rl.dropped, 1100 - (1024 - RL_HUD_RESERVE));
    rl.dropped = 0;
    rl_hud_begin(&rl);
    for (int i = 0; i < RL_HUD_RESERVE + 5; i++) rl_rect(&rl, 0.0f, 0.0f, 1.0f, 1.0f, PAT_BLACK, false);
    CHECK_EQ_INT(rl.n, 1024);
    CHECK_EQ_INT(rl.dropped, 5);
    rl_clear(&rl);   /* clearing closes the reserve again */
    for (int i = 0; i < 1100; i++) rl_rect(&rl, 0.0f, 0.0f, 1.0f, 1.0f, PAT_BLACK, false);
    CHECK_EQ_INT(rl.n, 1024 - RL_HUD_RESERVE);
}
