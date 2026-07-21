#pragma once
#include <cstdint>
#include <iterator>

namespace oge
{
template <typename T, size_t Max>
class FixedVector
{
   public:
    using value_type = T;
    using size_type = std::size_t;

    FixedVector() : m_size(0) {}

    bool push_back(const T& aabb)
    {
        if (m_size >= Max) return false;

        m_data[m_size++] = aabb;
        return true;
    }

    void clear() { m_size = 0; }

    size_type size() const { return m_size; }
    static constexpr size_type capacity() { return Max; }

    T& operator[](size_type i) { return m_data[i]; }
    const T& operator[](size_type i) const { return m_data[i]; }

    T* begin() { return m_data; }
    T* end() { return m_data + m_size; }

    const T* begin() const { return m_data; }
    const T* end() const { return m_data + m_size; }

   private:
    T m_data[Max];
    size_type m_size;
};
}  // namespace oge