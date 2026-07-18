#include "game/client.hpp"
#include "oge/graphics/backend.hpp"
#include "oge/graphics/vulkan/create_backend.hpp"

namespace game
{
    void Client::Initialize(WindowHandle* handle)
    {
        m_backend = oge::graphics::vulkan::CreateVulkanBackend();
        m_backend->Initialize(BackendDesc{handle, FrameTimePreference::VSync});
    }

    AppFrameAction Client::Update(float dt, oge::input::RawInputStream& input)
    {
        return AppFrameAction::None;
    }

    void Client::Shutdown()
    {
        m_backend->Shutdown();
    }

    void Client::OnWindowRecreate(WindowHandle*)
    {

    }

    void Client::OnResize(int width, int height)
    {

    }
} // namespace game
