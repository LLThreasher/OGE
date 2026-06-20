#include "Engine/Input/InputSystem.hpp"

namespace OneGame::Engine
{

    void InputSystem::NewFrame()
    {
        m_previousKeys = m_currentKeys;
        m_previousMouse = m_currentMouse;
        m_previousTouch = m_currentTouch;
        m_previousMouseX = m_mouseX;
        m_previousMouseY = m_mouseY;
        m_mouseDX = 0;
        m_mouseDY = 0;
        m_previousTouch = m_currentTouch;
        m_pressedTouchIdMask = 0;
        m_releasedTouchIdMask = 0;
    }

    void InputSystem::SetKey(KeyCode key, bool down)
    {
        m_currentKeys[static_cast<uint32_t>(key)] = down;
    }

    void InputSystem::SetMouseButton(MouseButton button, bool down)
    {
        m_currentMouse[static_cast<uint32_t>(button)] = down;
    }

    void InputSystem::SetMouseDelta(float dx, float dy)
    {
        m_mouseDX += dx;
        m_mouseDY += dy;
    }

    void InputSystem::SetMousePosition(float x, float y)
    {
        m_mouseX = x;
        m_mouseY = y;
    }


    void InputSystem::SetTouchDown(int id, float x, float y)
    {
        m_pressedTouchIdMask |= 1 << id;
        m_currentTouch[id] = { x,y };
    }

    void InputSystem::SetTouchUpdate(int id, float x, float y)
    {
        m_currentTouch[id] = { x, y };
    }

    void InputSystem::SetTouchUp(int id, float x, float y)
    {
        m_releasedTouchIdMask |= 1 << id;
        m_currentTouch[id] = { x, y };
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
    //float InputSystem::GetMouseDX() const { return m_mouseX - m_previousMouseX; }
    //float InputSystem::GetMouseDY() const { return m_mouseY - m_previousMouseY; }
    float InputSystem::GetMouseDX() const { return m_mouseDX; }
    float InputSystem::GetMouseDY() const { return m_mouseDY; }

    uint32_t InputSystem::GetPressedTouchIdMask() const { return m_pressedTouchIdMask; }
    uint32_t InputSystem::GetReleasedTouchIdMask() const { return m_releasedTouchIdMask; }
    float InputSystem::GetTouchX(int id) const { return m_currentTouch[id].x; }
    float InputSystem::GetTouchY(int id) const { return m_currentTouch[id].y; }
    float InputSystem::GetTouchDX(int id) const { return m_currentTouch[id].x - m_previousTouch[id].x; }
    float InputSystem::GetTouchDY(int id) const { return m_currentTouch[id].y - m_previousTouch[id].y; }

}
