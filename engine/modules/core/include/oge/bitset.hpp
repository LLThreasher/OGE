#include <cassert>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <bit>

namespace oge
{

class BitSet32
{
   private:
    uint32_t mask;

   public:
    BitSet32() : mask(0) {}

    void set(unsigned x, bool value)
    {
        assert(x < 32);
        uint32_t m = 1u << x;
        mask = (mask & ~m) | (uint32_t(-value) & m);
    }

    bool get(unsigned x) const
    {
        assert(x < 32);
        return (mask >> x) & 1u;
    }

    // Add element
    void add(int x) { mask |= (1ULL << x); }

    // Remove element
    void remove(int x) { mask &= ~(1ULL << x); }

    // Toggle element
    void toggle(int x) { mask ^= (1ULL << x); }

    // Check if element exists
    bool contains(int x) const { return (mask & (1ULL << x)) != 0; }

    // Clear all elements
    void clear() { mask = 0; }

    class iterator
    {
       private:
        uint32_t remaining;

       public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = int;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = int;

        iterator(uint32_t mask) : remaining(mask) {}

        int operator*() const
        {
            return std::countr_zero(remaining);  // index of lowest set bit
        }

        iterator& operator++()
        {
            remaining &= (remaining - 1);  // clear lowest set bit
            return *this;
        }

        bool operator!=(const iterator& other) const { return remaining != other.remaining; }
    };

    iterator begin() const { return iterator(mask); }
    iterator end() const { return iterator(0); }
};

template <typename T>
class AnyBitSet32 : public BitSet32
{
public:
    // Add element
    void add(T x) { BitSet32::add(static_cast<unsigned int>(x)); }

    // Remove element
    void remove(T x) { BitSet32::remove(static_cast<unsigned int>(x)); }

    // Toggle element
    void toggle(T x) { BitSet32::toggle(static_cast<unsigned int>(x)); }

    // Check if element exists
    bool contains(T x) const { BitSet32::contains(static_cast<unsigned int>(x)); }
};

class BitSet256
{
    std::array<uint32_t, 8> sets{};

   public:
    void set(unsigned x, bool value)
    {
        assert(x < 256);
        auto& w = sets[x >> 5];
        uint32_t m = 1u << (x & 31);
        w = (w & ~m) | (uint32_t(-value) & m);
    }

    bool get(unsigned x) const
    {
        assert(x < 256);
        return (sets[x >> 5] >> (x & 31)) & 1u;
    }

    void add(unsigned x)
    {
        assert(x < 256);
        sets[x >> 5] |= (1u << (x & 31));
    }

    void remove(unsigned x)
    {
        assert(x < 256);
        sets[x >> 5] &= ~(1u << (x & 31));
    }

    bool contains(unsigned x) const
    {
        assert(x < 256);
        return sets[x >> 5] & (1u << (x & 31));
    }

    void toggle(unsigned x)
    {
        assert(x < 256);
        sets[x >> 5] ^= (1u << (x & 31));
    }

    void clear() { sets.fill(0); }
};

template <typename T>
class AnyBitSet256 : public BitSet256
{
public:
    void set(T ky, bool down)
    {
        BitSet256::set(static_cast<unsigned int>(ky), down);
    }

    bool get(T ky) const
    {
        return BitSet256::get(static_cast<unsigned int>(ky));
    }

    void add(T ky)
    {
        BitSet256::add(static_cast<unsigned int>(ky));
    }

    void remove(T ky)
    {
        BitSet256::remove(static_cast<unsigned int>(ky));
    }

    bool contains(T ky) const
    {
        return BitSet256::contains(static_cast<unsigned int>(ky));
    }
};
}  // namespace oge
