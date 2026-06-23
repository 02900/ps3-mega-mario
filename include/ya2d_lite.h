/*
 * ya2d for C++ TUs, MINUS ya2d_controls.h.
 *
 * The umbrella <ya2d/ya2d.h> pulls in ya2d_controls.h, which defines globals
 * (`padData ya2d_paddata[7];`, `padInfo2 ya2d_padinfo;`) WITHOUT `extern`. In C
 * those are tentative defs that merge; in C++ they are real definitions, so any
 * second C++ TU that includes ya2d → duplicate-symbol link error. We don't use
 * ya2d's controls anyway (input goes through io/pad.h), so include the sub-headers
 * we need and skip controls. See docs/PATTERNS.md §7.
 *
 * ya2d.h has no extern "C" guard and pulls C system headers, so pre-include the
 * system headers (guards set) before the extern "C" wrap.
 */
#pragma once
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <ppu-types.h>
#include <tiny3d.h>

extern "C" {
#include <ya2d/ya2d_globals.h>
#include <ya2d/ya2d_main.h>
#include <ya2d/ya2d_texture.h>
#include <ya2d/ya2d_draw.h>
#include <ya2d/ya2d_screen.h>
#include <ya2d/ya2d_utils.h>
}
