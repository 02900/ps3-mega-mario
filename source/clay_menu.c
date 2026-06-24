/*
 * Clay layout for the level-select menu. Pure C (like the sister port's UI) so
 * the CLAY_* compound-literal macros compile under -std=gnu99 without C++ quirks.
 * CLAY_IMPLEMENTATION lives in clay_renderer.c; here we only use the macros/decls.
 */
#include <string.h>

#include "clay.h"
#include "clay_renderer.h"
#include "clay_menu.h"

/* Clay_String over a runtime C-string (CLAY_STRING is literal-only). The chars
 * are not copied, so they must outlive the clay_render() call below — they do,
 * since the caller's std::strings live for the whole frame. */
static Clay_String cstr(const char *s)
{
    Clay_String r;
    r.isStaticallyAllocated = false;
    r.length = (int32_t)strlen(s);
    r.chars = s;
    return r;
}

void clay_render_menu(const char *title, const char *const *items,
                      int item_count, int selected)
{
    const Clay_Color white = { 255, 255, 255, 255 };
    const Clay_Color dim   = { 235, 235, 235, 255 };
    const Clay_Color red   = { 229,  37,  33, 255 };   /* Mario red */
    const Clay_Color rowc  = {   0,   0,   0,  90 };   /* translucent dark row     */
    const Clay_Color rowf  = {   0,   0,   0, 160 };   /* focused row, darker      */
    const Clay_Color gold  = { 255, 209,   0, 255 };   /* focus border + ? blocks  */

    /* Layout space = the tiny3d 2D canvas; the renderer scales it to the screen. */
    Clay_SetLayoutDimensions((Clay_Dimensions){ 848, 512 });
    Clay_BeginLayout();

    CLAY(CLAY_ID("MenuRoot"), {
        .layout = {
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
            .padding = CLAY_PADDING_ALL(40),
            .childGap = 14,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER }
        }
    }) {
        CLAY_TEXT(cstr(title),
                  CLAY_TEXT_CONFIG({ .textColor = red, .fontSize = 48 }));
        CLAY_TEXT(CLAY_STRING("Select a level"),
                  CLAY_TEXT_CONFIG({ .textColor = dim, .fontSize = 16 }));

        CLAY(CLAY_ID("MenuSpacer"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(0), CLAY_SIZING_FIXED(18) } }
        }) {}

        for (int i = 0; i < item_count; i++) {
            int f = (i == selected);
            CLAY(CLAY_IDI("MenuItem", i), {
                .layout = {
                    .sizing = { CLAY_SIZING_FIXED(300), CLAY_SIZING_FIXED(46) },
                    .padding = { 20, 20, 8, 8 },
                    .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER }
                },
                .backgroundColor = f ? rowf : rowc,
                .border = { .color = gold, .width = CLAY_BORDER_OUTSIDE(f ? 3 : 0) }
            }) {
                CLAY_TEXT(cstr(items[i]),
                          CLAY_TEXT_CONFIG({ .textColor = white, .fontSize = 26 }));
            }
        }

        CLAY(CLAY_ID("MenuGrow"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } }
        }) {}

        CLAY_TEXT(CLAY_STRING("D-pad: move     Circle: select     Start: exit"),
                  CLAY_TEXT_CONFIG({ .textColor = dim, .fontSize = 14 }));
    }

    clay_render(Clay_EndLayout(0.0f));
}
