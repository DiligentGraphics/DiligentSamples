#pragma once

#include <DirectXMath.h>
#include <tuple>

namespace VoxelOC
{
    struct DrawTask
    {
        DirectX::XMFLOAT4 BasePosAndScale;
        DirectX::XMFLOAT4 RandomValue;

        bool operator==(DrawTask& other)
        {
            return BasePosAndScale.x == other.BasePosAndScale.x
                && BasePosAndScale.y == other.BasePosAndScale.y
                && BasePosAndScale.z == other.BasePosAndScale.z;
        }
    };

    struct GPUOctreeNode
    {
        DirectX::XMFLOAT4 minAndIsFull{};
        DirectX::XMFLOAT4 max{};
        int               childrenStartIndex{};
        int               numChildren{};
    };
}
