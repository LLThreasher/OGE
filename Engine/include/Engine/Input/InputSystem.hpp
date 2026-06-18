#pragma once
#include <array>
#include <optional>
#include "Keyboard.hpp"
#include "Mouse.hpp"
#include "Touch.hpp"

namespace OneGame::Engine
{

    class InputSystem
    {
    public:
        static constexpr int MaxKeys = 256;
        static constexpr int MaxMouseButtons = 8;
        static constexpr int MaxTouches = 4;

        void NewFrame();

        void SetKey(KeyCode key, bool down);
        void SetMouseButton(MouseButton button, bool down);
        void SetMousePosition(float x, float y);

        bool IsKeyDown(KeyCode key) const;
        bool IsKeyPressed(KeyCode key) const;
        bool IsKeyReleased(KeyCode key) const;

        bool IsMouseDown(MouseButton button) const;
        bool IsMousePressed(MouseButton button) const;
        bool IsMouseReleased(MouseButton button) const;

        float GetMouseX() const;
        float GetMouseY() const;

    private:
        std::array<bool, MaxKeys> m_currentKeys{};
        std::array<bool, MaxKeys> m_previousKeys{};

        std::array<bool, MaxMouseButtons> m_currentMouse{};
        std::array<bool, MaxMouseButtons> m_previousMouse{};

        std::array<TouchPoint, MaxTouches> m_currentTouch{};
        std::array<TouchPoint, MaxTouches> m_previousTouch{};

        float m_mouseX = 0.0f;
        float m_mouseY = 0.0f;
    };
}
