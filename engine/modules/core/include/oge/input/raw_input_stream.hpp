#pragma once

#include "oge/event_stream.hpp"
#include "oge/input/keyboard.hpp"
#include "oge/input/mouse.hpp"
#include "oge/input/touch.hpp"
#include "oge/log.hpp"
#include "oge/math.hpp"
#include "oge/bitset.hpp"

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
    AddMouse,
    RemoveMouse,
};

struct PackedMouseInfo
{
    uint8_t val;

    PackedMouseInfo(size_t id, MouseButton button)
        : val((static_cast<uint8_t>(id & 0x7) << 4) | (static_cast<uint8_t>(button) & 0x7))
    {
    }

    MouseButton button() const { return static_cast<MouseButton>(val & 0x7); }

    size_t ptrIdx() const { return (val >> 4) & 0x7; }
};

struct InputEvent
{
    InputEventType type;
    union
    {
        KeyCode key;
        PackedMouseInfo mouse;
        uint8_t pointerIdx;
    };
};

using EventInputStream = DiscreteEventStream<InputEvent>;
using PointerInputStream = AccumulativeEventStream<math::vec2>;

class RawInputStream
{
   public:
    static constexpr size_t MaxMousePtrCount = 4;
    static constexpr std::array<size_t, MaxMousePtrCount> MousePtrInputIndices = {0, 1, 2, 3};
    static constexpr std::array<size_t, 10> TouchPtrInputIndices = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
    static constexpr size_t PtrInputCount = MousePtrInputIndices.size() + TouchPtrInputIndices.size();
    struct Cursor
    {
        EventInputStream::Cursor eventCursor;
        std::array<PointerInputStream::Cursor, PtrInputCount> ptrCursors;
    };

    void AdvanceCursor(Cursor& cursor) const;
    bool PollEvent(Cursor& cursor, InputEvent& eventOut) const;
    bool PollPtr(size_t ptrIdx, Cursor& cursor, math::vec2& posOut) const;
    math::vec2 PollPtrLatest(size_t ptrIdx, Cursor& cursor) const;
    math::vec2 PollPtrDelta(size_t ptrIdx, Cursor& cursor) const;
    const BitSet32& ActivePtrs() const { return frameFrontier.activePtrs; }
    static bool IsMouse(size_t ptrIdx) { return ptrIdx < MaxMousePtrCount; }

    void NewFrame();

    void SetKey(KeyCode key, bool down);
    void SetMouseButton(int id, MouseButton button, bool down);
    void SetMouseDelta(int id, float dx, float dy);
    void SetMousePosition(int id, float x, float y);
    void AddMouse(int id);
    void DelMouse(int id);

    void SetTouchDown(int id, float x, float y);
    void SetTouchUpdate(int id, float x, float y);
    void SetTouchUp(int id, float x, float y);

   private:
    uint32_t FindMouse(int id);

    EventInputStream events;
    std::array<int, MaxMousePtrCount> mouseIds{};
    std::array<PointerInputStream, PtrInputCount> pointers;
    struct { Cursor cursor; BitSet32 activePtrs; } frameFrontier;
    BitSet32 activePtrs;
};

enum class UIEventType : uint8_t
{
    DragStart = 0,
    DragUpdate,
    DragEnd,
};
}  // namespace oge::input
