# PS3 Mega Mario

A PlayStation 3 homebrew port of **[mega-mario](https://github.com/Terpodia/mega-mario)** —
a 2D platformer (Mario × Mega Man) written in **C++** with an **Entity Component System**
and a factory pattern, originally rendered with **SFML**.

Because the original is already C++, the ECS / game logic ports largely intact; the work
is replacing the **SFML layer** (graphics / audio / input / window) with PS3 backends —
**ya2d** (2D sprites), the PSL1GHT **pad** API, **MikMod** (audio), and **Clay** (UI/HUD) —
behind a thin SFML-compatibility shim.

> ## 🚧 Status: scaffold
> This repo currently contains **only the project skeleton and the migration roadmap** —
> it is **not yet playable**. `source/main.cpp` is a placeholder. The step-by-step plan
> lives in **[todo/ROADMAP.md](todo/ROADMAP.md)**, and the hard-won PS3 conventions
> (carried over from a prior PS3 port) are in **[docs/PATTERNS.md](docs/PATTERNS.md)** —
> read it before adding code.

---

## Building

You need the PSL1GHT toolchain. The easiest way is the prebuilt Docker image — no local
install, works on macOS/Windows/Linux. Mount the project at `/src` and run a command
inside the image:

```bash
# Build  ->  produces src.elf / src.self in the project root
docker run --rm -v "$PWD":/src -w /src ghcr.io/02900/ps3-toolchain make

# Build an installable PKG (for the XMB)
docker run --rm -v "$PWD":/src -w /src ghcr.io/02900/ps3-toolchain make pkg
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

Outputs are named after the mount dir (`/src`), so they are `src.elf` / `src.self`.

## Toolchain & libraries

Built against the toolchain image's libraries: **ya2d** (2D sprites/textures), **Tiny3D**
(2D mode / RSX), **FreeType** (TTF text), **pngdec**/**jpgdec** (image decode), **MikMod**
(audio), plus the PSL1GHT pad/audio/sysutil APIs and the **Clay** UI layout engine
(`extern/clay-ps3`). This is a 2D game — no 3D pipeline needed.

## Project structure

```
ps3-mega-mario/
├── .github/workflows/   # CI: build (via toolchain image) + docs link lint
├── source/              # C++ game source (PPU) — main.cpp (stub for now)
├── include/             # Shared headers (the SFML shim lands here)
├── data/                # Embedded assets (bin2o): sprites, fonts, level/anim configs
├── pkgfiles/            # Files bundled into the PKG (ICON0.PNG, assets/)
├── extern/              # External deps (Clay UI submodule)
├── docs/                # PATTERNS.md (PS3 homebrew conventions) + api/ notes
├── scripts/             # Dockerized build.sh / deploy.sh wrappers
├── todo/                # → ROADMAP.md: the migration plan
├── Makefile             # PSL1GHT build (C++ / gnu++17)
├── sfo.xml              # Application metadata (TITLE_ID: MEGAMARIO)
└── README.md
```

## Roadmap & conventions

- **[todo/ROADMAP.md](todo/ROADMAP.md)** — the phase-by-phase migration plan.
- **[docs/PATTERNS.md](docs/PATTERNS.md)** — reusable PS3/PSL1GHT patterns & gotchas
  (pad input, colour formats, Clay UI, audio, packaging) from a prior port.

## Credits

- Original game: **[Terpodia/mega-mario](https://github.com/Terpodia/mega-mario)** (C++/SFML).
- Toolchain & scaffold conventions: [02900/ps3-toolchain](https://github.com/02900/ps3-toolchain),
  [02900/ps3-homebrew-template](https://github.com/02900/ps3-homebrew-template),
  [02900/ki-blast-arena](https://github.com/02900/ki-blast-arena).

## License

MIT.
