# EXWORLD — Vision vs. Reality Audit (2026-10-29)

Full-codebase read of exgine (~7,000 lines) and exworld (~2,100 lines), line-by-line on
every system that touches what the player sees. Written because the game on screen does
not match the game in our heads. This document is the honest bridge between the two.

---

## 1. What we wanted (the reference shots)

The target aesthetic, from the owner's reference images:

- A **living coastal city**: streets with real road surfaces and markings, varied
  building facades (windows that read as windows, color variation, rooflines), parked
  and driving traffic, street furniture, palm trees and vegetation.
- **Characters that read as people**: clothing colors, skin tones, hair, poses.
- **Cars that read as cars**: distinct paint, correct rotation while steering, wheels.
- **Atmosphere**: sky gradient, sun with warm/cool shifts across the day, fog for depth,
  shadows, water that shimmers against the skyline.
- A GTA-style open loop: drive anywhere, get out, enter buildings, evade police.

## 2. What actually ships today (verified, file:line)

The current APK renders: flat gray land, flat blue water, 22 identical gray boxes
(one shared baked mesh), gray capsule people frozen in T-pose, gray cars that slide
without turning, a hardcoded flat blue sky, no shadows, no fog, no roads, no trees,
no readable HUD. Root causes, in order of visual impact:

1. **The gray-fallback material bug** — `exgine/src/material.cpp` defines 11 materials;
   every other name (`terrain`, `skin`, `fabric`, `asphalt`, `paint`, `hud`, …) falls
   into `base_color = {0.5,0.5,0.5}`. Nearly every surface in the world is the same gray.
2. **The baked-building override** — hydration generates real procedural buildings
   (walls with door gaps, window glass, floors, stairs — `exgine/src/building.cpp`),
   then `exworld/src/visuals.cpp` overwrites every building with one 14×15×12 box shell.
3. **Invisible vegetation** — `ProceduralWorld::generate_chunk` scatters vegetation
   instances per biome (`exgine/src/world.cpp:22`); no code ever builds meshes for them.
4. **Rotation never reaches the screen** — physics simulates vehicle turning, but
   `Runtime::update` syncs entities with `set_entity_position(..., rotation={0,0,0})`
   (`exgine/src/runtime_clean.cpp:21`). Cars slide sideways through corners like ice
   cubes. Characters also never face their movement direction.
5. **Double render bridge** — two `WorldRenderBridge` instances stream/sync the same
   chunks every frame (`exworld/src/world.cpp:56` and `exworld/src/camera.cpp:63`);
   duplicated terrain draws, water quads, and physics heightfields.
6. **No atmosphere** — sky is a fixed clear color; `FogSettings` and a 256-light shader
   exist but nothing binds them; no shadow map; `EnvironmentSystem` computes a full
   day/night cycle that nothing pipes into lighting.
7. **No roads, no city layout** — "dense city" in the README corresponds to 22
   hand-placed coordinates in a scene file; `downtown.exg` declares road parameters
   that zero code reads.
8. **Animation deliberately disabled** — `exworld/src/animation_driver.cpp` refuses to
   attach a skeleton ("makes EVERY frame fail") — so the walk/run clips the engine can
   generate never play.

Honest scoring of the README claims: physics ✓ real; streaming ✓ real (terrain-only);
vehicles ✓ simulated / ✗ visually frozen; police ~ 4 straight-line walkers; save ✓
implemented / ✗ unreachable (no code calls it); HUD ✗ data with no renderer.

## 3. What we fixed in this pass (the "make it not gray" commit)

- Material library expanded (terrain per biome, asphalt, skin/hair/fabric/denim/leather
  with per-seed tone variation, car `paint` with per-seed liveries, foliage/trunk,
  brick + wall-color variants for buildings, hud, watch/backpack). The gray-fallback
  bug is dead for every name the game actually uses.
- Buildings: the baked-box override is removed; every building now shows its own
  procedural geometry (door gaps, window glass, per-seed wall colors and floors).
- Vegetation bridge: streamed chunks now build merged low-poly tree/bush meshes from
  the vegetation instances that already existed — forests and palms become visible.
- Terrain: per-chunk material chosen from the biome sampler (grass plains, forest
  floor, desert sand, snow, rock, beach, wetland) instead of one gray slab.
- Vehicles: physics yaw now syncs to the scene — cars visibly steer and rotate.
  Body material is per-seed paint, so traffic has liveries.
- Characters: entities rotate toward their movement heading; skin/shirt/pants/shoes
  colors vary per person.
- Sky + fog: day/night cycle wired to lighting (sun direction, elevation-based color
  temperature, sky and ambient shift, distance fog matched to the sky color).
- The duplicate render bridge is removed (one streamer, one set of chunks).

## 4. What is still missing (next passes, ranked)

1. Roads + city blocks — the one genuinely new system (grid flattening on the
   heightfield, lane markings, sidewalks; `downtown.exg` already declares the params).
2. Shadow map for the directional sun (the renderer's biggest depth-cue gap).
3. Character animation — attach the existing skeleton/clips and remove the bail-out.
4. Readable HUD — 2D overlay with a font atlas; wanted stars, speed, prompts, minimap.
5. Police escalation that matters — spawn-on-heat, car pursuit, arrest consequence.
6. Save/load trigger (autosave + pause menu) — the system already exists.
7. Facade depth: window holes instead of glass-on-wall, balconies, roof hardware.
8. Wheels that spin + steer visually; camera obstruction raycast.

## 5. The rule going forward

A milestone is not "code exists". A milestone is "a screenshot of the running game
shows it". The Phase 61–95 tracks that exist as math files disconnected from the
render path stay disconnected until the renderer actually exercises them.
