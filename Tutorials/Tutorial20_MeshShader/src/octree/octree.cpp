#include "octree.h"
#include "DrawTask.h"

std::vector<DrawTask> ObjectBuffer = {};

AABB GetObjectBounds(int index)
{
    DrawTask task = ObjectBuffer.at(index);

    DirectX::XMVECTOR voxelSizeOffset = {task.BasePosAndScale.w * 2, task.BasePosAndScale.w * 2, task.BasePosAndScale.w * 2};

    auto minBoundVec = DirectX::XMVectorSubtract({task.BasePosAndScale.x, task.BasePosAndScale.y, task.BasePosAndScale.z}, voxelSizeOffset);
    auto maxBoundVec = DirectX::XMVectorAdd({task.BasePosAndScale.x, task.BasePosAndScale.y, task.BasePosAndScale.z}, voxelSizeOffset);

    DirectX::XMFLOAT3 minBound;
    DirectX::XMFLOAT3 maxBound;

    DirectX::XMStoreFloat3(&minBound, minBoundVec);
    DirectX::XMStoreFloat3(&maxBound, maxBoundVec);

    return {DirectX::XMFLOAT3{}, DirectX::XMFLOAT3{}};
}

template<typename T>
void OctreeNode<T>::SplitNode()
{
    if (!isLeaf) return;

    DirectX::XMFLOAT3 center = 
    {
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

        children[i] = new OctreeNode({newMin, newMax});
    }

    isLeaf = false;
}
    
template<typename T>
void OctreeNode<T>::InsertObject(int objectIndex, const AABB& objectBounds)
{
    // Check if object needs to be inside this node
    if (!Intersects(bounds, objectBounds))
        return;

    if (isLeaf && objectIndices.size() < MaxObjectsPerLeaf())
    {
        objectIndices.push_back(objectIndex);
    }
    else
    {
        if (isLeaf)
        {
            SplitNode();
                
            // Insert existing objects in child nodes
            for (int index : objectIndices)
            {
                // You'd need to get the bounds for this object from somewhere
                AABB existingObjectBounds = GetObjectBounds(index);

                for (auto& child : children)
                    child->InsertObject(index, existingObjectBounds);
                
            }
            objectIndices.clear(); // Clear indices of the splitted node
        }

        // Insert new object in child nodes
        for (auto& child : children)
        {
            child->InsertObject(objectIndex, objectBounds);
        }
    }
}



/// <summary>
/// Checks if two AABBs intersect each other.
/// </summary>
/// <param name="first">The first AABB to check against.</param>
/// <param name="second">The second AABB to check against.</param>
/// <returns>True if both AABBs do intersect. False if both AABBs do not intersect.</returns>
bool Intersects(const AABB& first, const AABB& second)
{
    return (first.min.x <= second.max.x && first.max.x >= second.min.x) &&
        (first.min.y <= second.max.y && first.max.y >= second.min.y) &&
        (first.min.z <= second.max.z && first.max.z >= second.min.z);
}
