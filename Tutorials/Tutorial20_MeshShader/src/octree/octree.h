#pragma once

#include <DirectXMath.h>
#include <array>
#include <vector>
#include <set>
#include "../DrawTask.h"

struct AABB
{
    DirectX::XMFLOAT3 min = {};
    DirectX::XMFLOAT3 max = {};

    DirectX::XMFLOAT3 Center() const
    {
        return {(min.x + max.x) / 2.f, (min.y + max.y) / 2.f, (min.z + max.z) / 2.f};
    }
};

extern std::vector<VoxelOC::DrawTask> ObjectBuffer;

constexpr int MaxObjectsPerLeaf() { return 32; }
bool          IntersectAABBAABB(const AABB& first, const AABB& second);
bool          IntersectAABBPoint(const AABB& first, const DirectX::XMFLOAT3& second);
extern AABB   GetObjectBounds(int index);

struct DebugInfo
{
    size_t processedIndices = 0;
    size_t acceptedIndices  = 0;
    size_t skippedIndices   = 0;
    size_t minIndex         = INT_MAX;
    size_t maxIndex         = 0;
    size_t minIndexSkipped  = INT_MAX;
    size_t maxIndexSkipped  = 0;
};

template<typename T>
class OctreeNode
{
public:
    AABB                       bounds                = {};
    std::array<OctreeNode*, 8> children              = {};
    std::vector<int>           objectIndices         = {};
    bool                       isLeaf                = {};
    unsigned long long         contentOccupationMask = 0;
    unsigned int               childOccupationMask   = 0;

    DebugInfo* getGridIndicesDebugInfo = nullptr;
    DebugInfo* insertOctreeDebugInfo   = nullptr;

    OctreeNode(const AABB& bounds, DebugInfo* getGridIdicesDebugInfo, DebugInfo* insertOctreeDebugInfo) :
        bounds(bounds), isLeaf(true), getGridIndicesDebugInfo(getGridIdicesDebugInfo), insertOctreeDebugInfo(insertOctreeDebugInfo)
    {
        children.fill(nullptr);
        objectIndices.reserve(MaxObjectsPerLeaf());
    }

    ~OctreeNode()
    {
        contentOccupationMask = 0;
        childOccupationMask   = 0;
        objectIndices.clear();
        
        for(auto & child : children)
        {
            delete child;
        }
    }

    void GetAllGridIndices(std::vector<int>& gridIndexBuffer, std::vector<char>& duplicateBuffer, std::vector<VoxelOC::GPUOctreeNode>& octreeNodeBuffer)
    {
        // Check for children (Buttom Up)
        for (int i = 0; i < children.size(); ++i)
        {
            //if (childOccupationMask & (0b00000001 << i))
            if (children[i] != nullptr)
                children[i]->GetAllGridIndices(gridIndexBuffer, duplicateBuffer, octreeNodeBuffer);
        }

        // Create octree node data for gpu
        VoxelOC::GPUOctreeNode ocNode;
        ocNode.childrenStartIndex = static_cast<int>(gridIndexBuffer.size());
        ocNode.numChildren        = static_cast<int>(objectIndices.size());
        ocNode.minAndIsFull       = DirectX::XMFLOAT4{bounds.min.x, bounds.min.y, bounds.min.z, objectIndices.size() >= MaxObjectsPerLeaf() ? 1.0f : 0.0f};
        ocNode.max                = DirectX::XMFLOAT4{bounds.max.x, bounds.max.y, bounds.max.z, 0};

        // @TODO: Check if it can be adventageous to treat nodes in a tree fashion and collaps full nodes to a full parent node!
        if (objectIndices.size() > 0)       // Only insert nodes which actually store voxels. Makes it easier to iterate in depth pre-pass
            octreeNodeBuffer.push_back(std::move(ocNode));

        // Add own indices if present
        for (int index = 0; index < objectIndices.size(); ++index)
        {
            if (duplicateBuffer[objectIndices[index]] == 0)
            {
                gridIndexBuffer.push_back(objectIndices[index]) ;
                duplicateBuffer[objectIndices[index]] = 1;
            }
        }
    }

    void SplitNode()
    {
        if (!isLeaf) return;

        DirectX::XMFLOAT3 center = {
                (bounds.min.x + bounds.max.x) * 0.5f,
                (bounds.min.y + bounds.max.y) * 0.5f,
                (bounds.min.z + bounds.max.z) * 0.5f
            };

        for (int i = 0; i < 8; ++i)
        {
            DirectX::XMFLOAT3 newMin{};
            DirectX::XMFLOAT3 newMax{};

            newMin.x = (i & 1) ? center.x : bounds.min.x;
            newMin.y = (i & 2) ? center.y : bounds.min.y;
            newMin.z = (i & 4) ? center.z : bounds.min.z;
            newMax.x = (i & 1) ? bounds.max.x : center.x;
            newMax.y = (i & 2) ? bounds.max.y : center.y;
            newMax.z = (i & 4) ? bounds.max.z : center.z;

            children[i] = new OctreeNode({newMin, newMax}, getGridIndicesDebugInfo, insertOctreeDebugInfo);
        }

        isLeaf = false;
    }

    void InsertObject(int objectIndex, const AABB objectBounds)
    {
        OctreeNode*              currentNode = this;
        std::vector<OctreeNode*> path;

        ++insertOctreeDebugInfo->processedIndices;
        insertOctreeDebugInfo->minIndex = insertOctreeDebugInfo->minIndex > objectIndex ? objectIndex : insertOctreeDebugInfo->minIndex;
        insertOctreeDebugInfo->maxIndex = insertOctreeDebugInfo->maxIndex < objectIndex ? objectIndex : insertOctreeDebugInfo->maxIndex;

        while (true)
        {
            if (!IntersectAABBPoint(bounds, objectBounds.Center()))
            {
                ++insertOctreeDebugInfo->skippedIndices;
                return;
            }

            path.push_back(currentNode);

            if (currentNode->isLeaf)
            {
                if (currentNode->objectIndices.size() < MaxObjectsPerLeaf())
                {
                    currentNode->objectIndices.push_back(objectIndex);
                    ++insertOctreeDebugInfo->acceptedIndices;
                    return;
                }
                else
                {
                    // Need to split this node
                    currentNode->SplitNode();
                    // Re-distribute existing objects
                    for (int index : currentNode->objectIndices)
                    {

                        if (index == 103)
                        {
                            ++insertOctreeDebugInfo->acceptedIndices;
                        }

                        AABB existingBounds = GetObjectBounds(index);
                        for (auto& child : currentNode->children)
                        {
                            if (IntersectAABBPoint(child->bounds, existingBounds.Center()))
                            {
                                child->objectIndices.push_back(index);
                            }
                        }
                    }
                    currentNode->objectIndices.clear();
                    // Continue to insert the new object
                }
            }

            // Find the appropriate child
            bool foundChild = false;

            for (auto& child : currentNode->children)
            {
                if (IntersectAABBPoint(child->bounds, objectBounds.Center()))
                {
                    currentNode = child;
                    foundChild  = true;
                    break;                      // Breaking here is fatal when checking for intersection with an AABB!
                }
            }

            if (!foundChild)
            {
                // Object doesn't fit in any child, insert it here
                currentNode->objectIndices.push_back(objectIndex);
                ++insertOctreeDebugInfo->skippedIndices;
                return;
            }
        }
    }
};
