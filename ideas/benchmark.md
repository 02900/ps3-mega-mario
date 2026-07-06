# Cross-backend rendering benchmark

Compares the three render backends (Tiny3D on `main`, raylib on `raylib-backend`,
raw RSXGL on `rsxgl-backend`) under an identical, deterministic sprite load.

## What it does

`source/Scene_Benchmark.{h,cpp}` (a `Scene`, launched from the menu's **BENCHMARK**
entry) ramps the live-sprite population through fixed stages
`{250, 500, 1000, 2000, 4000}`, holding each for `STAGE_FRAMES = 150` frames (first
`WARMUP = 30` excluded). Sprites are cheap single-frame textures
(`Bullet/Brick/Block/Ground/Question2`, round-robined) with random velocities that
bounce off the screen edges, and a 60–180 frame `CLifeSpan` so there is **continuous
spawn/destroy churn** (each frame refills to the stage target — stresses the
`EntityManager` add/remove too). A seeded LCG drives every spawn, so all three
backends run a byte-identical workload (same game code + same ppu-g++ → deterministic).

Per-frame time is sampled with `gettimeofday()`. At each stage boundary it appends a
CSV row via `sysFsOpen/Write/Close` to:

    /dev_hdd0/tmp/bench_<backend>.csv        (falls back to /dev_hdd0/ if /tmp missing)

On macOS RPCS3 maps that to
`~/Library/Application Support/rpcs3/dev_hdd0/tmp/bench_<backend>.csv`.

CSV columns: `backend,stage,target,live,frames,avg_ms,avg_fps,min_fps,max_fps,spawns,destroys`.

The on-screen indicator is a `RectangleShape` progress bar (renders on all three
backends; `draw(Text)` is a no-op on the Tiny3D shim) — the growing sprite count is
the load indicator. `Escape` returns to the menu.

## Design choices

- **60fps cap left ON** all three. Light stages read ~60 for everyone; the backends
  separate in the heavy, render-bound stages where the cap is inactive. This is the
  only fair option since Tiny3D's `tiny3d_Flip` VSYNC can't be cleanly disabled.
- **Measured under RPCS3, not a real PS3.** Valid as a *relative* comparison of the
  render paths, not as absolute PS3 performance. For heavy stages to reflect render
  cost (not RPCS3's own limiter), set RPCS3's Frame limit to Off/Auto.
- **Same-texture batching** is real and intentionally exercised: rsxgl/raylib coalesce
  same-texture quads, so the multi-texture sprite set gives a realistic draw-call load.

## Per-backend bits

- `source/sfml_backend.cpp` defines `const char *BACKEND_NAME` ("tiny3d"/"raylib"/"rsxgl").
- `Makefile` links `-lsysfs` (sysFsOpen lives in libsysfs, not liblv2).
- Everything else (`Scene_Benchmark.*`, the menu entry) is identical on all three branches.

## Running it

For each branch:

```bash
git checkout <main|raylib-backend|rsxgl-backend>
DOCKER_DEFAULT_PLATFORM=linux/amd64 ./scripts/build.sh
/Applications/RPCS3.app/Contents/MacOS/rpcs3 src.self
# In game: menu -> BENCHMARK -> let all 6 stages complete (progress bar fills green).
```

Then compare the three CSVs:

```bash
python3 scripts/bench-compare.py
# auto-finds ~/Library/Application Support/rpcs3/dev_hdd0/tmp/bench_*.csv,
# prints an avg-FPS-per-stage table + ratios, writes bench-comparison.csv
```

## Results (RPCS3, 6-stage ramp)

Average FPS per stage, higher = better. **Bold** = fastest at that load. Measured under
RPCS3 (an emulator, not a real PS3) — a *relative* comparison of the render paths.

| sprites | tiny3d | raylib | rsxgl (optimized) |
|--------:|-------:|-------:|------------------:|
|   1000  | **60.0** | 59.1 | 57.5 |
|   2000  | **60.0** | 59.1 | 57.7 |
|   4000  | 59.1 | **60.0** | 57.6 |
|   8000  | 57.8 | **59.8** | 58.4 |
|  16000  | 28.9 | 29.9 | **34.6** |
|  32000  | 13.7 | 14.8 | **17.3** |

**Reading it:**
- **≤ 8000 sprites** — all three are pinned at the 60fps cap (not render-bound). rsxgl sits
  ~3% under because the multi-texture workload forces one `glDrawArrays` per sprite (fixed
  draw-call overhead); it's cap-bound anyway.
- **16000–32000 (render-bound)** — **rsxgl is fastest**: +15% over raylib and +20–26% over
  tiny3d. The optimized "one VBO upload + N cheap draws per frame" path scales better than
  rlgl's and ya2d's per-sprite work.
- **Quality caveat — raylib drops sprites under load.** From ~2000 sprites raylib renders
  **white squares** in place of many sprites (rlgl can't draw them all). So raylib's numbers
  above 2000 are inflated — it's partly fast because it's drawing *less*. tiny3d and rsxgl
  render every sprite. A backend holding 17fps drawing all 32000 correctly beats one showing
  a higher number while dropping half of them.

**Bottom line:** the optimized raw-RSXGL backend is the strongest of the three at scale —
fastest under heavy load, one of only two that render every sprite correctly, and the only
one with clean debug-grid text. The turning point was the batcher rework (one `glBufferData`
per frame instead of one per draw run): it took rsxgl from ~15× slower than the others to the
outright winner. See the `perf(rsxgl)` commit.

## Tuning

Stage targets / frame counts are `const`s at the top of `Scene_Benchmark.h` (rsxgl's frame
buffer `BATCH_MAX_VERTS/RUNS` in `sfml_backend.cpp` must fit the largest stage in one upload).
If even the top stage stays at 60fps for all three (fast host), raise the targets; if the run
is too long, lower `STAGE_FRAMES`.
