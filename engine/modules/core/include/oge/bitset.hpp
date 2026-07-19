#include <cstdint>
#include <iostream>
#include <iterator>

namespace oge
{

class BitSet32
{
   private:
    uint32_t mask;

   public:
    BitSet32() : mask(0) {}

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
            return __builtin_ctz(remaining);  // index of lowest set bit
        }

        iterator& operator++()
        {
            remaining &= (remaining - 1);  // clear lowest set bit
            return *this;
        }

        bool operator!=(const iterator& other) const
        {
            return remaining != other.remaining;
        }
    };

    iterator begin() const { return iterator(mask); }
    iterator end()   const { return iterator(0); }
};
}  // namespace oge
