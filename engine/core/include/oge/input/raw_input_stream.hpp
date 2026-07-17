#pragma once

#include "oge/event_stream.hpp"
#include "oge/log.hpp"
#include "oge/math.hpp"
#include "oge/input/keyboard.hpp"
#include "oge/input/mouse.hpp"
#include "oge/input/touch.hpp"

namespace oge::input
{
enum class InputEventType : uint8_t
{
    MouseButtonDown = 0,
    MouseButtonUp,
    KeyDown,
    KeyUp,
    PointerDown,
    PointerUp,
};

struct InputEvent
{
    InputEventType type;
    union
    {
        KeyCode key;
        MouseButton mouseButton;
        uint8_t pointerIdx;
    };
};

using EventInputStream = DiscreteEventStream<InputEvent>;
using PointerInputStream = AccumulativeEventStream<math::vec2>;

class RawInputStream
{
   public:
    static constexpr size_t MousePtrInputIdx = 0;
    static constexpr size_t PtrInputCount = 11;
    struct Cursor
    {
        EventInputStream::Cursor eventCursor;
        std::array<PointerInputStream::Cursor, PtrInputCount> ptrCursors;
    };

    void AdvanceCursor(Cursor& cursor) const;
    bool PollEvent(Cursor& cursor, InputEvent& eventOut) const;
    bool PollPtr(size_t ptrIdx, Cursor& cursor, math::vec2& posOut) const;
    math::vec2 PollPtrDelta(size_t ptrIdx, Cursor& cursor) const;

    void NewFrame();

    void SetKey(KeyCode key, bool down);
    void SetMouseButton(MouseButton button, bool down);
    void SetMouseDelta(float dx, float dy);
    void SetMousePosition(float x, float y);

    void SetTouchDown(int id, float x, float y);
    void SetTouchUpdate(int id, float x, float y);
    void SetTouchUp(int id, float x, float y);

   private:
    EventInputStream events;
    std::array<PointerInputStream, PtrInputCount> pointers;
    Cursor frameFrontier;
};

enum class UIEventType : uint8_t
{
    DragStart = 0,
    DragUpdate,
    DragEnd,
};
}  // namespace oge::platform
