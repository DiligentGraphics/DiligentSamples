#pragma once

#include <DirectXMath.h>
#include <array>
#include <vector>
#include <set>
#include "DrawTask.h"

struct AABB
{
    DirectX::XMFLOAT3 min = {};
    DirectX::XMFLOAT3 max = {};
};

extern std::vector<VoxelOC::DrawTask> ObjectBuffer;

constexpr int MaxObjectsPerLeaf() { return 64; }
bool Intersects(const AABB& first, const AABB& second);
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
    DebugInfo* insertObjectDebugInfo = nullptr;

    OctreeNode(const AABB& bounds, DebugInfo* getGridIdicesDebugInfo, DebugInfo* insertObjectDebugInfo) :
        bounds(bounds), isLeaf(true), getGridIndicesDebugInfo(getGridIdicesDebugInfo), insertObjectDebugInfo(insertObjectDebugInfo)
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

    void GetAllGridIndices(std::vector<int>& gridIndexBuffer, std::vector<char>& duplicateBuffer)
    {
        // Check for children (Buttom Up)
        if (childOccupationMask > 0)
        {
            for (int i = 0; i < children.size(); ++i)
            {
                //if (childOccupationMask & (0b00000001 << i))
                if (children[i] != nullptr)
                    children[i]->GetAllGridIndices(gridIndexBuffer, duplicateBuffer);
            }
        }

        // Add own indices if present
        for (int index = 0; index < objectIndices.size(); ++index)
        {
            ++getGridIndicesDebugInfo->processedIndices;

            if (duplicateBuffer[objectIndices[index]] == 0)
            {
                gridIndexBuffer.push_back(objectIndices[index]) ;
                duplicateBuffer[objectIndices[index]] = 1;
                ++getGridIndicesDebugInfo->acceptedIndices;

                getGridIndicesDebugInfo->maxIndex = getGridIndicesDebugInfo->maxIndex < objectIndices[index] ? objectIndices[index] : getGridIndicesDebugInfo->maxIndex;
                getGridIndicesDebugInfo->minIndex = getGridIndicesDebugInfo->minIndex > objectIndices[index] ? objectIndices[index] : getGridIndicesDebugInfo->minIndex;
            }
            else 
            {
                ++getGridIndicesDebugInfo->skippedIndices;
                getGridIndicesDebugInfo->maxIndexSkipped = getGridIndicesDebugInfo->maxIndexSkipped < objectIndices[index] ? objectIndices[index] : getGridIndicesDebugInfo->maxIndexSkipped;
                getGridIndicesDebugInfo->minIndexSkipped = getGridIndicesDebugInfo->minIndexSkipped > objectIndices[index] ? objectIndices[index] : getGridIndicesDebugInfo->minIndexSkipped;         
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
            DirectX::XMFLOAT3 newMin, newMax;

            newMin.x = (i & 1) ? center.x : bounds.min.x;
            newMin.y = (i & 2) ? center.y : bounds.min.y;
            newMin.z = (i & 4) ? center.z : bounds.min.z;
            newMax.x = (i & 1) ? bounds.max.x : center.x;
            newMax.y = (i & 2) ? bounds.max.y : center.y;
            newMax.z = (i & 4) ? bounds.max.z : center.z;

            children[i] = new OctreeNode({newMin, newMax}, getGridIndicesDebugInfo, insertObjectDebugInfo);
            childOccupationMask |= 0b00000001 << i;
        }

        isLeaf = false;
    }

    void InsertObject(int objectIndex, const AABB objectBounds)
    {
        ++insertObjectDebugInfo->processedIndices;
        // Check if object needs to be inside this node
        if (!Intersects(bounds, objectBounds))
            return;

        if (isLeaf && objectIndices.size() < MaxObjectsPerLeaf())
        {
            objectIndices.push_back(objectIndex);
            ++insertObjectDebugInfo->acceptedIndices;
            
            insertObjectDebugInfo->maxIndex = 
                insertObjectDebugInfo->maxIndex < objectIndex ? objectIndex : insertObjectDebugInfo->maxIndex;

            insertObjectDebugInfo->minIndex =
                insertObjectDebugInfo->minIndex > objectIndex ? objectIndex : insertObjectDebugInfo->minIndex;
        }
        else
        {
            if (isLeaf)
            {
                ++insertObjectDebugInfo->skippedIndices;

                SplitNode();

                // Insert existing objects in child nodes
                for (int index : objectIndices)
                {
                    // You'd need to get the bounds for this object from somewhere
                    AABB existingObjectBounds = GetObjectBounds(index);

                    for (auto& child : children)
                        child->InsertObject(index, existingObjectBounds);
                }
                contentOccupationMask = 0;
                objectIndices.clear(); // Clear indices of the splitted node
            }

            // Insert new object in child nodes
            for (auto& child : children)
            {
                child->InsertObject(objectIndex, objectBounds);
            }
        }
    }
};
