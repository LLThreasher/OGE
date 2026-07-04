#pragma once
#include <vector>
#include <cstring>
#include <stdexcept>

namespace OneGame::Engine::Net
{

class NetBuffer
{
public:
    NetBuffer(uint8_t* memory, size_t size)
        : data(memory), capacity(size) {}

    template<typename T>
    void Write(const T& value)
    {
        if (writePos + sizeof(T) > capacity)
            throw std::runtime_error("Buffer overflow");

        std::memcpy(data + writePos, &value, sizeof(T));
        writePos += sizeof(T);
    }

    template<typename T>
    T Read()
    {
        if (readPos + sizeof(T) > writePos)
            throw std::runtime_error("Buffer overflow");

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

struct NetInt
{
    int value;

    void Serialize(NetBuffer& buffer)
    {
        buffer.Write<int>(value);
    }

    void Deserialize(NetBuffer& buffer)
    {
        value = buffer.Read<int>();
    }
};

struct NetFloat
{
    float value;

    void Serialize(NetBuffer& buffer)
    {
        buffer.Write<float>(value);
    }

    void Deserialize(NetBuffer& buffer)
    {
        value = buffer.Read<float>();
    }
};

struct NetBool
{
    bool value;

    void Serialize(NetBuffer& buffer)
    {
        buffer.Write<bool>(value);
    }

    void Deserialize(NetBuffer& buffer)
    {
        value = buffer.Read<bool>();
    }
};

template<typename Derived>
class NetObject
{
public:
    void Serialize(NetBuffer& buffer)
    {
        static_cast<Derived*>(this)->VisitFields(
            [&](auto& field)
            {
                field.Serialize(buffer);
            });
    }

    void Deserialize(NetBuffer& buffer)
    {
        static_cast<Derived*>(this)->VisitFields(
            [&](auto& field)
            {
                field.Deserialize(buffer);
            });
    }
};

}
