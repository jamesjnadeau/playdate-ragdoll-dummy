#include "hud.h"
#include <string.h>
#include "rig.h"
#include "game.h"
#include "tuning.h"

#define PANEL_X 20.0f
#define PANEL_Y 24.0f
#define PANEL_W 360.0f
#define PANEL_H 192.0f
#define COL_W 160.0f
#define ICON_SCALE 2.0f
#define ICON_PX 32.0f
#define BAR_W 12.0f
#define BAR_H 10.0f
#define BAR_GAP 3.0f
#define BAR_MAX 6

static void bars(RenderList* rl, float x, float y, int n) {
    int i;
    for (i = 0; i < BAR_MAX; i++) {
        float bx = x + (float)i * (BAR_W + BAR_GAP);
        if (i < n) rl_rect(rl, bx, y, BAR_W, BAR_H, PAT_BLACK, false);
        else rl_rect(rl, bx, y, BAR_W, BAR_H, PAT_WHITE, true);
    }
}

static void column(RenderList* rl, float x, const char* heading, WeaponId id) {
    const WeaponDef* w = weapon_def(id);
    float ix = x + (COL_W - ICON_PX * ICON_SCALE) * 0.5f;
    if (!w) return;
    rl_text(rl, heading, x, PANEL_Y + 40.0f);
    rl_bitmap(rl, IMG_WEAPON_ICON0 + (int)id, ix, PANEL_Y + 62.0f, 0.0f, 0.0f, 0.0f, ICON_SCALE, ICON_SCALE, false);
    rl_text(rl, w->name, x, PANEL_Y + 132.0f);
    rl_text(rl, "Speed", x, PANEL_Y + 152.0f);
    bars(rl, x + 56.0f, PANEL_Y + 154.0f, w->speed_bars);
    rl_text(rl, "Power", x, PANEL_Y + 168.0f);
    bars(rl, x + 56.0f, PANEL_Y + 170.0f, w->power_bars);
}

void hud_draw_offer(RenderList* rl, WeaponId current, WeaponId offer) {
    rl_rect(rl, PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PAT_WHITE, true);
    rl_text(rl, "A new weapon!", PANEL_X + 124.0f, PANEL_Y + 10.0f);
    column(rl, PANEL_X + 16.0f, "Keep (B)", current);
    rl_line(rl, PANEL_X + PANEL_W * 0.5f, PANEL_Y + 36.0f, PANEL_X + PANEL_W * 0.5f, PANEL_Y + PANEL_H - 12.0f, 1, PAT_BLACK);
    column(rl, PANEL_X + PANEL_W * 0.5f + 16.0f, "Take (A)", offer);
}

/* ---- In-play HUD (mockup A) ---- */

#define HUD_MARGIN 4.0f
#define HUD_PLATE_H 24.0f
#define HUD_FRAME 2.0f
#define HEART_SLOTS 5
#define HEART_STEP 16.0f
#define HEART_W 14.0f
#define HEART_H 12.0f
#define BAG_PX 16.0f
#define DIST_PLATE_W 72.0f
#define DIST_CHAR_W 5.0f      /* half of an average glyph advance, to centre the text */
#define BAGS_PLATE_W 76.0f
#define WPN_PLATE_W 136.0f
#define WPN_PLATE_H 62.0f
#define WPN_LINE 17.0f
#define SQ 8.0f
#define SQ_STEP 10.0f
#define SWING_CX 200.0f
#define SWING_CY 212.0f
#define SWING_R 15.0f
#define SWING_DOT_DX 7.0f
#define SWING_DOT 3.0f
#define SWING_ARROW_GAP 6.0f
#define SWING_ARROW_LEN 12.0f
#define SWING_ARROW_HALF 8.0f

/* Writes the decimal digits of v (negative values print as 0) and returns the length. */
static int int_to_str(int v, char* out) {
    char tmp[12];
    int n = 0, i = 0;
    if (v < 0) v = 0;
    do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v > 0);
    while (n > 0) out[i++] = tmp[--n];
    out[i] = '\0';
    return i;
}

/* A white plate with a 2 px black frame. */
static void plate(RenderList* rl, float x, float y, float w, float h) {
    rl_rect(rl, x, y, w, h, PAT_BLACK, false);
    rl_rect(rl, x + HUD_FRAME, y + HUD_FRAME, w - 2.0f * HUD_FRAME, h - 2.0f * HUD_FRAME, PAT_WHITE, false);
}

/* "<prefix><n><suffix>" into out (at least 16 bytes). */
static void label(char* out, const char* prefix, int n, const char* suffix) {
    size_t p = strlen(prefix);
    memcpy(out, prefix, p);
    int_to_str(n, out + p);
    strcpy(out + p + strlen(out + p), suffix);
}

/* BAR_MAX squares; the first n are black, the rest a light dither (drawRect outlines cost far more on device). */
static void squares(RenderList* rl, float x, float y, int n) {
    int i;
    for (i = 0; i < BAR_MAX; i++)
        rl_rect(rl, x + (float)i * SQ_STEP, y, SQ, SQ, i < n ? PAT_BLACK : PAT_D25, false);
}

/* A round-looking disc from two overlapping rectangles (corners cut): a fillRect is far cheaper on device than a polygon. */
static void disc(RenderList* rl, float cx, float cy, float r) {
    float s = r * 0.3f;   /* how far the corners are cut */
    rl_rect(rl, cx - r, cy - r + s, 2.0f * r, 2.0f * (r - s), PAT_BLACK, false);
    rl_rect(rl, cx - r + s, cy - r, 2.0f * (r - s), 2.0f * r, PAT_BLACK, false);
}

void hud_draw_play(RenderList* rl, const struct Game* g) {
    char buf[16];
    int i, hearts = g->player.hearts;
    float x, y;
    const WeaponDef* w = weapon_def(g->weapon);

    /* Hearts, top left. */
    plate(rl, HUD_MARGIN, HUD_MARGIN, 6.0f + HEART_STEP * (float)HEART_SLOTS, HUD_PLATE_H);
    for (i = 0; i < HEART_SLOTS; i++)
        rl_bitmap(rl, i < hearts ? IMG_UI_HEART_FULL : IMG_UI_HEART_EMPTY,
                  HUD_MARGIN + 6.0f + HEART_STEP * (float)i, HUD_MARGIN + (HUD_PLATE_H - HEART_H) * 0.5f,
                  0.0f, 0.0f, 0.0f, 1.0f, 1.0f, false);

    /* Distance, top centre. */
    label(buf, "", (int)g->player.z, "m");
    x = (SCREEN_W - DIST_PLATE_W) * 0.5f;
    plate(rl, x, HUD_MARGIN, DIST_PLATE_W, HUD_PLATE_H);
    rl_text(rl, buf, SCREEN_W * 0.5f - DIST_CHAR_W * (float)strlen(buf), HUD_MARGIN + 4.0f);

    /* Bags, top right. */
    x = SCREEN_W - HUD_MARGIN - BAGS_PLATE_W;
    plate(rl, x, HUD_MARGIN, BAGS_PLATE_W, HUD_PLATE_H);
    rl_bitmap(rl, IMG_UI_BAG, x + 8.0f, HUD_MARGIN + (HUD_PLATE_H - BAG_PX) * 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, false);
    label(buf, "x", g->bags, "");
    rl_text(rl, buf, x + 8.0f + BAG_PX + 6.0f, HUD_MARGIN + 4.0f);

    /* Weapon, bottom left. */
    y = SCREEN_H - HUD_MARGIN - WPN_PLATE_H;
    plate(rl, HUD_MARGIN, y, WPN_PLATE_W, WPN_PLATE_H);
    if (w) {
        rl_text(rl, w->name, HUD_MARGIN + 8.0f, y + 3.0f);
        rl_text(rl, "SPD", HUD_MARGIN + 8.0f, y + 3.0f + WPN_LINE);
        squares(rl, HUD_MARGIN + 44.0f, y + 3.0f + WPN_LINE + 5.0f, w->speed_bars);
        rl_text(rl, "PWR", HUD_MARGIN + 8.0f, y + 3.0f + 2.0f * WPN_LINE);
        squares(rl, HUD_MARGIN + 44.0f, y + 3.0f + 2.0f * WPN_LINE + 5.0f, w->power_bars);
    }

    /* Swing side, bottom centre: a black disc with three white dots, and an arrow on the held side. */
    disc(rl, SWING_CX, SWING_CY, SWING_R);
    for (i = -1; i <= 1; i++)
        rl_rect(rl, SWING_CX + SWING_DOT_DX * (float)i - SWING_DOT * 0.5f, SWING_CY - SWING_DOT * 0.5f,
                SWING_DOT, SWING_DOT, PAT_WHITE, false);
    {
        int dir = (g->held & BTN_LEFT) ? -1 : (g->held & BTN_RIGHT) ? 1 : 0;
        if (dir != 0) {
            float base = SWING_CX + (float)dir * (SWING_R + SWING_ARROW_GAP);
            float tri[6];
            tri[0] = base + (float)dir * SWING_ARROW_LEN; tri[1] = SWING_CY;
            tri[2] = base; tri[3] = SWING_CY - SWING_ARROW_HALF;
            tri[4] = base; tri[5] = SWING_CY + SWING_ARROW_HALF;
            rl_poly(rl, tri, 3, PAT_BLACK);
        }
    }
}
