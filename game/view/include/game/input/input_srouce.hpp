#pragma once

#include "game/input/player_input_stream.hpp"
#include "game/input/window_ctx.hpp"
#include "oge/input/raw_input_stream.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/staged_scheduler.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game::input
{
using oge::input::RawInputStream;
using oge::runtime::AnythingFactory;
using oge::runtime::FramePipeline;
using oge::runtime::oge_id_type;
using oge::runtime::OGEContext;
using oge::runtime::Stage;

struct InputContext
{
    WindowCtx& windowCtx;
    PlayerInputStream& player;
    entt::registry& uiWorld;
};

struct InputFrame
{
    float dt;
    const RawInputStream& raw;
};

struct FInputContext
{
    float dt;
    const RawInputStream& raw;
    PlayerInputStream& player;
    entt::registry& uiWorld;

    FInputContext(InputFrame& f, InputContext& ctx) : dt(f.dt), raw(f.raw), player(ctx.player), uiWorld(ctx.uiWorld) {}
};

class InputSource : public Stage<InputContext, FInputContext>
{
   public:
    InputSource(oge_id_type id) : Stage<InputContext, FInputContext>(id) {}
};

class InputPipeline : public FramePipeline<InputSource, InputFrame>
{
   public:
    InputPipeline(InputContext& state, AnythingFactory& af) : FramePipeline<InputSource, InputFrame>(state, af) {}
};
};  // namespace game::input