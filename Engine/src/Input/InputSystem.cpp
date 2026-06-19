#include "Engine/Input/InputSystem.hpp"

namespace OneGame::Engine
{

    void InputSystem::NewFrame()
    {
        m_previousKeys = m_currentKeys;
        m_previousMouse = m_currentMouse;
        m_previousTouch = m_currentTouch;
    }

    void InputSystem::SetKey(KeyCode key, bool down)
    {
        m_currentKeys[static_cast<uint32_t>(key)] = down;
    }

    void InputSystem::SetMouseButton(MouseButton button, bool down)
    {
        m_currentMouse[static_cast<uint32_t>(button)] = down;
    }

    void InputSystem::SetMousePosition(float x, float y)
    {
        m_mouseDX = x - m_mouseX;
        m_mouseDY = y - m_mouseY;
        m_mouseX = x;
        m_mouseY = y;
    }

    bool InputSystem::IsKeyDown(KeyCode key) const
    {
        return m_currentKeys[static_cast<uint32_t>(key)];
    }

    bool InputSystem::IsKeyPressed(KeyCode key) const
    {
        return m_currentKeys[static_cast<uint32_t>(key)] && !m_previousKeys[static_cast<uint32_t>(key)];
    }

    bool InputSystem::IsKeyReleased(KeyCode key) const
    {
        return !m_currentKeys[static_cast<uint32_t>(key)] && m_previousKeys[static_cast<uint32_t>(key)];
    }

    bool InputSystem::IsMouseDown(MouseButton button) const
    {
        return m_currentMouse[static_cast<uint32_t>(button)];
    }

    bool InputSystem::IsMousePressed(MouseButton button) const
    {
        return m_currentMouse[static_cast<uint32_t>(button)] && !m_previousMouse[static_cast<uint32_t>(button)];
    }

    bool InputSystem::IsMouseReleased(MouseButton button) const
    {
        return !m_currentMouse[static_cast<uint32_t>(button)] && m_previousMouse[static_cast<uint32_t>(button)];
    }

    float InputSystem::GetMouseX() const { return m_mouseX; }
    float InputSystem::GetMouseY() const { return m_mouseY; }
    float InputSystem::GetMouseDX() const { return m_mouseDX; }
    float InputSystem::GetMouseDY() const { return m_mouseDY; }

}
