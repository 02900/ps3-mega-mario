# Mega Mario — raylib render backend (branch `raylib-backend`)

## Why

The PS3 port renders through an SFML shim backed by **Tiny3D/ya2d** (sprites),
**Clay over Tiny3D/ttf** (menu/HUD), and **ttf_render** (text). This branch swaps
that whole render path to **raylib** (the `ghcr.io/02900/ps3-toolchain-raylib`
image, raylib over RSXGL) to dogfood raylib on a real game and compare. raylib and
Tiny3D can't share a frame (each drives the RSX its own way), so it's all-or-nothing.

Keep: game logic, **audio** (MikMod, independent of the GPU), asset registry.
Drop/replace: Tiny3D, ya2d, ttf_render, font3d.

## Surfaces to port

1. **SFML sprite backend** (`source/sfml_backend.cpp`) — the core.
   - `Texture::loadFromFile`: `LoadImageFromMemory(".png", buf, size)` →
     `LoadTextureFromImage`, `SetTextureFilter(POINT)` (crisp pixel art).
   - `RenderWindow::create` → `InitWindow` + `SetTargetFPS(60)` (raylib also inits
     the pad here). `clear` → `BeginDrawing`+`ClearBackground`. `display` →
     `EndDrawing` + `audio_update()` + XMB-exit check (`WindowShouldClose`).
   - `draw(Sprite)` → `DrawTexturePro(tex, srcRec, destRec, origin, 0, tint)`;
     flip via negative `srcRec.width`; world→screen with the `sf::View` camera and
     `GetScreenWidth()/m_w` scale. `draw(RectangleShape)`/`draw(Vertex* Lines)` →
     `DrawRectangle(Lines)` / `DrawLineV`. `sf::Text` is already a no-op.
2. **Input** (`build_pad_events`) — raylib already reads the pad in `InitWindow`, so
   raw `ioPadGetData` would be starved (one-reader-per-port). Re-source the same
   sf::Event(KeyPressed/Released) edges from raylib's gamepad API
   (`IsGamepadButtonDown(0, GAMEPAD_BUTTON_*)`, `GetGamepadAxisMovement`).
3. **Clay menu** (`extern/clay-ps3/clay_renderer.c`) — provide a **raylib Clay
   renderer** (Clay emits RECTANGLE/TEXT/BORDER/IMAGE commands → `DrawRectangle`,
   `DrawTextEx`, `DrawRectangleLinesEx`). Clay ships an upstream raylib renderer to
   adapt. Fonts via raylib (`GetFontDefault`/`LoadFontFromMemory`), dropping ttf_render.

## Build changes

- **Makefile**: image → `ghcr.io/02900/ps3-toolchain-raylib`; `LIBS` →
  `-lraylib -lEGL -lGL -lrsx -lgcm_sys -lio -lsysutil -lsysmodule -lnet -lrt -llv2
  -lpng -lz -lmikmod -laudio -lm` (drop `-ltiny3d -lya2d -lfont3d -lfreetype
  -lpngdec -ljpgdec`); keep `LD := $(CXX)` (RSXGL underneath is C++). `-D__RSX__`.

## Stages (build-green at each)

1. **Sprites + input + window** in raylib; Clay menu temporarily stubbed
   (`clay_backend_init`/`clay_render` → no-op, drop `extern/clay-ps3` + `ttf_render`
   from SOURCES). Gameplay (`Scene_Play`) renders; navigate the blank menu by pressing
   start/select. Validate on RPCS3.
2. **Clay menu** ported to a raylib renderer (menu/HUD back).
3. Polish: resolution/letterboxing, exact colour/blend parity, debug overlays.

## Risks

- raylib's GLES2 over RSXGL (same stack as ps3-gl-test/raylib-test — proven to build
  & run). Pad mapping order/!directions to verify on hardware (see memory
  `project_raylib-ps3-variant`, `psl1ght-input` skill). DrawTexturePro flip sign and
  the Y-down/world→screen mapping need an on-hardware check.
