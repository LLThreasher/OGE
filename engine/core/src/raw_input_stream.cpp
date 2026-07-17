#include "oge/input/raw_input_stream.hpp"

namespace oge::input
{
void RawInputStream::AdvanceCursor(Cursor& cursor) const { cursor = frameFrontier; }

bool RawInputStream::PollEvent(Cursor& cursor, InputEvent& eventOut) const
{
    if (cursor.eventCursor == frameFrontier.eventCursor) return false;
    return events.PollOne(cursor.eventCursor, eventOut);
}

bool RawInputStream::PollPtr(size_t ptrIdx, Cursor& cursor, math::vec2& posOut) const
{
    if (cursor.ptrCursors[ptrIdx] == frameFrontier.ptrCursors[ptrIdx]) return false;
    return pointers[ptrIdx].PollOne(cursor.ptrCursors[ptrIdx], posOut);
}

math::vec2 RawInputStream::PollPtrDelta(size_t ptrIdx, Cursor& cursor) const
{
    if (cursor.ptrCursors[ptrIdx] == frameFrontier.ptrCursors[ptrIdx]) return {};
    return pointers[ptrIdx].PollDelta(cursor.ptrCursors[ptrIdx]);
}

void RawInputStream::NewFrame()
{
    events.AdvanceCursor(frameFrontier.eventCursor);
    for (size_t i = 0; i < PtrInputCount; ++i)
    {
        pointers[i].AdvanceCursor(frameFrontier.ptrCursors[i]);
    }
}

void RawInputStream::SetKey(KeyCode key, bool down)
{
    InputEvent res{down ? InputEventType::KeyDown : InputEventType::KeyUp};
    res.key = key;
    events.Push(res);
}

void RawInputStream::SetMouseButton(MouseButton button, bool down)
{
    InputEvent res{down ? InputEventType::MouseButtonDown : InputEventType::MouseButtonUp};
    res.mouseButton = button;
    events.Push(res);

    InputEvent res2{InputEventType::PointerDown};
    res2.pointerIdx = RawInputStream::MousePtrInputIdx;
    events.Push(res2);
}

void RawInputStream::SetMouseDelta(float dx, float dy)
{
    auto ptr = pointers[RawInputStream::MousePtrInputIdx];
    ptr.Push(ptr.Head() + math::vec2{dx, dy});
}

void RawInputStream::SetMousePosition(float x, float y)
{
}

void RawInputStream::SetTouchDown(int id, float x, float y)
{
    InputEvent res{InputEventType::PointerDown};
    res.pointerIdx = RawInputStream::MousePtrInputIdx;
    events.Push(res);
    pointers[id].Push({x, y});
}

void RawInputStream::SetTouchUpdate(int id, float x, float y)
{
    pointers[id].Push({x, y});
}

void RawInputStream::SetTouchUp(int id, float x, float y)
{
    InputEvent res{InputEventType::PointerUp};
    res.pointerIdx = RawInputStream::MousePtrInputIdx;
    events.Push(res);
    pointers[id].Push({x, y});
}

}  // namespace OneGame::Engine
