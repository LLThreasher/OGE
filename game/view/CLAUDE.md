# game/view — Rendering Module

## Overview

The `game/view` module provides the rendering layer for the game engine.  It follows a
command-buffer / render-pass architecture where game logic emits typed *commands* into a
submission queue, and *passes* consume those commands to produce GPU draw calls.

## Directory layout

```
game/view/
├── shaders/                              — GLSL shader sources
│   ├── mesh.vert / mesh.frag             — General 3D mesh (vertex color, UV, normals)
│   └── gizmo.vert / gizmo.frag           — Wireframe gizmo rendering
├── include/game/view/
│   ├── renderer.hpp                      — Renderer base class, RenderPipeline
│   ├── submission_queue.hpp              — SubmissionQueue (typed command groups)
│   └── gfx/
│       ├── commands.hpp                  — Core draw commands
│       ├── gizmo_commands.hpp            — Gizmo commands (wire cube, wire rect)
│       ├── gizmo_pass.hpp                — GizmoPass (wireframe renderer)
│       ├── mesh_pass.hpp                 — GeneralMeshPass (3D mesh renderer)
│       ├── ui_pass.hpp                   — UIPass (sprite / UI rendering)
│       ├── terrain_pass2.hpp             — TerrainPass2 (voxel terrain)
│       ├── panel_sprite.hpp              — PanelSprite (9-slice panel component)
│       └── view_executor.hpp             — View executor
│   ├── input/                            — Input handling
│   └── terrain/                          — Terrain renderer
└── CLAUDE.md                             — This file
```

## Architecture

```
┌──────────────┐    commands    ┌──────────────┐    draws    ┌──────────────┐
│  Game Logic  │ ──────────────→│ SubmissionQueue│ ──────────→│  GPU Backend │
│  (subsystem) │                │ (per-view)     │            │  (Vulkan)    │
└──────────────┘                └──────────────┘            └──────────────┘
                                       ↑
                                 ┌─────┴─────┐
                                 │   Passes   │
                                 │ (consume   │
                                 │  commands) │
                                 └───────────┘
```

### Commands

Commands are plain structs defined in `commands.hpp` and `gizmo_commands.hpp`.  They carry
the data needed to issue a draw call.  Adding a new draw type means:

1. Define the command struct
2. Create a Pass that inherits `Pass<YourCommand>`
3. Add the command type to the `SubmissionQueue` typedef in `submission_queue.hpp`

### Passes

Each pass inherits from `oge::runtime::gfx::Pass<CmdType...>` and implements:
- `onAttach(InitDrawContext&)` — create pipelines, binding layouts
- `onDetach(InitDrawContext&)` — cleanup
- `onUpdate(DrawContext&, View, pushConstant)` — consume commands, emit draws

Passes are tagged with either `RequiresVPTransform` (3D: receives view-projection matrix)
or `RequiresScreenAffine` (2D: receives screen-space transform).

## Shaders

Shaders are written in GLSL 450 (Vulkan-compatible).  Key conventions:

| Shader | Purpose | Inputs | Uniforms |
|---|---|---|---|
| `mesh.vert` | 3D mesh vertex | position, normal, UV, color | model+MVP+normalMatrix UBO |
| `mesh.frag` | 3D mesh fragment | worldPos, normal, UV, color | texture, light UBO (direction, ambient, diffuse) |
| `gizmo.vert` | Wireframe vertex | position, color | MVP UBO |
| `gizmo.frag` | Wireframe fragment | color | — (pass-through) |

## Passes

### GeneralMeshPass (`mesh_pass.hpp`)
Renders `CmdDrawGeneralMeshOpaque` commands.  Batches draws by texture.  Supports:
- Vertex color (`inColor`)
- UV texturing (`inUV` → `uTexture`)
- Normal-based directional lighting (`uLightDir`, `uAmbientColor`, `uLightColor`)

Pipeline: depth test + depth write enabled, backface culling.

### GizmoPass (`gizmo_pass.hpp`)
Renders `CmdDrawWireCube` and `CmdDrawWireRect` commands.  Generates wireframe geometry
on the CPU each frame.  Uses `VK_POLYGON_MODE_LINE` for wireframe rendering.

### UIPass (`ui_pass.hpp`)
Renders `CmdDrawSprite` commands.  2D screen-space rendering with push-constant
transforms.

### TerrainPass2 (`terrain_pass2.hpp`)
Renders `CmdDrawTerrainMeshOpaque` commands.  Voxel terrain with block textures and
palette-based coloring.

## Components

### PanelSprite (`panel_sprite.hpp`)
Renders a 9-slice panel.  Splits a source sprite into a 3×3 grid and emits 9
`CmdDrawSprite` commands — corners keep their original size, edges stretch to fill.

```cpp
PanelSprite panel{};
panel.sourceSprite = PSprite{texture, {0, 0, 48, 48}};
panel.targetRect   = IRect{100, 100, 300, 200};
panel.margin       = Margin{8, 8, 8, 8};  // left, right, top, bottom

std::pmr::vector<CmdDrawSprite> cmds;
PanelSprite::EmitCommands(panel, cmds);
// cmds now contains 9 draw commands
```

## Adding a new pass

1. Create shaders in `shaders/`
2. Define commands in `gfx/commands.hpp` (or a new header)
3. Create the pass header in `gfx/`
4. Add the command type to `SingleSubmissionQueue` in `submission_queue.hpp`
5. Register the pass in the renderer registration function
