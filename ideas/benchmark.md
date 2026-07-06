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
# In game: menu -> BENCHMARK -> let all 5 stages complete (progress bar fills green).
```

Then compare the three CSVs:

```bash
python3 scripts/bench-compare.py
# auto-finds ~/Library/Application Support/rpcs3/dev_hdd0/tmp/bench_*.csv,
# prints an avg-FPS-per-stage table + ratios, writes bench-comparison.csv
```

## Tuning

Stage targets / frame counts are `const`s at the top of `Scene_Benchmark.h`. If even
4000 sprites stays at 60fps for all three (fast host), raise the targets; if the run is
too long, lower `STAGE_FRAMES`.
