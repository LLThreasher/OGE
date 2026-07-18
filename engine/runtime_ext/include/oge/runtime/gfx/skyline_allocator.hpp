#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

#include "oge/graphics/backend.hpp"
#include "oge/graphics/objects.hpp"
#include "oge/rect.hpp"

namespace oge::runtime::gfx
{

class SkylineAllocator
{
   public:
    SkylineAllocator(uint32_t w_capacity, uint32_t h_capacity, uint32_t padding)
        : m_w_capacity(w_capacity), m_h_capacity(h_capacity), m_padding(padding)
    {
        // Start with one flat skyline node
        m_skyline.push_back({0, 0, w_capacity});
    }

    bool Allocate(uint32_t width, uint32_t height, URect& output)
    {
        // Apply padding
        width += m_padding;
        height += m_padding;

        uint32_t bestY = std::numeric_limits<uint32_t>::max();
        uint32_t bestX = 0;
        int bestIndex = -1;

        for (size_t i = 0; i < m_skyline.size(); ++i)
        {
            uint32_t y = ComputeFitY(i, width);
            if (y == UINT32_MAX) continue;

            if (y + height > m_h_capacity) continue;

            if (y < bestY)
            {
                bestY = y;
                bestX = m_skyline[i].x;
                bestIndex = static_cast<int>(i);
            }
        }

        if (bestIndex == -1) return false;

        InsertNode(bestIndex, bestX, bestY, width, height);

        output.pos = UPoint2{bestX, bestY};
        output.extent = UPoint2{bestX + width - m_padding, bestY + height - m_padding};

        return true;
    }

   private:
    struct Node
    {
        uint32_t x;
        uint32_t y;
        uint32_t width;
    };

    std::vector<Node> m_skyline;

    uint32_t m_w_capacity;
    uint32_t m_h_capacity;
    uint32_t m_padding;

   private:
    uint32_t ComputeFitY(size_t index, uint32_t width) const
    {
        uint32_t x = m_skyline[index].x;
        if (x + width > m_w_capacity) return UINT32_MAX;

        uint32_t widthLeft = width;
        uint32_t y = m_skyline[index].y;
        size_t i = index;

        while (widthLeft > 0)
        {
            if (i >= m_skyline.size()) return UINT32_MAX;

            y = std::max(y, m_skyline[i].y);

            if (m_skyline[i].width >= widthLeft) break;

            widthLeft -= m_skyline[i].width;
            ++i;
        }

        return y;
    }

    void InsertNode(int index, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    {
        Node newNode;
        newNode.x = x;
        newNode.y = y + height;
        newNode.width = width;

        m_skyline.insert(m_skyline.begin() + index, newNode);

        // Fix overlaps
        for (size_t i = index + 1; i < m_skyline.size(); ++i)
        {
            Node& prev = m_skyline[i - 1];
            Node& curr = m_skyline[i];

            uint32_t prevEnd = prev.x + prev.width;

            if (curr.x < prevEnd)
            {
                uint32_t overlap = prevEnd - curr.x;

                if (overlap >= curr.width)
                {
                    m_skyline.erase(m_skyline.begin() + i);
                    --i;
                }
                else
                {
                    curr.x += overlap;
                    curr.width -= overlap;
                    break;
                }
            }
            else
            {
                break;
            }
        }

        MergeNodes();
    }

    void MergeNodes()
    {
        for (size_t i = 0; i + 1 < m_skyline.size();)
        {
            if (m_skyline[i].y == m_skyline[i + 1].y)
            {
                m_skyline[i].width += m_skyline[i + 1].width;
                m_skyline.erase(m_skyline.begin() + i + 1);
            }
            else
            {
                ++i;
            }
        }
    }
};

const uint32_t SKYLINE_PADDING = 2;

class DynamicSkylineAllocator
{
   public:
    DynamicSkylineAllocator(uint32_t atlasWidth = 1024, uint32_t atlasHeight = 1024)
        : m_atlasWidth(atlasWidth), m_atlasHeight(atlasHeight)
    {
    }

    uint32_t GetWidth() { return m_atlasWidth; }
    uint32_t GetHeight() { return m_atlasHeight; }

    GPUTextureRegion Allocate(graphics::IGraphicsBackend& backend, uint32_t width, uint32_t height)
    {
        GPUTextureRegion output;
        if (m_allocators.empty())
        {
            m_allocators.emplace_back(m_atlasWidth, m_atlasHeight, SKYLINE_PADDING);
            m_textures.push_back(backend.AllocateGPUTexture(m_atlasWidth, m_atlasHeight));
        }
        if (m_allocators.back().Allocate(width, height, output.region))
        {
            output.texture = m_textures.back();
        }
        else
        {
            m_allocators.emplace_back(m_atlasWidth, m_atlasHeight, SKYLINE_PADDING);
            m_textures.push_back(backend.AllocateGPUTexture(m_atlasWidth, m_atlasHeight));
            assert(m_allocators.back().Allocate(width, height, output.region));
            output.texture = m_textures.back();
        }
        return output;
    }

   private:
    uint32_t m_atlasWidth;
    uint32_t m_atlasHeight;
    std::vector<SkylineAllocator> m_allocators;
    std::vector<GPUTextureHandle> m_textures;
};
}  // namespace oge::runtime::renderer