# Choosing a render backend — Tiny3D vs raylib vs raw RSXGL

mega-mario ships the same game on three interchangeable render backends, one per
branch: `main` = **Tiny3D + ya2d**, `raylib-backend` = **raylib**, `rsxgl-backend` =
**raw RSXGL/OpenGL**. This is the practical verdict from porting the *same* game to
all three — ease of use, performance, and render quality on PS3 / RPCS3.

See also: `ideas/benchmark.md` (the numbers), `ideas/raylib-backend.md` and
`ideas/rsxgl-backend.md` (the per-backend deep dives + gotchas).

## Ease of use — raylib is the simplest by far

| backend | handles for you | you write | rank |
|---|---|---|:--:|
| **raylib** | EGL context, pad init, frame pacing (`SetTargetFPS`), PNG load (`LoadTextureFromImage` — *incl.* the RSX colour byte-swap), text (`DrawTextEx` + a built-in font), gamepad input | almost nothing — `InitWindow` / `LoadTexture` / `DrawTexturePro` | 🥇 |
| **Tiny3D + ya2d** | 2D sprite/texture helpers (`ya2d`), text from `/dev_flash` TTF (`ttf_render`), the RSX plumbing | your draw calls, plus the gotchas below | 🥈 |
| **raw RSXGL** | nothing above OpenGL — it's standard GL | the *entire* 2D layer: EGL context, GLSL shaders, VBO/VAO + a batcher, libpng decode, a bitmap font, the colour byte-swap, pad reader, frame pacing | 🥉 |

With raylib you call a handful of functions and it works. Tiny3D gives you 2D helpers
but its own conventions. Raw RSXGL means building the whole 2D engine yourself.

## Render quality on RSXGL (what "simple" costs you)

- **raylib** — the friendly API hides real RSXGL problems we could **not** fully fix:
  heavy text corrupts (rlgl merges same-atlas glyphs into one draw the RSX mangles, and
  RSXGL exposes no GPU sync to force it), and it **drops sprites under load** (white
  squares from ~2000 sprites). Fast to write, but quality is capped by what raylib does
  internally.
- **Tiny3D** — renders everything correctly; watch the **ARGB-vs-RGBA** colour trap and
  the fixed 848×512 2D canvas.
- **raw RSXGL** — you control batching, colour order and sync, so it's the only backend
  with **clean debug-grid text** *and* it renders every sprite. The price is discovering
  the gotchas yourself (per-pixel `__builtin_bswap32` for colours, fragment-shader
  uniforms being ignored, no sync primitives).

## Performance (RPCS3, from `ideas/benchmark.md`)

Below ~8000 sprites all three sit at the 60fps cap. When it becomes render-bound the
optimized raw-RSXGL backend is **fastest** (16k: 34.6fps vs raylib 29.9 / tiny3d 28.9;
32k: 17.3 vs 14.8 / 13.7) — and it draws every sprite, whereas raylib's higher-looking
numbers come partly from dropping sprites.

## The trade-off, in one line

> **raylib** — prototype fast with the least code (accept the RSXGL quality caveats).
> **Tiny3D** — the balanced PS3-native path.
> **raw RSXGL** — most work to write, but most control and the best performance/quality
> once built.

It's the classic **ease-of-use ↔ control/performance** triangle. The twist here: after
the one-upload-per-frame optimization, the *hardest* backend (raw RSXGL) also turned out
to be the *fastest* and the only one with clean text — so "simplest to use" (raylib) is
not "best result" on this hardware.
