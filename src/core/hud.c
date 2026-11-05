#include "hud.h"
#include "rig.h"

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
