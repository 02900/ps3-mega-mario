/*
 * PS3 Mega Mario - entry point
 *
 * PS3 homebrew port of "mega-mario" (a C++/SFML ECS platformer). The original is
 * C++, so this is built with ppu-g++; the SFML layer will be replaced by a PS3
 * backend (ya2d / pad / MikMod / Clay) behind a thin sf:: shim.
 *
 * Phase 1 (engine bring-up): initialize the PSL1GHT 2D stack and render a test
 * frame, exiting cleanly on START / XMB. No game yet. The SFML shim + the ECS
 * source land in Phase 2 (todo/ROADMAP.md).
 *
 * C++/C interop: the PS3 libs are C. tiny3d.h / io/pad.h / sysutil.h carry their
 * own `extern "C"` guards; ya2d.h does NOT, so it's wrapped below. ttf_render.h
 * was given a guard when vendored.
 */

#include <cstdio>

/* Pre-include the C system headers ya2d.h pulls in (it has no extern "C" guard),
 * so their own include guards are set BEFORE the extern "C" wrap below — otherwise
 * machine/malloc.h's vec_* decls land inside extern "C" and clash. */
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include <ppu-types.h>
#include <io/pad.h>
#include <sysutil/sysutil.h>
#include <tiny3d.h>

extern "C" {
#include <ya2d/ya2d.h>
}

#include "ttf_render.h"

#define SCREEN_WIDTH  848
#define SCREEN_HEIGHT 512

/* RGBA (0xRRGGBBAA) for ya2d / ttf; tiny3d_Clear wants 0xAARRGGBB. */
#define COLOR_WHITE  0xFFFFFFFF
#define COLOR_SKY    0xff5C94FC   /* classic Mario sky (ARGB for tiny3d_Clear) */
#define GROUND_COL   0xC84C0CFF   /* brick brown (RGBA for ya2d)               */

static int running = 1;
static padInfo pad_info;
static padData pad_data;
static u32 *ttf_texture = nullptr;

static void sys_callback(u64 status, u64 param, void *userdata)
{
	(void)param;
	(void)userdata;
	if (status == SYSUTIL_EXIT_GAME)
		running = 0;
}

static void init_fonts()
{
	/* PS3 system fonts (present on real consoles + RPCS3). */
	TTFLoadFont(0, (char *)"/dev_flash/data/font/SCE-PS3-SR-R-LATIN2.TTF", nullptr, 0);
	TTFLoadFont(1, (char *)"/dev_flash/data/font/SCE-PS3-DH-R-CGB.TTF", nullptr, 0);
	TTFLoadFont(2, (char *)"/dev_flash/data/font/SCE-PS3-SR-R-JPN.TTF", nullptr, 0);
	ttf_texture = (u32 *)init_ttf_table((u16 *)ya2d_texturePointer);
	ya2d_texturePointer = ttf_texture;
}

static void init_screen()
{
	tiny3d_Init(1024 * 1024);
	ya2d_init();
	init_fonts();
	set_ttf_window(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
}

/* Edge-triggered START so a held button doesn't re-fire. */
static int start_pressed()
{
	static u32 prev = 0;
	u32 cur = 0;
	ioPadGetInfo(&pad_info);
	if (pad_info.status[0]) {
		ioPadGetData(0, &pad_data);
		cur = pad_data.BTN_START;
	}
	u32 pressed = cur & ~prev;
	prev = cur;
	return pressed != 0;
}

static void begin_2d_frame()
{
	tiny3d_Clear(COLOR_SKY, TINY3D_CLEAR_ALL);
	tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);
	tiny3d_BlendFunc(1,
		(blend_src_func)(TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA),
		(blend_dst_func)(TINY3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA | TINY3D_BLEND_FUNC_DST_ALPHA_ZERO),
		(blend_func)(TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD));
	tiny3d_Project2D();
	reset_ttf_frame();
}

int main(int argc, const char *argv[])
{
	(void)argc;
	(void)argv;

	std::printf("\n=== PS3 Mega Mario (Phase 1 bring-up) ===\n");

	sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sys_callback, nullptr);
	init_screen();
	ioPadInit(7);

	while (running) {
		if (start_pressed())
			running = 0;

		begin_2d_frame();
		/* a ground strip + a title, so it's clearly alive */
		ya2d_drawFillRectZ(0, SCREEN_HEIGHT - 60, 0, SCREEN_WIDTH, 60, GROUND_COL);
		display_ttf_string(60, 60, "MEGA MARIO", 0xFFD23FFF, 0, 40, 52);
		display_ttf_string(62, 120, "PS3 port - Phase 1 (engine bring-up)", 0xA0A0A0FF, 0, 14, 20);
		display_ttf_string(60, SCREEN_HEIGHT - 100, "Press START to exit", COLOR_WHITE, 0, 14, 20);
		tiny3d_Flip();

		sysUtilCheckCallback();
	}

	std::printf("Exiting...\n");
	ya2d_deinit();
	ioPadEnd();
	return 0;
}
