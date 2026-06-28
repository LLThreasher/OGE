#pragma once
#include <array>
#include <optional>
#include "Keyboard.hpp"
#include "Mouse.hpp"
#include "Touch.hpp"

namespace OneGame::Engine
{
    namespace PointerIdx
    {
        constexpr int COUNT = 11;
        constexpr int MOUSE = 0;
        constexpr int TOUCH0 = 1;
        constexpr int TOUCH1 = 2;
        constexpr int TOUCH2 = 3;
        constexpr int TOUCH3 = 4;
        constexpr int TOUCH4 = 5;
        constexpr int TOUCH5 = 6;
        constexpr int TOUCH6 = 7;
        constexpr int TOUCH7 = 8;
        constexpr int TOUCH8 = 9;
        constexpr int TOUCH9 = 10;

        int PtrIdxFromTouchIdx(int idx);
        int TouchIdxFromPtrIdx(int idx);
    }

    class InputSystem
    {
    public:
        static constexpr int MaxKeys = 256;
        static constexpr int MaxMouseButtons = 8;
        static constexpr int MaxTouches = 10;

        void NewFrame();

        void SetKey(KeyCode key, bool down);
        void SetMouseButton(MouseButton button, bool down);
        void SetMouseDelta(float dx, float dy);
        void SetMousePosition(float x, float y);

        void SetTouchDown(int id, float x, float y);
        void SetTouchUpdate(int id, float x, float y);
        void SetTouchUp(int id, float x, float y);

        bool IsKeyDown(KeyCode key) const;
        bool IsKeyPressed(KeyCode key) const;
        bool IsKeyReleased(KeyCode key) const;

        bool IsMouseDown(MouseButton button) const;
        bool IsMousePressed(MouseButton button) const;
        bool IsMouseReleased(MouseButton button) const;

        float GetMouseX() const;
        float GetMouseY() const;
        float GetMouseDX() const;
        float GetMouseDY() const;

        uint32_t GetPressedTouchIdMask() const;
        uint32_t GetReleasedTouchIdMask() const;
        float GetTouchX(int id) const;
        float GetTouchY(int id) const;
        float GetTouchDX(int id) const;
        float GetTouchDY(int id) const;

    private:
        std::array<bool, MaxKeys> m_currentKeys{};
        std::array<bool, MaxKeys> m_previousKeys{};

        std::array<bool, MaxMouseButtons> m_currentMouse{};
        std::array<bool, MaxMouseButtons> m_previousMouse{};

        uint32_t m_pressedTouchIdMask = 0;
        uint32_t m_releasedTouchIdMask = 0;
        std::array<TouchPoint, MaxTouches> m_currentTouch{};
        std::array<TouchPoint, MaxTouches> m_previousTouch{};

        float m_mouseX = 0.0f;
        float m_mouseY = 0.0f;
        float m_mouseDX = 0.0f;
        float m_mouseDY = 0.0f;
        float m_previousMouseX = 0.0f;
        float m_previousMouseY = 0.0f;
    };
}
