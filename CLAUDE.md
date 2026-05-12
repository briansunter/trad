# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

TRAD Strike is a Raiden-inspired vertical shooter written in C99. It targets both native (macOS/Linux) and WebAssembly (browser) via [Sokol](https://github.com/floooh/sokol) for graphics/input and a header-only ECS library (`c-ecs`) for gameplay entities.

## Commands

```bash
make native       # Build native binary to build/trad
make web          # Build WASM to dist/
make run          # Build and run native
make serve        # Build web and serve at http://127.0.0.1:8000 (cache-busting dev server)
make assets       # Regenerate src/generated_assets.{h,c} from OBJ files
make clean        # Remove build/ and dist/
make all          # Build both native and web
```

**Dev environment:** `nix develop` drops into a shell with clang, emscripten, and python3. Without Nix, provide those yourself.

**Web dev server:** `make serve` uses `tools/serve_no_cache.py` which sends no-cache headers and appends a version query param to break browser caches.

## Architecture

All game code lives in `src/main.c` (~1400 lines). There are no other hand-written source files.

### Asset pipeline

`tools/obj_to_c.py` converts OBJ meshes from `assets/vendor/kenney-space-kit/` into `src/generated_assets.{h,c}`. It bakes per-vertex lighting at conversion time so the renderer does zero lighting math at runtime. The Makefile runs this script automatically when OBJ files change. `generated_assets.c` defines `trad_meshes[]` (array of `TradMesh`) and the `TradMeshId` enum used throughout `main.c`.

### ECS layout

`main.c` uses `c-ecs` with a flat, stack-allocated world. Components are plain structs (`Position`, `Velocity`, `Collider`, `Health`, `Lifetime`, `Damage`, `Pickup`, `Renderable`, `EnemyBrain`). Tags (`PlayerTag`, `EnemyTag`, `PlayerShotTag`, `EnemyShotTag`, `ParticleTag`, `PickupTag`) identify entity types. The `Game` struct holds the world, schedule, input state, starfield, and game counters.

### System execution order (registered in `init`)

1. `spawn_system` — enemy/pickup wave spawning on timers
2. `player_system` — input, movement, weapon fire
3. `enemy_system` — AI steering and firing
4. `movement_system` — applies velocities
5. `collision_system` — projectile/entity hit detection
6. `lifetime_system` — decrements timers, marks expired entities
7. `cleanup_system` — removes dead entities from world

### Rendering

`draw_world()` renders the 3D isometric scene (parallax starfield, scenery, all entities). `draw_ui()` renders the 2D HUD and touch controls. Both use Sokol GL immediate-mode (`sgl_*`). Two pipelines exist: opaque and alpha-blended (used for transparent/glowing objects).

### Input

Desktop: WASD/arrows for movement, Space/Z/X to fire, R to restart, mouse aim. Mobile: left-half touch for d-pad movement, right-half for fire button.

### Platform backends

- macOS: Objective-C entry point, Metal/MetalKit frameworks (`-framework Metal -framework MetalKit`)
- Linux: OpenGL + X11 (`-lGL -lX11`)
- Web: Emscripten with WebGL2 (`USE_WEBGL2=1`), no filesystem, memory growth enabled

## Agent skills

### Issue tracker

Issues live as local markdown files under `.scratch/`. See `docs/agents/issue-tracker.md`.

### Triage labels

Default label strings (`needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`). See `docs/agents/triage-labels.md`.

### Domain docs

Single-context layout: one `CONTEXT.md` + `docs/adr/` at the repo root. See `docs/agents/domain.md`.
