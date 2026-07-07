# PS3 Mega Mario

A PlayStation 3 homebrew port of **[mega-mario](https://github.com/Terpodia/mega-mario)** —
a 2D platformer (Mario × Mega Man) written in **C++** with an **Entity Component System**
and a factory pattern, originally rendered with **SFML**.

Because the original is already C++, the ECS / game logic ports largely intact; the work
is replacing the **SFML layer** (graphics / audio / input / window) with PS3 backends —
**ya2d** + **Tiny3D** (2D sprites), the PSL1GHT **pad** API, **MikMod** (audio), and
**Clay** (menu UI) — behind a thin SFML-compatibility shim.

> ## ✅ Status: playable (all 9 roadmap phases implemented)
> The full game runs on PS3 / RPCS3: the SFML **ECS compiles unchanged** behind the shim,
> levels render and scroll, physics & tile collisions work, the menu is built natively in
> **Clay**, music + SFX are **synthesized in code** via MikMod, and `make pkg` produces an
> **XMB-installable PKG**. The phase-by-phase log is in **[todo/ROADMAP.md](todo/ROADMAP.md)**;
> the reusable PS3/PSL1GHT conventions are in **[docs/PATTERNS.md](docs/PATTERNS.md)**.
>
> **Known issue:** an intermittent ~1-frame render glitch (a sprite jumps / some tiles vanish
> for a single frame every few seconds) — see ROADMAP Phase 9 and
> [`ideas/session-recorder-diagnostic.md`](ideas/session-recorder-diagnostic.md).

---

## Building

You need the PSL1GHT toolchain. The easiest way is the prebuilt Docker image — no local
install, works on macOS/Windows/Linux. Mount the project at `/src` and run a command
inside the image:

```bash
# Build  ->  produces src.elf / src.self in the project root
docker run --rm -v "$PWD":/src -w /src ghcr.io/02900/ps3-toolchain-tiny3d make

# Build an installable PKG (for the XMB)
docker run --rm -v "$PWD":/src -w /src ghcr.io/02900/ps3-toolchain-tiny3d make pkg
```

Or use the helper wrappers (they auto-retry the toolchain's transient emulation segfaults):

```bash
./scripts/build.sh            # build
./scripts/build.sh pkg        # installable PKG
./scripts/build.sh clean      # clean
PS3_IP=192.168.1.13 ./scripts/deploy.sh   # ps3load to a console running PS3LoadX
```

> **Platform notes**
> - **Apple Silicon:** add `--platform linux/amd64` to every `docker run` (or set
>   `DOCKER_DEFAULT_PLATFORM=linux/amd64`; the helper scripts rely on that).
> - **Windows:** run from a **WSL2** shell.   **Linux:** prefix with `sudo` if needed.

Outputs are named after the mount dir (`/src`), so they are `src.elf` / `src.self` / `src.pkg`.

## Playing

- **RPCS3:** *File → Install .pkg* → pick `src.pkg` → launch **Mega Mario** from the game list.
  (You can also boot `src.self` directly, but the PKG gives you the XMB icon + entry.) Enable
  audio (Cubeb/XAudio2) to hear the music/SFX.
- **Real PS3 (HEN/CFW):** install `src.pkg` from the XMB, or `ps3load` the `.self` (see
  `scripts/deploy.sh`).

**Controls** — boots to a Clay level-select menu, then into the level:

| Input | Menu | In level |
| --- | --- | --- |
| D-pad / left stick | ↑/↓ select | ←/→ move |
| ✕ Cross | — | jump |
| ○ Circle | choose level | — |
| □ Square | — | shoot |
| △ Triangle | — | pause |
| Start | exit | back to menu |
| L1 / R1 | — | debug: collision boxes / grid |

## Toolchain & libraries

Built against the toolchain image's libraries: **ya2d** (2D sprites/textures), **Tiny3D**
(2D mode / RSX), **FreeType** (TTF text), **pngdec**/**jpgdec** (image decode), **MikMod**
(audio), plus the PSL1GHT pad/audio/sysutil APIs and the **Clay** UI layout engine
(`extern/clay-ps3`). This is a 2D game — no 3D pipeline needed.

## Project structure

```
ps3-mega-mario/
├── .github/workflows/   # CI: build (via toolchain image) + docs link lint
├── source/              # C++ game source (PPU): ECS + SFML shim backend, Clay menu, audio
├── include/             # Shared headers (the SFML-compat shim: SFML/*.hpp)
├── data/                # Embedded assets (bin2o): sprites + level/asset .txt configs
├── pkgfiles/            # PKG payload: ICON0.PNG (assets/ is empty — everything is embedded)
├── extern/              # External deps (Clay UI: extern/clay-ps3)
├── .claude/skills/      # Submodule: ps3-homebrew patterns, as Claude skills (ps3-homebrew-skills)
├── docs/                # PATTERNS.md (now a pointer to the skills submodule) + api/ notes
├── scripts/             # Dockerized build.sh / deploy.sh wrappers
├── todo/                # → ROADMAP.md: the migration plan
├── ideas/               # Parked design notes (e.g. the glitch session-recorder)
├── Makefile             # PSL1GHT build (C++ gnu++17 + C gnu99 for Clay/audio)
├── sfo.xml              # Application metadata (TITLE_ID: MEGAMARIO)
└── README.md
```

## Roadmap & conventions

- **[todo/ROADMAP.md](todo/ROADMAP.md)** — the phase-by-phase migration plan.
- **[`.claude/skills/ps3-homebrew/`](https://github.com/02900/ps3-homebrew-skills)** — the
  reusable PS3/PSL1GHT patterns & gotchas (pad input, colour formats, Clay UI, audio, porting),
  vendored as a shared submodule **and** a set of Claude Code skills. Run
  `git submodule update --init` to fetch it. (`docs/PATTERNS.md` is now just a pointer here.)

## Credits

- Original game: **[Terpodia/mega-mario](https://github.com/Terpodia/mega-mario)** (C++/SFML).
- Toolchain & scaffold conventions: [02900/ps3-toolchain](https://github.com/02900/ps3-toolchain),
  [02900/ps3-homebrew-template](https://github.com/02900/ps3-homebrew-template),
  [02900/ki-blast-arena](https://github.com/02900/ki-blast-arena).

## License

MIT.
