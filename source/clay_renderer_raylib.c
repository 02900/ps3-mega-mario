/*
 * Clay -> raylib render backend (branch raylib-backend).
 *
 * Replaces extern/clay-ps3/clay_renderer.c (which draws Clay's layout output with
 * Tiny3D/ya2d/ttf) so the menu/HUD renders inside the raylib frame instead. Clay
 * is layout-only: it emits Clay_RenderCommand (rectangles, borders, text) that we
 * draw with raylib. This TU owns the single CLAY_IMPLEMENTATION.
 *
 * Clay lays out in an 848x512 virtual canvas (clay_menu.c calls
 * Clay_SetLayoutDimensions({848,512})); we scale every command to the real screen,
 * matching the original's "logical canvas stretched to the panel" behaviour.
 */
#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "clay_renderer.h"
#include "raylib.h"
#include "rlgl.h"        // rlDrawRenderBatchActive() — force-flush per text

#include <string.h>
#include <stdlib.h>

#define LAYOUT_W 848.0f
#define LAYOUT_H 512.0f

static Font g_font;

static Color to_rl(Clay_Color c)
{
	return (Color){ (unsigned char)c.r, (unsigned char)c.g,
	                (unsigned char)c.b, (unsigned char)c.a };
}

static void slice_to_cstr(Clay_StringSlice s, char *buf, int buf_size)
{
	int n = s.length;
	if (n > buf_size - 1) n = buf_size - 1;
	if (n > 0) memcpy(buf, s.chars, n);
	buf[n > 0 ? n : 0] = '\0';
}

// Clay measures text in layout (848x512) units; raylib measures in px, which is
// 1:1 with layout units at the unscaled font size. clay_render scales up on draw.
static Clay_Dimensions clay_measure_text(Clay_StringSlice text,
                                         Clay_TextElementConfig *config,
                                         void *userData)
{
	(void)userData;
	char buf[256];
	slice_to_cstr(text, buf, sizeof buf);
	Vector2 m = MeasureTextEx(g_font, buf, (float)config->fontSize, 1.0f);
	Clay_Dimensions d = { m.x, (float)config->fontSize };
	return d;
}

static void clay_error_handler(Clay_ErrorData err) { (void)err; }

void clay_backend_init(int screen_w, int screen_h)
{
	(void)screen_w; (void)screen_h;   // layout space is fixed (LAYOUT_W x LAYOUT_H)
	g_font = GetFontDefault();

	uint32_t mem_size = Clay_MinMemorySize();
	Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(mem_size, malloc(mem_size));
	Clay_Dimensions dims = { LAYOUT_W, LAYOUT_H };
	Clay_ErrorHandler handler = { clay_error_handler, NULL };

	Clay_Initialize(arena, dims, handler);
	Clay_SetMeasureTextFunction(clay_measure_text, NULL);
}

void clay_render(Clay_RenderCommandArray commands)
{
	const float sx = (float)GetScreenWidth()  / LAYOUT_W;
	const float sy = (float)GetScreenHeight() / LAYOUT_H;

	for (int32_t i = 0; i < commands.length; i++) {
		Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&commands, i);
		Clay_BoundingBox bb = cmd->boundingBox;
		float x = bb.x * sx, y = bb.y * sy, w = bb.width * sx, h = bb.height * sy;

		switch (cmd->commandType) {
		case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
			DrawRectangle((int)x, (int)y, (int)w, (int)h,
			              to_rl(cmd->renderData.rectangle.backgroundColor));
			break;

		case CLAY_RENDER_COMMAND_TYPE_BORDER: {
			Clay_BorderRenderData b = cmd->renderData.border;
			Color c = to_rl(b.color);
			if (b.width.top)
				DrawRectangle((int)x, (int)y, (int)w, (int)(b.width.top * sy), c);
			if (b.width.bottom)
				DrawRectangle((int)x, (int)(y + h - b.width.bottom * sy), (int)w,
				              (int)(b.width.bottom * sy), c);
			if (b.width.left)
				DrawRectangle((int)x, (int)y, (int)(b.width.left * sx), (int)h, c);
			if (b.width.right)
				DrawRectangle((int)(x + w - b.width.right * sx), (int)y,
				              (int)(b.width.right * sx), (int)h, c);
			break;
		}

		case CLAY_RENDER_COMMAND_TYPE_TEXT: {
			Clay_TextRenderData t = cmd->renderData.text;
			char buf[256];
			slice_to_cstr(t.stringContents, buf, sizeof buf);
			DrawTextEx(g_font, buf, (Vector2){ x, y }, (float)t.fontSize * sy,
			           1.0f * sy, to_rl(t.textColor));
			rlDrawRenderBatchActive();   // own draw call per text (avoid RSXGL big-batch text corruption)
			break;
		}

		default:
			// IMAGE / SCISSOR / CUSTOM: not used by the menu layout.
			break;
		}
	}
}
