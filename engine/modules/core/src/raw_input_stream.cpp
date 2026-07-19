#include "oge/input/raw_input_stream.hpp"

namespace oge::input
{
void RawInputStream::AdvanceCursor(Cursor& cursor) const { cursor = frameFrontier.cursor; }

bool RawInputStream::PollEvent(Cursor& cursor, InputEvent& eventOut) const
{
    return events.PollOne(cursor.eventCursor, eventOut, frameFrontier.cursor.eventCursor);
}

bool RawInputStream::PollPtr(size_t ptrIdx, Cursor& cursor, math::vec2& posOut) const
{
    return pointers[ptrIdx].PollOne(cursor.ptrCursors[ptrIdx], posOut, frameFrontier.cursor.ptrCursors[ptrIdx]);
}

math::vec2 RawInputStream::PollPtrLatest(size_t ptrIdx, Cursor& cursor) const
{
    auto& frontier = frameFrontier.cursor.ptrCursors[ptrIdx];
    cursor.ptrCursors[ptrIdx] = frontier;
    return pointers[ptrIdx].Get(frontier);
}

math::vec2 RawInputStream::PollPtrDelta(size_t ptrIdx, Cursor& cursor) const
{
    return pointers[ptrIdx].PollDelta(cursor.ptrCursors[ptrIdx], frameFrontier.cursor.ptrCursors[ptrIdx]);
}

void RawInputStream::NewFrame()
{
    frameFrontier.activePtrs = activePtrs;
    events.AdvanceCursor(frameFrontier.cursor.eventCursor);
    for (size_t i = 0; i < PtrInputCount; ++i)
    {
        pointers[i].AdvanceCursor(frameFrontier.cursor.ptrCursors[i]);
    }
}

void RawInputStream::SetKey(KeyCode key, bool down)
{
    InputEvent res{down ? InputEventType::KeyDown : InputEventType::KeyUp};
    res.key = key;
    events.Push(res);
}

void RawInputStream::SetMouseButton(int id, MouseButton button, bool down)
{
    InputEvent res{down ? InputEventType::MouseButtonDown : InputEventType::MouseButtonUp};
    res.mouse = {FindMouse(id), button};
    events.Push(res);
}

void RawInputStream::SetMouseDelta(int id, float dx, float dy)
{
    auto ptr = pointers[FindMouse(id)];
    ptr.Push(ptr.Head() + math::vec2{dx, dy});
}

void RawInputStream::SetMousePosition(int id, float x, float y)
{
}

void RawInputStream::AddMouse(int id)
{
    uint32_t resultId = 0;
    for (uint32_t i = 0; i < MousePtrInputIndices.size(); ++i)
    {
        if (!activePtrs.contains(i))
        {
            resultId = i;
            break;
        }
    }
    LOG_INFO("add mouse {} at slot {}", id, resultId);
    InputEvent res2{InputEventType::AddMouse};
    res2.pointerIdx = resultId;
    events.Push(res2);
}

void RawInputStream::DelMouse(int id)
{
    InputEvent res2{InputEventType::RemoveMouse};
    res2.pointerIdx = FindMouse(id);
    events.Push(res2);
    activePtrs.remove(res2.pointerIdx);
    LOG_INFO("remove mouse {} from slot {}", id, res2.pointerIdx);
}

uint32_t RawInputStream::FindMouse(int id)
{
    for (uint32_t i = 0; i < MousePtrInputIndices.size(); ++i)
    {
        if (activePtrs.contains(i) && mouseIds[i] == id)
            return i;
    }
    return 0;
}

void RawInputStream::SetTouchDown(int id, float x, float y)
{
    InputEvent res{InputEventType::PointerDown};
    res.pointerIdx = MousePtrInputIndices.size() + id;
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
    res.pointerIdx = MousePtrInputIndices.size() + id;
    events.Push(res);
    pointers[id].Push({x, y});
}

}  // namespace OneGame::Engine
