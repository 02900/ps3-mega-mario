# Third backend: raw RSXGL/OpenGL 2D renderer (`rsxgl-backend`) — COMPLETE

## Context

mega-mario's SFML shim has swappable backends. This is the **third**, rendering 2D
**directly on RSXGL/OpenGL** (a small ortho renderer), no raylib/Tiny3D. Motivation:
the raylib backend's debug-grid text corrupts because rlgl merges all same-font-atlas
text into one huge `glDrawElements` that RSXGL mishandles, and RSXGL exposes no GPU
sync to fully fix it (see `ideas/raylib-backend.md`). Rendering raw means *we* control
batching — each text string is its own small draw — which sidesteps the corruption by
construction. Verified on RPCS3: gameplay, camera, input, audio, the Clay menu, clean
debug-grid text, correct colours, 60 fps.

## What changed

Only two files rewritten (SOURCES wildcard-compiles `source/`), everything else
(shim headers, `clay_menu.c`, `audio.*`, `asset_registry.*`, the game/ECS) untouched:

- **`source/sfml_backend.cpp`** — the GL renderer: EGL/RSXGL context (from
  `ps3-gl-test`), one 2D textured-quad shader (`#version 130`, ortho, alpha blend;
  solid fills use a 1x1 white texture × vertex colour), libpng-from-memory PNG decode
  → `glTexImage2D`, the shim's world→screen camera math, 60fps `gettimeofday` pacing,
  and raw `ioPad` → `sf::Event` input (ported from the Tiny3D backend on `main`), pad
  init AFTER the GL context.
- **`source/clay_renderer_raylib.c`** — draws Clay's RECTANGLE/BORDER/TEXT via the
  shared `gl2d` helpers.
- New: **`source/gl2d.h`** (shared 2D helpers: `gl2d_rect`/`gl2d_text`/
  `gl2d_text_width` + `gl2d_screen_w/h`) and **`source/font8x8.h`** (public-domain
  8x8 bitmap ASCII font, no TTF/FreeType).
- Build: `ghcr.io/02900/ps3-toolchain-rsxgl`; `LIBS` drop `-lraylib` (keep
  EGL/GL/rsx/gcm/io/png/z/mikmod/audio).

## RSXGL findings (the hard part — reusable for any raw-RSXGL work)

1. **Textures come out channel-rotated — byte-swap every pixel.** Uploading
   `GL_RGBA`/`GL_UNSIGNED_BYTE` bytes `[R,G,B,A]`, the sampler returns them rotated
   (orange bricks → magenta, green pipes → purple; the sky was fine because it's the
   `glClearColor`, not a texture). Fix on the CPU: `__builtin_bswap32` each pixel
   (`[R,G,B,A] → [A,B,G,R]`) before `glTexImage2D`. This is a known "buggy PS3 opengl
   driver" quirk — **raylib-ps3 does the exact same thing** (rtextures.c
   `LoadTextureFromImage`, "Handle buggy PS3 opengl driver and swap endianess"),
   which is why raylib got correct colours with an identical `glTexImage2D(GL_RGBA)`.
2. **RSXGL ignores extra fragment-shader uniforms.** Only the sampler and *vertex*
   uniforms (the ortho matrix) work. An `int` mode uniform or a `mat4` used only in
   the fragment shader → `glGetUniformLocation` returns -1, the value never arrives
   (reads as 0 → a zero permutation matrix → blank screen), and fragment dynamic
   branching on a uniform isn't honoured by the Cg compiler. Do per-fragment
   variation on the CPU, not via fragment uniforms. (Cost us two dead ends — an
   int-uniform swizzle selector and a mat4 permutation selector — before finding #1.)
3. **Batch quads to hit 60fps.** One `glBufferData`+`glDrawArrays` per quad is
   hundreds of tiny uploads/frame (the debug grid's ~500 line-quads dropped it to
   ~20fps). Accumulate quads into a CPU buffer and flush only on texture change /
   buffer full / frame end; flushing on every texture change preserves painter's
   order and consecutive same-texture quads (grid lines, white rects) coalesce to ~1
   draw. **But flush text per-string** so glyph batches stay small — a big merged
   font-atlas draw is exactly the RSXGL corruption this backend set out to avoid.

Also reused from `ps3-gl-test`: link with the C++ driver (`LD:=$(CXX)`), `-D__RSX__`,
pad init AFTER `eglMakeCurrent`, manual 60fps pacing (`eglSwapBuffers` doesn't
throttle). See memory `project_rsxgl-toolchain-variant`.

## The three backends

| Backend | Branch | Renders via | Notes |
|---|---|---|---|
| Tiny3D | `main` | Tiny3D + ya2d (+ ttf_render text) | original PS3 port |
| raylib | `raylib-backend` | raylib over RSXGL | clean, but debug-grid text corrupts (rlgl big-batch) |
| raw RSXGL | `rsxgl-backend` | hand-written 2D GL (ortho, libpng, 8x8 font) | debug-grid text clean; 60fps; correct colours |

## Verification

`./scripts/build.sh` (auto-pulls the rsxgl image) → `src.self` → RPCS3. Confirmed:
menu (Clay + bitmap font), gameplay (sprites/camera/flip), input (D-pad/stick, Cross
jump, Start exit), audio, **debug grid (F) with clean text and correct colours**, 60fps.
