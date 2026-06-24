# PS3 Mega Mario — Migration Roadmap

Porting **[mega-mario](https://github.com/Terpodia/mega-mario)** (C++17/20, SFML, ~1,300
LOC) to PS3 homebrew (PSL1GHT). Structured like a phase list with **Done / In progress /
Open**; each phase names the original files it touches.

Legend: ✅ done · 🚧 in progress · ⬜ not started

---

## Source game at a glance

A clean C++ **Entity Component System** platformer (Mario × Mega Man) with a factory for
entity creation, rendered with **SFML 2.6**:

- **Engine/ECS**: `GameEngine.{h,cpp}` (window + scene stack + main loop), `Entity`,
  `EntityManager`, `Components.h`, `Action.{h,cpp}`, `Vec2.{h,cpp}`.
- **Scenes**: `Scene.{h,cpp}`, `Scene_Menu.{h,cpp}`, `Scene_Play.{h,cpp}` (level, physics,
  rendering, input).
- **Assets/anim**: `Assets.{h,cpp}`, `Animation.{h,cpp}`, `Texture.h`, `Physics.{h,cpp}`.
- **Content** (`bin/`): 22 PNG sprites (mario 16, megaman 4, others 3), 2 TTF fonts, and
  `.txt` config files (assets list, level layout, animations).

**SFML surface to replace** (grep-confirmed, small): `sf::RenderWindow`, `sf::View`,
`sf::Texture`, `sf::Sprite`, `sf::IntRect`, `sf::Vertex`/`VertexArray` (lines),
`sf::RectangleShape`, `sf::Font`, `sf::Text`, `sf::Color`, `sf::Vector2`, `sf::Keyboard`,
`sf::Event`, `sf::Music`/`sf::Sound`, `sf::VideoMode`/`sf::Style`.

---

## Phases

### Phase 0 — Scaffold ✅ (this commit)
- Repo layout, **C++ Makefile** (`gnu++17`, `-fno-exceptions -fno-rtti`), `sfo.xml`
  (`MEGAMARIO`), Docker `build.sh`/`deploy.sh`, CI (`build` + `lint`), README, themed
  ICON0, this roadmap, and `docs/PATTERNS.md` (carried-over PS3 conventions).
- `extern/clay-ps3` submodule **checked in** but not compiled yet (its `clay_renderer.c`
  needs the `ttf_render` helper, vendored in Phase 1). The trivial `main.cpp` builds green.

### Phase 1 — Build & toolchain green ✅
- ✅ Vendored `ttf_render.{c,h}` (with an added `extern "C"` guard) and **re-added
  `extern/clay-ps3`** to the Makefile `SOURCES` / `INCLUDES`. Clay now compiles (C).
- ✅ A real C++ `main.cpp` inits Tiny3D + ya2d + fonts and renders a test frame
  (sky + ground strip + "MEGA MARIO" title), exiting cleanly on START / XMB.
- ✅ Solved the **C++↔C header interop**: `tiny3d.h`/`io/pad.h`/`sysutil.h` carry their own
  `extern "C"` guards; `ya2d.h` doesn't, so it's wrapped — with the C system headers
  (`stdlib.h`/`malloc.h`/`string.h`) **pre-included** so `machine/malloc.h`'s `vec_*` decls
  don't land inside the `extern "C"` block and clash.
- ✅ `make` builds a valid `src.self` (~428 KB); the trivial test screen replaces the stub.
- ✅ **Exit criteria — boots to the test screen:** confirmed on RPCS3 (sky + ground +
  "MEGA MARIO" title render; START exits). On-PS3-hardware still to be confirmed.
- Note: bringing the original `src/` in + the SFML shim moved to **Phase 2** (it was too
  much for one phase; Phase 1 is the clean rendering foundation).

### Phase 2 — SFML compatibility shim ⬜
- Implement the `sf::` shim (`include/SFML/…`) over PS3 backends (see `docs/PATTERNS.md`
  §7): `RenderWindow`→tiny3d clear/flip, `Texture`→`ya2d_Texture`, `Sprite`→textured quad
  (+`IntRect` sub-rect), `View`→2D camera offset, `Color`/`Vector2`/`IntRect`/
  `RectangleShape` value types.
- **UI is Clay, not hand-drawn (`docs/PATTERNS.md` §3.5).** The shim renders the game
  **scene** (tiles/sprites/player) with ya2d; the **menus (`Scene_Menu`) and HUD**
  (score/coins/lives/time) are (re)built in **Clay**, *not* via a hand-drawn `sf::Text`
  shim — mixing hand-drawn text with the sprite scene caused frame glitches in the sister
  port. So `sf::Text`/`sf::Font` get a minimal stub (enough to link), and the UI screens
  are implemented natively in Clay (Phase 7).
- ✅ **Done.** The header-only `include/SFML/{System,Window,Graphics,Audio}.hpp` shim
  covers the ~20 methods the game uses; the **entire ECS (`source/*.cpp`, 1,297 LOC)
  compiles & links UNCHANGED** at `gnu++17` (`src.self` ~808 KB). `std::to_string`/`stoi`/
  `stof` (absent on newlib) are supplied by `include/ps3_compat.h`, force-included via the
  Makefile. The bring-up `main.cpp` is still the entry; the ECS is linked but dormant.
- ⬜ Backend + running the game (load assets from memory, draw via ya2d) follow in
  Phases 3 / 5.

### Phase 3 — Asset pipeline ✅ (sprite draw; sub-rect + configs in Phase 5)
- ✅ Embedded the **22 PNG sprites** via `bin2o` (`data/`) + a generated
  `source/asset_registry.cpp` mapping a path's **basename → embedded buffer**.
- ✅ **`sf::Texture::loadFromFile`** now maps path → registry → `ya2d_loadPNGfromBuffer`,
  and **`sf::RenderWindow::draw(Sprite)`** draws via `ya2d_drawTextureEx` (applying the
  `sf::View` as a 2D camera offset) — implemented in `source/sfml_backend.cpp` (the only
  TU that touches ya2d besides main).
- ✅ Fixed a C++ multi-TU link clash: `ya2d/ya2d_controls.h` defines globals **without
  `extern`** (duplicate symbols across C++ TUs) — added `include/ya2d_lite.h` (ya2d minus
  controls; we use `io/pad.h` for input).
- ✅ A smoke test in `main.cpp` loads + draws real sprites (megaman + question block +
  cloud) through the shim. Builds green (`src.self` ~864 KB).
- ⬜ **Exit criteria — sprites draw on hardware:** confirm on PS3/RPCS3.
- Note: full-texture draw only (sprite-sheet **sub-rects** for animation frames, plus
  **nearest-neighbour** filtering for crisp pixel art) and **config parsing from memory**
  (`assets.txt`/`level1.txt`) land in Phase 5 with the real scene rendering.

### Phase 4 — Input ✅ (backend; full game-input verify with Phase 5)
- ✅ `sf::RenderWindow::pollEvent` is implemented in `source/sfml_backend.cpp`: it reads
  the DualShock (retained-packet pattern, `docs/PATTERNS.md` §2.1) and emits
  `sf::Event` KeyPressed/KeyReleased via a small event queue — the exact path the game's
  `sUserInput()` consumes.
- ✅ Mapping (each logical key OR-combines pad buttons): D-pad/L-stick → **W/A/S/D**
  (Up·Jump / Left / Down / Right), **Cross** → W (jump), **Circle** → Enter (menu select),
  **Square** → Space (shoot), **Triangle** → P (pause), **Start** → Escape (quit/back) —
  covering both `Scene_Menu` and `Scene_Play` action sets.
- ✅ Smoke test: Mega Man moves with the D-pad/stick and the HUD shows the last action,
  proving the press/release event flow. Builds green (`src.self` ~864 KB).
- ⬜ **Exit criteria — menu nav + in-game move/jump on the real game:** verified once the
  game actually runs (Phase 5); the input layer itself is done.

### Phase 5 — Rendering & camera ✅
- ✅ **Game wired in.** `source/main.cpp` is now the original entry
  (`GameEngine game("../bin/assets.txt"); game.run();`); all PS3 bring-up (tiny3d / ya2d /
  pad / fonts / XMB callback) moved into `sf::RenderWindow::create()` in
  `source/sfml_backend.cpp`, and `clear()`/`display()` drive the tiny3d frame (begin / flip).
- ✅ **Config from memory.** Embedded `assets.txt` + `level1/2/3.txt` via `bin2o`
  (`data/*_txt.bin`); `GameEngine::init` and `Scene_Play::init` read them through
  `load_config()` + `std::istringstream` instead of `std::ifstream` (no PS3 filesystem).
- ✅ **Sub-rect sprite rendering.** `draw(Sprite)` is a textured tiny3d quad honoring the
  texture **sub-rect** (animation frames / sheet cells), origin, scale (negative `scale.x` =
  facing flip), the **`sf::View` camera** (`screen = world − camera`), and **nearest-neighbour**
  filtering (crisp pixel art). World is a virtual **1920×1080** (SFML Y-down) mapped onto
  tiny3d's fixed **848×512** 2D canvas (`screen = (world − cam) · canvas/window`).
- ✅ Debug overlays wired (`J` collision AABBs, `F` grid) via the same camera mapping.
- ✅ Builds green (`src.self` ~907 KB). Boots to `Scene_Menu` (text is stub → blank sky
  until Phase 7's Clay menu); **press ○ (Circle → Enter) to start Level 1** and see the scene.
- ✅ **Off-screen culling**: `draw(Sprite)` skips sprites whose screen AABB misses the
  848×512 canvas (the level is ~210 tiles wide; only ~30 are on-screen).
- ✅ **Exit criteria — a level renders and scrolls: CONFIRMED on RPCS3.** Full scene
  (ground, bricks, pipes, `?`-blocks, bushes, hills, clouds, Mega Man) renders stable and
  scrolls horizontally with the player.
- ⚠️ **Root-cause fixed — vertical camera flicker.** `Scene_Play::sRender` sets the camera
  via `height() - getView().getCenter().y`, which feeds the previous frame's center.y back.
  SFML's window view defaults to *centered* (center = size/2) so `1080-540==540` is a stable
  fixed point; our `sf::View()` defaulted to center `(0,0)`, making it oscillate `1080<->0`
  each frame (whole scene jumped ±256px vertically → half the entities culled, alternating).
  Fixed by initializing the window view to the centered default in `RenderWindow::create()`.

### Phase 6 — Physics & gameplay 🚧
- ⚠️ **Fixed a silent float-truncation bug in collision.** `Physics::GetOverlap` (and the
  player flip in `Scene_Play`) called `abs()` on **floats**, but no `<cmath>` was included, so
  it resolved to C's `abs(int)` — truncating the fractional part of every overlap and, in
  `flip /= abs(flip)`, dividing by zero when `|scale.x| < 1`. The original game got `<cmath>`
  transitively via SFML headers; our minimal shim doesn't. Fixed with explicit `std::fabs`
  (`#include <cmath>`). This directly governs AABB precision (landing on tiles, not clipping).
- Timestep: the game uses fixed per-frame deltas (no dt); `tiny3d_Flip()` caps to vblank
  (60 fps), matching the original's tuning. No change needed unless it feels off.
- **Exit criteria:** Mega Man runs, jumps, lands on tiles, breaks bricks, bumps `?`-blocks
  (→ coin), shoots, and the flag resets the level — verify on RPCS3 / PS3.

### Phase 7 — UI / HUD / menu 🚧
- ✅ **`Scene_Menu` rebuilt in Clay** (not hand-drawn `sf::Text`, per `docs/PATTERNS.md` §3.5).
  New C TU `source/clay_menu.{c,h}` builds the layout (title + "Select a level" + the 3 level
  rows, selected one highlighted with a gold border + darker row + controls hint); `Scene_Menu::sRender`
  calls `clay_render_menu(...)` between `clear()` and `display()`. `clay_backend_init(848,512)`
  runs once in `platform_init`. Pure-C UI mirrors the sister port so the `CLAY_*` macros compile
  under `-std=gnu99`; C++ calls it via an `extern "C"` bridge header.
- ✅ **Menu navigation fixed for the X-only-jump mapping**: D-pad/stick **Up/Down → arrow keys**
  (`Scene_Menu` now registers `Up`/`Down` instead of `W`/`S`, which freed `W` to be jump-only),
  **Circle → select**, **Start → exit**.
- ⬜ This game has **no in-game HUD** (no score/lives/time in `Scene_Play`), so the HUD half of
  this phase is N/A — the menu is the deliverable.
- ⬜ **Exit criteria — menu → play → back loop on hardware:** verify on RPCS3 (title + rows
  render, highlight moves with the D-pad, Circle enters the level, the flag/quit returns).

### Phase 8 — Audio 🚧
- ✅ **MikMod audio, fully synthesized in code** (`source/audio.{c,h}`). The original ships no
  audio, so every sound is generated as 16-bit mono PCM (square/noise + sweep + click-free
  envelope), wrapped in a **little-endian** WAV (PPU is big-endian — bytes emitted by hand,
  §5.3) and loaded via an in-memory `MREADER` → `Sample_LoadGeneric` (§5.2). No `libm`
  (manual phase wrap instead of `fmod`).
- ✅ **SFX**: jump (rising sweep), coin (B5→E6 two-tone), brick-break (thud + noise), shoot
  (descending blip), level-clear (C-E-G-C-E arpeggio). **Music**: a looping square-wave melody
  played as an `SF_LOOP` sample on an `SFX_CRITICAL` voice, ducked under SFX.
- ✅ **Defensive init** (§5.5): `audio_init()` no-ops on any MikMod failure (`audio_ok` guards
  every entry point) so bad audio can't hang the console. `audio_update()` runs each frame in
  `RenderWindow::display()`; events hooked in `Scene_Play` (jump/shoot/coin/brick/flag);
  `audio_shutdown()` on exit. Makefile already linked `-lmikmod -laudio`.
- ✅ Builds green (`src.self` ~989 KB).
- ⬜ **Exit criteria — music + SFX audible on hardware:** verify on RPCS3 / PS3 (and tune
  volumes / the melody if grating).

### Phase 9 — Packaging & polish 🚧
- ✅ **Installable PKG**: `make pkg` (or `./scripts/build.sh pkg`) produces a valid PS3 PKG
  (`src.pkg`, ~1 MB, magic `\x7fPKG`, `CONTENT_ID = UP0001-MEGAMARIO_00-…`). The `.self` is
  self-contained (all sprites + level/asset configs embedded via `bin2o`), so the PKG just
  wraps it as `EBOOT.BIN` + `sfo.xml` + `ICON0.PNG`; `pkgfiles/assets/` is empty by design.
- ✅ **ICON0.PNG** themed, correct XMB size (**320×176**); `sfo.xml` set (TITLE "Mega Mario",
  TITLE_ID `MEGAMARIO`). README documents build / `make pkg` / install.
- ✅ **Performance**: stable 60 fps; off-screen sprite culling + 8 MB tiny3d vertex buffer.
- ⬜ **Exit criteria — installs + boots from XMB:** verify on RPCS3 (File → Install .pkg →
  launch from the game list) and ideally on real hardware.
- ⚠️ **Known issue (separate from packaging):** an intermittent ~1-frame render glitch (a
  sprite jumps / some tiles vanish for a single frame every few seconds). Not gameplay/camera;
  looks like RSX vertex/command-buffer reuse. Diagnostic plan parked in
  `ideas/session-recorder-diagnostic.md`.

---

## Risks & open questions

- **C++20 → C++17 (gcc 7.2).** The original targets C++20; ppu-g++ is 7.2 (good C++17).
  Expect to replace concepts/ranges and a few library bits. Confirm `-fno-exceptions /
  -fno-rtti` is viable (the ECS may use `dynamic_cast`/exceptions — relax the flags if so).
- **STL on PS3.** `std::vector/map/string/shared_ptr` should work via libstdc++, but watch
  binary size and heap use; the ECS allocates per-entity.
- **SFML shim fidelity.** `sf::View`, sub-rect sprites, and text metrics must match closely
  enough that levels line up; verify against the original visually.
- **Audio conversion.** MikMod plays modules/PCM, not arbitrary formats — convert on the
  host (see `docs/PATTERNS.md` §5).

## Reference

- Original game: `mega-mario` (sibling checkout) — keep `bin/assets/` + `.txt` configs.
- Conventions/toolchain: `docs/PATTERNS.md`, `ghcr.io/02900/ps3-toolchain:latest`,
  and the sister port [02900/ki-blast-arena](https://github.com/02900/ki-blast-arena).
