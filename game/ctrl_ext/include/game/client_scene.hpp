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
    oge_id_type nextSene;
public:
    ClientScene(const Def& def) : SceneExt(def)
    {
        auto it = def.args.find("next_scene");
        assert(it != def.args.end());
        nextSene = std::get<int64_t>(it->second);
    }

    
};
} // namespace OneGame