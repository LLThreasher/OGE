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
    const RawInputStream& raw;
    PlayerInputStream& player;
    entt::registry& uiWorld;
};

struct FInputContext
{
    float dt;
    const RawInputStream& raw;
    PlayerInputStream& player;
    entt::registry& uiWorld;

    FInputContext(float dt, InputContext& ctx) : dt(dt), raw(ctx.raw), player(ctx.player), uiWorld(ctx.uiWorld) {}
};

class InputSource : public Stage<InputContext, FInputContext>
{
   public:
    InputSource(oge_id_type id) : Stage<InputContext, FInputContext>(id) {}
};

class InputPipeline : public FramePipeline<InputSource>
{
   public:
    InputPipeline(InputContext& state, AnythingFactory& af) : FramePipeline<InputSource>(state, af) {}
};
};  // namespace game::input