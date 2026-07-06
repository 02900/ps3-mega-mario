/*
 * Clay -> RSXGL/OpenGL render backend (branch rsxgl-backend).
 *
 * Replaces the raylib Clay renderer: Clay is layout-only, emitting
 * Clay_RenderCommand (rectangles, borders, text) that we draw with the shared
 * gl2d helpers (implemented over RSXGL in sfml_backend.cpp). This TU owns the
 * single CLAY_IMPLEMENTATION.
 *
 * Clay lays out in an 848x512 virtual canvas (clay_menu.c calls
 * Clay_SetLayoutDimensions({848,512})); we scale every command to the real
 * screen. Text uses the built-in 8x8 bitmap font (monospace), so measure and
 * render both use gl2d_text_width / gl2d_text.
 */
#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "clay_renderer.h"
#include "gl2d.h"

#include <string.h>
#include <stdlib.h>

#define LAYOUT_W 848.0f
#define LAYOUT_H 512.0f

static void slice_to_cstr(Clay_StringSlice s, char *buf, int buf_size)
{
	int n = s.length;
	if (n > buf_size - 1) n = buf_size - 1;
	if (n > 0) memcpy(buf, s.chars, n);
	buf[n > 0 ? n : 0] = '\0';
}

// Clay measures text in layout units; our bitmap font is monospace (advance ==
// fontSize), so width = length * fontSize. gl2d_text at render time scales up.
static Clay_Dimensions clay_measure_text(Clay_StringSlice text,
                                         Clay_TextElementConfig *config,
                                         void *userData)
{
	(void)userData;
	Clay_Dimensions d;
	d.width  = gl2d_text_width(text.chars, text.length, (float)config->fontSize);
	d.height = (float)config->fontSize;
	return d;
}

static void clay_error_handler(Clay_ErrorData err) { (void)err; }

void clay_backend_init(int screen_w, int screen_h)
{
	(void)screen_w; (void)screen_h;   // layout space is fixed (LAYOUT_W x LAYOUT_H)

	uint32_t mem_size = Clay_MinMemorySize();
	Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(mem_size, malloc(mem_size));
	Clay_Dimensions dims = { LAYOUT_W, LAYOUT_H };
	Clay_ErrorHandler handler = { clay_error_handler, NULL };

	Clay_Initialize(arena, dims, handler);
	Clay_SetMeasureTextFunction(clay_measure_text, NULL);
}

void clay_render(Clay_RenderCommandArray commands)
{
	const float sx = (float)gl2d_screen_w / LAYOUT_W;
	const float sy = (float)gl2d_screen_h / LAYOUT_H;

	for (int32_t i = 0; i < commands.length; i++) {
		Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&commands, i);
		Clay_BoundingBox bb = cmd->boundingBox;
		float x = bb.x * sx, y = bb.y * sy, w = bb.width * sx, h = bb.height * sy;

		switch (cmd->commandType) {
		case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
			Clay_Color c = cmd->renderData.rectangle.backgroundColor;
			gl2d_rect(x, y, w, h,
			          (unsigned char)c.r, (unsigned char)c.g, (unsigned char)c.b, (unsigned char)c.a);
			break;
		}
		case CLAY_RENDER_COMMAND_TYPE_BORDER: {
			Clay_BorderRenderData b = cmd->renderData.border;
			unsigned char R = (unsigned char)b.color.r, G = (unsigned char)b.color.g,
			              B = (unsigned char)b.color.b, A = (unsigned char)b.color.a;
			if (b.width.top)    gl2d_rect(x, y, w, b.width.top * sy, R, G, B, A);
			if (b.width.bottom) gl2d_rect(x, y + h - b.width.bottom * sy, w, b.width.bottom * sy, R, G, B, A);
			if (b.width.left)   gl2d_rect(x, y, b.width.left * sx, h, R, G, B, A);
			if (b.width.right)  gl2d_rect(x + w - b.width.right * sx, y, b.width.right * sx, h, R, G, B, A);
			break;
		}
		case CLAY_RENDER_COMMAND_TYPE_TEXT: {
			Clay_TextRenderData t = cmd->renderData.text;
			char buf[256];
			slice_to_cstr(t.stringContents, buf, sizeof buf);
			gl2d_text(buf, x, y, (float)t.fontSize * sy,
			          (unsigned char)t.textColor.r, (unsigned char)t.textColor.g,
			          (unsigned char)t.textColor.b, (unsigned char)t.textColor.a);
			break;
		}
		default:
			break;   // IMAGE / SCISSOR / CUSTOM: unused by the menu
		}
	}
}
