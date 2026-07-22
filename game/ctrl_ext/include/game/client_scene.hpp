#pragma once
#include "game/graphical_scene.hpp"
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

class DistrubutedGameWorld
{
    struct WorldConfig : net::Object<WorldConfig>
    {
        template<typename F>
        void VisitFields(F&& visitor)
        {
        }
    };
};

class ClientScene
{
    GraphicalScene* child;
public:
    
};
} // namespace OneGame