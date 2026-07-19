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
    entt::registry& uiWorld;

    FInputContext(InputFrame& f, InputContext& ctx) : dt(f.dt), raw(f.raw), uiWorld(ctx.uiWorld) {}
};

class InputSource : public Stage<InputContext, FInputContext>
{
   public:
    struct Def
    {
        PlayerInputStream* target;
        union
        {
            struct
            {
                entt::entity viewWidget;
                entt::entity moveWidget;
            } widgetInput;
            size_t mouseIuput;
        };
    };
    InputSource(oge_id_type id) : Stage<InputContext, FInputContext>(id) {}
};

class UIDragInput : public InputSource
{
    std::array<entt::entity, RawInputStream::PtrInputCount> activeDrags;
    RawInputStream::Cursor raw_idx;
   public:
    DECL_ID(UIDragInput);
    UIDragInput(const Def& def) : InputSource(Id)
    {
    }

    void onAttach(InputContext& ctx);
    void onDetach(InputContext& ctx);
    void onUpdate(FInputContext& ctx);
};

class WidgetInput : public InputSource
{
    PlayerInputStream& out;
    entt::entity viewWidget;
    entt::entity moveWidget;
    RawInputStream::Cursor raw_idx;

   public:
    DECL_ID(WidgetInput);
    WidgetInput(const Def& def) : InputSource(Id), out(*def.target)
    {
        viewWidget = def.widgetInput.viewWidget;
        moveWidget = def.widgetInput.moveWidget;
    }

    std::unique_ptr<InputSource> Build(const Def& def, AnythingFactory& af)
    {
        return std::make_unique<WidgetInput>(def);
    }

    void onAttach(InputContext& ctx);
    void onDetach(InputContext& ctx);
    void onUpdate(FInputContext& ctx);
};

class KeyMouseInput : public InputSource
{
    PlayerInputStream& out;
    size_t mouseIdx;
    RawInputStream::Cursor raw_idx;

   public:
    DECL_ID(KeyMouseInput);
    KeyMouseInput(const Def& def) : InputSource(Id), out(*def.target)
    {
        mouseIdx = def.mouseIuput;
    }

    std::unique_ptr<InputSource> Build(const Def& def, AnythingFactory& af)
    {
        return std::make_unique<KeyMouseInput>(def);
    }

    void onAttach(InputContext& ctx);
    void onDetach(InputContext& ctx);
    void onUpdate(FInputContext& ctx);
};

class InputPipeline : public FramePipeline<InputSource, InputFrame>
{
   public:
    InputPipeline(InputContext& state, AnythingFactory& af) : FramePipeline<InputSource, InputFrame>(state, af) {}
};
};  // namespace game::input