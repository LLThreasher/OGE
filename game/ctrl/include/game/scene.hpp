#pragma once

#include <uuid.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <optional>
#include <span>
#include <variant>
#include <vector>

#include "game/app_context.hpp"
#include "game/components.hpp"
#include "game/game_world.hpp"
#include "game/json.hpp"
#include "game/scene_runner.hpp"
#include "game/sim/subsystem.hpp"
#include "oge/log.hpp"
#include "oge/math.hpp"
#include "oge/platform/io.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game
{
using oge::input::RawInputStream;
using oge::runtime::AnythingFactory;
using oge::runtime::OGEContext;

class Scene : protected AppRuntime
{
   protected:
    entt::registry m_world;
    sim::SubsystemPipeline m_subsystems;
    sim::RealtimeSubsystemPipeline m_realtimeSubsystems;

    SceneConfig m_sceneConfig = {};

   public:
    struct Frame
    {
        float dt;
    };

    struct Def
    {
        AppContext ctx;
        const json::Object& args;
    };

    DECL_ID(Scene)
    Scene(const Def& def);

    virtual ~Scene();
    virtual void Update(Frame f, SceneContext sctx);
    virtual void Load();
    virtual void Unload();

   protected:
    void CreateEntityEventStream();
};

inline PlayerInfo LoadOrCreatePlayer()
{
    std::vector<char> data;
    if (!oge::platform::TryLoadBlob("player.bin", data))
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        uuids::uuid const id = uuids::uuid_random_generator{gen}();
        data.resize(16 + sizeof(math::vec3));
        memcpy(data.data(), id.as_bytes().data(), id.as_bytes().size_bytes());
        math::vec3 pos{20.f, 20.f, 20.f};
        memcpy(&data[16], &pos, sizeof(math::vec3));
        oge::platform::TrySaveBlob("player.bin", data);
    }
    assert(data.size() == 16 + sizeof(math::vec3));
    std::array<uint8_t, 16> _uuid;
    memcpy(_uuid.data(), data.data(), 16);
    math::vec3 pos;
    memcpy(&pos, &data[16], sizeof(math::vec3));
    LOG_INFO("player loaded with uuid {}",
             uuids::to_string(uuids::uuid{_uuid}));
    return {std::move(_uuid), pos};
}

}  // namespace game

namespace oge::runtime
{
template <>
struct TypeName<game::Scene>
{
    static consteval std::string_view Get()
    {
        return "core::Scene";
    }
};
}  // namespace oge::runtime
