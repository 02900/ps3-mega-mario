/*
 * Clay-based level-select menu (Phase 7). The game's Scene_Menu owns the menu
 * state (title, items, selected index) but the UI is rendered natively in Clay,
 * not via hand-drawn sf::Text (docs/PATTERNS.md §3.5). This C TU builds the Clay
 * layout; Scene_Menu (C++) calls it through these extern-"C" hooks.
 */
#ifndef CLAY_MENU_H
#define CLAY_MENU_H

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the Clay backend (arena + TTF measure-text). Call once at startup,
 * after the TTF/ya2d stack is up. Defined in extern/clay-ps3/clay_renderer.c. */
void clay_backend_init(int screen_w, int screen_h);

/* Render the level-select menu (title + items, the selected one highlighted).
 * Call inside a frame, between window.clear() and window.display(). */
void clay_render_menu(const char *title, const char *const *items,
                      int item_count, int selected);

#ifdef __cplusplus
}
#endif

#endif /* CLAY_MENU_H */
