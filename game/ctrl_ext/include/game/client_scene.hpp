#pragma once

#include "game/game_world.hpp"
#include "game/json.hpp"
#include "game/scene_ext.hpp"
#include "oge/runtime/net_serializer.hpp"

namespace game
{
namespace net = oge::runtime::net;

struct ClientArgs
{
    std::string ip = "127.0.0.1";
    uint16_t port = 25567;
    uint32_t timeout = 5000;
};

enum class ClientState
{
    WaitingConnect,
    WaitingConfig,
    ReceivedConfig,
    Available,
    Disconnected,
};

class ClientScene : public SceneExt
{
};

class ClientConnScene : public SceneExt
{
    oge_id_type m_nextSene;

   public:
    ClientConnScene(const Def& def) : SceneExt(def)
    {
        auto it = def.args.find("next_scene");
        if (it == def.args.end())
        {
            m_nextSene = Id<ClientScene>();
        }
        else
        {
            m_nextSene = std::get<int64_t>(it->second);
        }
    }

    void Update(Frame f, SceneContext sctx) override
    {
        SceneExt::Update(f, sctx);
    }
};
}  // namespace game

namespace oge::runtime {
template<>
struct TypeName<game::ClientScene>
{
    static consteval std::string_view Get()
    {
        return "core::ClientScene";
    }
};

template<>
struct TypeName<game::ClientConnScene>
{
    static consteval std::string_view Get()
    {
        return "core::ClientConnScene";
    }
};
}
