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

### Phase 4 — Input ⬜
- Map the DualShock to the game's `Action` system (`sf::Keyboard` → pad), using the
  retained-packet + edge/level patterns (`docs/PATTERNS.md` §2).
- **Exit criteria:** menu navigation + in-game move/jump respond to the pad.

### Phase 5 — Rendering & camera ⬜
- Sprites, animations, and the **2D scrolling camera** (`sf::View` → `screen = world −
  camera`, cull off-screen); the tilemap/level draw in `Scene_Play`.
- **Exit criteria:** a level renders and scrolls with the player.

### Phase 6 — Physics & gameplay ⬜
- Verify the ECS `Physics` (AABB overlap, gravity, jump, tile collisions) and entity
  factory behave on hardware; tune the fixed timestep.
- **Exit criteria:** Mario runs, jumps, collides, and interacts with the level.

### Phase 7 — UI / HUD / menu ⬜
- `Scene_Menu` and the in-game HUD (score / coins / lives / time) built **entirely in
  Clay** (`docs/PATTERNS.md` §3.5), not hand-drawn.
- **Exit criteria:** a working menu → play → (game over / win) → menu loop.

### Phase 8 — Audio ⬜
- Music + SFX via **MikMod** (convert the original audio to module/PCM; jump, coin, stomp,
  level-clear). Defensive init (`docs/PATTERNS.md` §5.5).
- **Exit criteria:** background music + core SFX.

### Phase 9 — Packaging & polish ⬜
- `ICON0.PNG`, `make pkg` for XMB install, performance pass, docs.
- **Exit criteria:** an installable PKG that runs from the XMB.

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
