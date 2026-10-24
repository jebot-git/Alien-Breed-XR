/* Regression: HUD fill stays visible and within its track when downscaled. */
#include "../src/display.c"
#include <assert.h>

int main(void)
{
    const int heights[] = { 4, 40, 64, 100, 120, 180, 240, 360, 540, 1080 };
    const int maxima[] = { PLAYER_MAX_ENERGY, 63 };
    for (int kind = 0; kind < 2; kind++) {
        for (int i = 0; i < (int)(sizeof(heights) / sizeof(heights[0])); i++) {
            SDL_Rect track = { 7, 13, 4, heights[i] };
            int previous = 0;
            for (int value = 0; value <= maxima[kind]; value++) {
                SDL_Rect fill = display_hud_bar_fill_rect(track, value, maxima[kind]);
                assert(fill.x == track.x && fill.w == track.w);
                assert(fill.y + fill.h == track.y + track.h);
                assert(fill.y >= track.y && fill.h <= track.h);
                assert(fill.h >= previous);
                assert(value ? fill.h >= 1 : fill.h == 0);
                if (value == maxima[kind]) assert(fill.h == track.h);
                previous = fill.h;
            }
            assert(display_hud_bar_fill_rect(track, -1, maxima[kind]).h == 0);
            assert(display_hud_bar_fill_rect(track, maxima[kind] + 1, maxima[kind]).h == track.h);
        }
    }
    puts("PASS: low-resolution health/ammo fills remain visible, bottom-aligned and monotonic; empty/full and bounds correct");
    return 0;
}
