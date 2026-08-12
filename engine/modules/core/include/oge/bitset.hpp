#pragma once

#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>

namespace oge
{

template <std::size_t BitCount>
class BitSet
{
    static_assert(BitCount > 0);

   private:
    using Word = uint32_t;

    static constexpr std::size_t wordBits = 32;
    static constexpr std::size_t wordCount =
        (BitCount + wordBits - 1) / wordBits;
    static constexpr std::size_t lastWordBits = BitCount % wordBits;

    std::array<Word, wordCount> sets{};

    static constexpr Word last_word_mask()
    {
        if constexpr (lastWordBits == 0)
        {
            return Word{0xFFFFFFFFu};
        }
        else
        {
            return Word{(Word{1u} << lastWordBits) - Word{1u}};
        }
    }

    void mask_unused_bits()
    {
        sets[wordCount - 1] &= last_word_mask();
    }

   public:
    static constexpr std::size_t bit_count = BitCount;
    static constexpr std::size_t word_count = wordCount;

    static constexpr BitSet make_full()
    {
        BitSet result;
        result.sets.fill(Word{0xFFFFFFFFu});
        result.sets[wordCount - 1] &= last_word_mask();
        return result;
    }

    constexpr void set(std::size_t x, bool value)
    {
        assert(x < BitCount);

        Word& w = sets[x >> 5];
        Word m = Word{1u} << (x & 31);

        w = (w & ~m) | (Word{0u - static_cast<unsigned>(value)} & m);
    }

    constexpr bool get(std::size_t x) const
    {
        assert(x < BitCount);
        return (sets[x >> 5] >> (x & 31)) & Word{1u};
    }

    constexpr void add(std::size_t x)
    {
        assert(x < BitCount);
        sets[x >> 5] |= Word{1u} << (x & 31);
    }

    constexpr void remove(std::size_t x)
    {
        assert(x < BitCount);
        sets[x >> 5] &= ~(Word{1u} << (x & 31));
    }

    constexpr bool contains(std::size_t x) const
    {
        assert(x < BitCount);
        return (sets[x >> 5] & (Word{1u} << (x & 31))) != 0;
    }

    constexpr void toggle(std::size_t x)
    {
        assert(x < BitCount);
        sets[x >> 5] ^= Word{1u} << (x & 31);
    }

    constexpr void clear()
    {
        sets.fill(Word{0u});
    }

    class iterator
    {
       private:
        const BitSet* owner = nullptr;
        std::size_t wordIndex = 0;
        Word currentRemaining = 0;

        void advance_to_next_non_empty_word()
        {
            while (currentRemaining == 0 && wordIndex < wordCount)
            {
                currentRemaining = owner->sets[wordIndex];

                if (currentRemaining != 0)
                {
                    break;
                }

                ++wordIndex;
            }
        }

       public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = std::size_t;

        iterator() = default;

        iterator(const BitSet* owner, std::size_t wordIndex)
            : owner(owner), wordIndex(wordIndex)
        {
            if (owner != nullptr && wordIndex < wordCount)
            {
                currentRemaining = owner->sets[wordIndex];
                advance_to_next_non_empty_word();
            }
        }

        std::size_t operator*() const
        {
            return wordIndex * wordBits + std::countr_zero(currentRemaining);
        }

        iterator& operator++()
        {
            currentRemaining &= currentRemaining - 1;

            if (currentRemaining == 0)
            {
                ++wordIndex;
                advance_to_next_non_empty_word();
            }

            return *this;
        }

        iterator operator++(int)
        {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const iterator& other) const
        {
            return owner == other.owner && wordIndex == other.wordIndex &&
                   currentRemaining == other.currentRemaining;
        }

        bool operator!=(const iterator& other) const
        {
            return !(*this == other);
        }
    };

    iterator begin() const
    {
        return iterator(this, 0);
    }

    iterator end() const
    {
        return iterator(this, wordCount);
    }
};

using BitSet32 = BitSet<32>;
using BitSet256 = BitSet<256>;

template <std::size_t BitCount, typename T>
class AnyBitSet : public BitSet<BitCount>
{
    using Base = BitSet<BitCount>;
   public:
    void set(T ky, bool down)
    {
        Base::set(static_cast<unsigned int>(ky), down);
    }

    bool get(T ky) const
    {
        return Base::get(static_cast<unsigned int>(ky));
    }

    void add(T ky)
    {
        Base::add(static_cast<unsigned int>(ky));
    }

    void remove(T ky)
    {
        Base::remove(static_cast<unsigned int>(ky));
    }

    void toggle(T x)
    {
        Base::toggle(static_cast<unsigned int>(x));
    }

    bool contains(T ky) const
    {
        return Base::contains(static_cast<unsigned int>(ky));
    }
};

template <typename T>
using AnyBitSet32 = AnyBitSet<32, T>;

template <typename T>
using AnyBitSet256 = AnyBitSet<256, T>;

}  // namespace oge
