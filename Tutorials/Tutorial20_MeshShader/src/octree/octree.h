#pragma once

#include <DirectXMath.h>
#include <array>
#include <vector>
#include "DrawTask.h"

struct AABB
{
    DirectX::XMFLOAT3 min = {};
    DirectX::XMFLOAT3 max = {};
};

template<typename T>
class OctreeNode
{
public:
    AABB                       bounds        = {};
    std::array<OctreeNode*, 8> children;
    std::vector<int>           objectIndices;
    bool                       isLeaf        = {};

    OctreeNode(const AABB& bounds) :
        bounds(bounds), isLeaf(true)
    {
        children.fill(nullptr);
    }

    void SplitNode();
    void InsertObject(int objectIndex, const AABB& objectBounds);
};

constexpr int MaxObjectsPerLeaf() { return 100; }
bool Intersects(const AABB& first, const AABB& second);

extern std::vector<DrawTask> ObjectBuffer;