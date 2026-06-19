#pragma once
#include <random>

namespace OneGame::Engine
{
    class Random {
    public:
        static std::mt19937& Get()
        {
            static std::mt19937 rng(std::random_device{}());
            return rng;
        }

        static int RandInt(int lo, int hi)
        {
            std::uniform_int_distribution<int> dist(lo, hi);
            return dist(Get());
        }
    };
}
