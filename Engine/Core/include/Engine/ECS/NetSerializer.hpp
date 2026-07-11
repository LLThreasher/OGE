#pragma once
#include <cstring>
#include <stdexcept>
#include <vector>

namespace OneGame::Engine::Net
{

class Buffer
{
   public:
    Buffer(uint8_t* memory, size_t size) : data(memory), capacity(size) {}

    template <typename T>
    void Write(const T& value)
    {
        assert(writePos + sizeof(T) <= capacity);

        std::memcpy(data + writePos, &value, sizeof(T));
        writePos += sizeof(T);
    }

    template <typename T>
    T Read()
    {
        assert(readPos + sizeof(T) <= writePos);

        T value;
        std::memcpy(&value, data + readPos, sizeof(T));
        readPos += sizeof(T);
        return value;
    }

    void Reset()
    {
        writePos = 0;
        readPos = 0;
    }

   private:
    uint8_t* data;
    size_t capacity;
    size_t writePos = 0;
    size_t readPos = 0;
};

struct Int
{
    int value;

    void Serialize(Buffer& buffer) { buffer.Write<int>(value); }

    void Deserialize(Buffer& buffer) { value = buffer.Read<int>(); }
};

struct Float
{
    float value;

    void Serialize(Buffer& buffer) { buffer.Write<float>(value); }

    void Deserialize(Buffer& buffer) { value = buffer.Read<float>(); }
};

struct Bool
{
    bool value;

    void Serialize(Buffer& buffer) { buffer.Write<bool>(value); }

    void Deserialize(Buffer& buffer) { value = buffer.Read<bool>(); }
};

template <typename Derived>
class Object
{
   public:
    void Serialize(Buffer& buffer)
    {
        static_cast<Derived*>(this)->VisitFields([&](auto& field) { field.Serialize(buffer); });
    }

    void Deserialize(Buffer& buffer)
    {
        static_cast<Derived*>(this)->VisitFields([&](auto& field) { field.Deserialize(buffer); });
    }
};

}  // namespace OneGame::Engine::Net
