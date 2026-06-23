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
  (`MEGAMARIO`), Docker `build.sh`/`deploy.sh`, CI (`build` + `lint`), README, this
  roadmap, and `docs/PATTERNS.md` (carried-over PS3 conventions).

### Phase 1 — Build & toolchain green ⬜
- Add `extern/clay-ps3` as a git submodule.
- Bring the original `src/*.{h,cpp}` into `source/`; **downgrade C++20 → C++17** (replace
  concepts/ranges/etc. as the compiler flags them).
- A minimal `main.cpp` that inits Tiny3D + ya2d and renders a blank frame; **stub the SFML
  shim** (empty `sf::` types) so the ECS code *links*.
- **Exit criteria:** `make` produces a valid `src.self`; boots to a blank screen.

### Phase 2 — SFML compatibility shim ⬜
- Implement the `sf::` shim (`include/SFML/…`) over PS3 backends (see `docs/PATTERNS.md`
  §7): `RenderWindow`→tiny3d clear/flip, `Texture`→`ya2d_Texture`, `Sprite`→textured quad
  (+`IntRect` sub-rect), `View`→2D camera offset, `Font`/`Text`→`ttf_render`/Clay,
  `Color`/`Vector2`/`IntRect`/`RectangleShape` value types.
- **Exit criteria:** the ECS game code compiles & links unchanged against the shim.

### Phase 3 — Asset pipeline ⬜
- Embed the 22 sprites + fonts + `.txt` configs via `bin2o` (`data/`); port
  `Assets.cpp`/`Animation.cpp`/`Texture.h` to load textures from embedded buffers
  (`ya2d_loadPNGfromBuffer`) and parse configs from memory.
- **Exit criteria:** assets load; an animation frame draws on screen.

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
