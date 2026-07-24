#pragma once

#include "game/components.hpp"
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
        float hfov;
        float vfov;
        union
        {
            struct
            {
                entt::entity viewWidget;
                entt::entity moveWidget;
            } widgetInput;
            struct
            {
                size_t mouseIdx;
            } mouseIuput;
        };
    };
};

void RegisterInputSources(AnythingFactory& af);

class UIDragInput : public InputSource
{
    std::array<entt::entity, RawInputStream::PtrInputCount> activeDrags;
    RawInputStream::Cursor raw_idx = {};

   public:
    DECL_ID(UIDragInput);

    static std::unique_ptr<InputSource> Build(const Def& def, AnythingFactory& af)
    {
        return std::make_unique<UIDragInput>();
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
    float vfov;
    float hfov;
    RawInputStream::Cursor raw_idx = {};
    bool isDigging = false;

   public:
    DECL_ID(WidgetInput);
    WidgetInput(const Def& def) : out(*def.target)
    {
        viewWidget = def.widgetInput.viewWidget;
        moveWidget = def.widgetInput.moveWidget;
        vfov = def.vfov;
        hfov = def.hfov;
    }

    static std::unique_ptr<InputSource> Build(const Def& def, AnythingFactory& af)
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
    float vfov;
    float hfov;
    RawInputStream::Cursor raw_idx = {};

   public:
    DECL_ID(KeyMouseInput);
    KeyMouseInput(const Def& def) : InputSource(), out(*def.target)
    {
        mouseIdx = def.mouseIuput.mouseIdx;
        vfov = def.vfov;
        hfov = def.hfov;
    }

    static std::unique_ptr<InputSource> Build(const Def& def, AnythingFactory& af)
    {
        return std::make_unique<KeyMouseInput>(def);
    }

    void onAttach(InputContext& ctx);
    void onDetach(InputContext& ctx);
    void onUpdate(FInputContext& ctx);
};

class InputPipeline : public FramePipeline<InputSource, InputFrame>
{
    InputContext m_input;
   public:
    InputPipeline(InputContext&& state, AnythingFactory& af) : m_input(state), FramePipeline<InputSource, InputFrame>(m_input, af) {}
};
};  // namespace game::input