/*
 * Level-select menu — raylib renderer (branch raylib-backend, stage 2).
 *
 * The original menu rendered via Clay over Tiny3D/ttf (extern/clay-ps3), which
 * can't run in the raylib frame. The menu is just a title + a vertical list with
 * the selected item highlighted, so we draw it directly with raylib (built-in
 * font, no asset loading). Same extern-"C" signature the game calls, so
 * Scene_Menu is unchanged. Called between window.clear() and window.display().
 */
#include "clay_menu.h"
#include "raylib.h"

void clay_backend_init(int screen_w, int screen_h)
{
	(void)screen_w; (void)screen_h;   /* raylib needs no menu backend setup */
}

void clay_render_menu(const char *title, const char *const *items,
                      int item_count, int selected)
{
	int sw = GetScreenWidth();
	int sh = GetScreenHeight();

	/* sizes scale with the screen height so it reads at any resolution */
	int titleSize = sh / 9;
	int itemSize  = sh / 16;
	int step      = (itemSize * 3) / 2;

	if (title && title[0]) {
		int tw = MeasureText(title, titleSize);
		DrawText(title, (sw - tw) / 2, sh / 8, titleSize, RAYWHITE);
	}

	int startY = sh / 2 - (item_count * step) / 2;
	for (int i = 0; i < item_count; i++) {
		const char *label = items[i] ? items[i] : "";
		int iw = MeasureText(label, itemSize);
		int x = (sw - iw) / 2;
		int y = startY + i * step;

		if (i == selected) {
			DrawRectangle(x - 24, y - 8, iw + 48, itemSize + 16,
			              (Color){ 255, 255, 255, 40 });          /* highlight bar */
			DrawText(label, x, y, itemSize, (Color){ 255, 209, 64, 255 });  /* amber */
		} else {
			DrawText(label, x, y, itemSize, (Color){ 200, 208, 224, 255 }); /* dim   */
		}
	}
}
