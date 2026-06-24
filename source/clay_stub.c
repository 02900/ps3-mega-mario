/*
 * Clay menu — stub (branch raylib-backend, stage 1).
 *
 * The real Clay menu renders via extern/clay-ps3/clay_renderer.c, which draws with
 * Tiny3D/ttf and so can't run in the raylib frame. Stage 1 stubs the two hooks the
 * game calls (so it builds and the gameplay renders); stage 2 replaces these with a
 * raylib Clay renderer that brings the menu/HUD back.
 */
#include "clay_menu.h"

void clay_backend_init(int screen_w, int screen_h)
{
	(void)screen_w; (void)screen_h;
}

void clay_render_menu(const char *title, const char *const *items,
                      int item_count, int selected)
{
	(void)title; (void)items; (void)item_count; (void)selected;
}
