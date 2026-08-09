# Contribution Guide

This guide covers the five most common "add a thing" workflows in OGE:

1. [Adding a block](#adding-a-block)
2. [Adding a component](#adding-a-component)
3. [Adding a subsystem](#adding-a-subsystem)
4. [Adding a renderer](#adding-a-renderer)
5. [Adding a pass](#adding-a-pass)

Each section lists the exact files to touch, in order, plus a checklist of
common mistakes.

---

## Adding a block

Blocks are declared in scene config or registered directly in a scene's
`Load()`.  The registry itself is `game::terrain::BlockRegistry`
(`game/data/include/game/terrain/block_registry.hpp`).

### 1. Register the block

In a scene's `Load()` (see `game/ctrl/include/game/server_scene.hpp:227-236`):

```cpp
auto& blocks = m_world.ctx().emplace<::game::terrain::BlockRegistry>();
blocks.RegisterBlock("dirt", {
    "Dirt",          // display name
    "dirt.png",      // texture
    1,               // block flags (1 = opaque)
});
```

Or in the shared default config (`GetDefaultSceneConfig` in
`game/ctrl/src/scene.cpp:70`), which every scene based on it inherits:

```cpp
config.blocks = {
    {"dirt",  {"Dirt",  "dirt.png",  1}},
    {"wood",  {"Wood",  "wood_plank.png", 1}},
    {"stone", {"Stone", "green_stone.png", 1}},
};
```

### 2. `BlockConfig` fields

| Field | Meaning |
|---|---|
| `blockDisplayName` | Human-readable name |
| `textureSlotPerFace` | 6 textures (or 1, applied to all faces) |
| `blockFlags` | `1` = opaque (`BLOCK_FLAG_OPAQUE_TO_MESHER`), `0` = transparent/air |
| `aabbs` | Collision boxes; default `DEFAULT_BLOCK_AABB` (unit cube) |

`air` is registered by the `BlockRegistry` constructor
(`game/data/src/terrain/block_registry.cpp:29`) — do not register it yourself.

### 3. Make sure the texture exists

The texture file must be present in the assets directory (e.g. `assets2/`) —
it's copied by `copy_assets` in `cmake/utils.cmake`.  Missing textures log a
warning and fall back to `invalid.png`.

### Checklist

- [ ] Block id name unique across the scene
- [ ] Texture file shipped in assets
- [ ] If you want the block spawnable in terrain generation, wire it into the
      terrain generator (see `game/sim/src/terrain/terrain_generator.cpp`)

---

## Adding a component

Components are plain structs stored in EnTT registries.  They may optionally
be network-replicated and JSON-serializable.

### 1. Define the struct

In `game/data/include/game/components.hpp` (or a module-local header):

```cpp
struct ComponentTargetBlock
{
    Point3 hitPos = {};
    bool valid = false;
};
```

### 2. Declare its type name

Immediately after the struct, in the same header (never a separate file):

```cpp
DECL_TYPE_NAME(game::ComponentTargetBlock, "core::ComponentTargetBlock")
```

### 3. (Optional) Network replication

If the component must be synced to peers, register it in
`game/ctrl/src/net/replication_registry.cpp` inside `RegisterReplications()`:

```cpp
RegisterComponentEvents<ComponentTargetBlock>(af);
rf.RegisterSnapshotComponent<ComponentTargetBlock>();
```

See `game/ctrl/CLAUDE.md` → "Adding a new component type" for details on the
event hooks and snapshot machinery this wires up.

### 4. (Optional) JSON serialization

Add a `DECL_JSON_OBJ(Type, { visit(...); })` block after the struct definition
for save/load support (see the `AABB` example at the bottom of
`block_registry.hpp`).

### Checklist

- [ ] `DECL_TYPE_NAME` directly after the struct definition
- [ ] Included `<type_name.hpp>` via `oge/runtime/type_name.hpp` where the
      macro is used
- [ ] If replicated: registered in `replication_registry.cpp` (both
      `RegisterComponentEvents` and `RegisterSnapshotComponent`)
- [ ] If serialized: `DECL_JSON_OBJ` present
- [ ] Prefer `OgeRegistry`/`OgeRegistryRef` over raw `entt::registry`

---

## Adding a subsystem

Subsystems are the sim-side per-frame logic (player control, physics,
creatures, terrain).  Each runs in a fixed-step pipeline and/or a realtime
pipeline.

### 1. Declare the class

In `game/sim/include/game/sim/subsystem.hpp` using the provided macros:

```cpp
DECL_SYS(SubsystemMyThing)   // expands to class + onAttach/onDetach/onUpdate
```

For systems with a fixed-step *and* realtime variant, make it a template and
instantiate both (like `SubsystemPlayer`):

```cpp
template <UpdateType utype>
class SubsystemMyThing : public Subsystem
{
    DECL_FNS
};

#undef DECL_FNS

DECL_UTYPES_IMPL(SubsystemMyThing)   // in the .cpp file
```

### 2. Implement in a new `.cpp`

`game/sim/src/subsystem_my_thing.cpp`:

```cpp
#include "game/sim/subsystem.hpp"

namespace game::sim
{
template <UpdateType variant>
void SubsystemMyThing<variant>::onAttach(GameState& ctx) {}

template <UpdateType variant>
void SubsystemMyThing<variant>::onDetach(GameState& ctx) {}

template <UpdateType variant>
void SubsystemMyThing<variant>::onUpdate(FGameState& ctx)
{
    // ctx.world is the game registry; ctx.events the dispatcher
    // Query entities: ctx.world.view<ComponentX, ComponentY>().each()
    // Emit debug text: ShowDebugText(ctx, "frame {}", frameCount);
}

DECL_UTYPES_IMPL(SubsystemMyThing)
}  // namespace game::sim
```

### 3. Register the factory

In `game/sim/src/registry.cpp`:

```cpp
RRU(SubsystemMyThing);   // registers both <Realtime> and <FixedStep>
// or RR(SubsystemMyThing);  // single non-templated system
```

Also add a `TypeName` specialization in `subsystem.hpp` (see
`SubsystemPlayer` at line 156) if you want `AddStage` to find it by id.

### 4. Add it to a scene

In `GetDefaultSceneConfig` (`game/ctrl/src/scene.cpp`), or per-scene in
`m_sceneConfig.subsystems` / `m_sceneConfig.realtimeSubsystems`:

```cpp
config.subsystems.push_back(af.Id<sim::SubsystemMyThing<UpdateType::FixedStep>>());
config.realtimeSubsystems.push_back(
    af.Id<sim::SubsystemMyThing<UpdateType::Realtime>>());
```

The scene's `Load()` runs `m_subsystems.AddStage(m_ctx.any_factory, stage)` for
every id in the config.

### Checklist

- [ ] `DECL_SYS`/`DECL_FNS` + `DECL_UTYPES_IMPL` pairing correct (templated
      systems must be explicitly instantiated)
- [ ] Registered via `RR`/`RRU` in `registry.cpp`
- [ ] `TypeName` specialization present
- [ ] Added to a scene config (`subsystems` = fixed-step, `realtimeSubsystems`
      = per-frame)

---

## Adding a renderer

Renderers are the view-side per-frame logic that *emits draw commands* into the
submission queue.  They never touch the GPU directly.

### 1. Declare the class

In `game/view/include/game/view/renderer.hpp`:

```cpp
class MyRenderer : public Renderer
{
   public:
    void onAttach(RendererState&) override;
    void onDetach(RendererState&) override;
    void onUpdate(FRendererState&) override;
};
```

Plus a `TypeName` specialization in the same file (required by
`AnythingFactory`):

```cpp
template <>
struct TypeName<MyRenderer>
{
    static constexpr std::string Get() { return "core::MyRenderer"; }
};
```

### 2. Implement in `game/view/src/view/my_renderer.cpp`

```cpp
#include "game/view/renderer.hpp"
#include "game/view/submission_queue.hpp"
#include "oge/log.hpp"

namespace game::view
{
void MyRenderer::onAttach(RendererState& ctx)  { LOG_DEBUG("attached"); }
void MyRenderer::onDetach(RendererState& ctx)  { LOG_DEBUG("detached"); }

void MyRenderer::onUpdate(FRendererState& f)
{
    // f.world     — game ECS registry
    // f.uiWorld   — UI ECS registry (ViewPanels live here)
    // f.submissionQueue — where commands go

    for (auto [e, viewPanel] : f.uiWorld.view<ViewPanel>()->each())
    {
        f.submissionQueue.Add<CmdDrawWireCube>(
            viewPanel.activeSlot,            // route to this player's view
            CmdDrawWireCube{myAabb, WHITE});
    }
}
}  // namespace game::view
```

### 3. Register the factory

In `game/view/src/view/registry.cpp`:

```cpp
af.RegisterDerived<Renderer, MyRenderer>();
```

### 4. Add the stage to a scene

In the scene's constructor (`debug_scene.hpp:186-189` pattern):

```cpp
m_renderers.AddStage<view::MyRenderer>(AF());
```

Stages run in registration order — add debug overlays after the main scene
renderers.

### Checklist

- [ ] `TypeName` specialization present (missing it = link error in
      `AddStage`)
- [ ] Registered in `registry.cpp` (`RegisterRenderers`)
- [ ] Stage added in the scene constructor
- [ ] Emits existing command types; if you need a new draw type, see
      [Adding a pass](#adding-a-pass)

---

## Adding a pass

Passes are GPU-side consumers: they take commands out of the submission queue
and issue draw calls.  They own pipelines, binding layouts, and buffers.

### 1. Write shaders

GLSL 450 in `shaders/` — e.g. `mything.vert` / `mything.frag`.  They're
compiled to `*.spv` and `*.opt.spv` automatically by `compile_shaders`
(`cmake/utils.cmake`).  Load them with
`ctx.assets.LoadBlob("mything.vert.opt.spv", desc.vertexShader)`.

### 2. Define a command struct

In a new or existing `game/view/include/game/view/gfx/` header:

```cpp
struct CmdDrawMyThing
{
    Point3 center;
    ColorRGBA8 color = RED;
};
```

### 3. Create the pass class

`game/view/include/game/view/gfx/my_pass.hpp` — model it on `GizmoPass`
(`gizmo_pass.hpp`):

```cpp
class MyPass : public RequiresVPTransform,      // 3D; or RequiresScreenAffine (2D)
               public Pass<CmdDrawMyThing>
{
   public:
    void onAttach(InitDrawContext& ctx);
    void onDetach(InitDrawContext& ctx);
    void onUpdate(DrawContext& ctx, View view, const math::mat4& pvTransform);
   private:
    GPUPipelineHandle m_pipelineHandle;
    GPUBindingGroupLayoutHandle m_bindingLayout;
};
```

`onAttach` creates the binding layout + pipeline; `onUpdate` iterates
`view.Get<CmdDrawMyThing>()`, uploads vertices (use `FrameArena`), binds the
pipeline, pushes constants, and draws.

### 4. Register the command type

Add it to `SingleSubmissionQueue` in
`game/view/include/game/view/submission_queue.hpp`:

```cpp
using SingleSubmissionQueue =
    oge::SubmissionGroup<..., CmdDrawMyThing>;
```

### 5. Add the pass to the view executor

In `game/ctrl_ext/include/game/scene_ext.hpp`, add to the tuple:

```cpp
using ViewExecutor = view::ViewExecutor<TerrainPass2, UIPass, DebugInfoPass,
                                        GizmoPass, MyPass>;
```

Passes draw in tuple order — put 3D world passes before 2D UI overlays.

### Checklist

- [ ] Shaders committed to `shaders/` (auto-compiled, no CMake edits needed)
- [ ] Command struct added to `SingleSubmissionQueue`
- [ ] Pass listed in the `ViewExecutor` tuple in `scene_ext.hpp`
- [ ] `onAttach`/`onDetach` clean up all handles (pipelines, layouts, arenas)
- [ ] Renderer emits your command (see [Adding a renderer](#adding-a-renderer))

---

## Quick reference

| Step | Renderer | Pass | Subsystem |
|---|---|---|---|
| Declare | `renderer.hpp` | `gfx/my_pass.hpp` | `sim/subsystem.hpp` |
| Implement | `src/view/*.cpp` | `src/view/gfx/*.cpp` | `src/*.cpp` |
| Factory | `registry.cpp` | — | `sim/src/registry.cpp` |
| Wire in | scene `AddStage` | `scene_ext.hpp` tuple | scene config lists |
| Type name | `TypeName<>` | — | `TypeName<>` |

**Type-name rule:** every class registered with `AnythingFactory` needs a
`TypeName<T>` specialization — `DECL_TYPE_NAME` for components/events, manual
`template <> struct TypeName<T>` for renderers and subsystems.
