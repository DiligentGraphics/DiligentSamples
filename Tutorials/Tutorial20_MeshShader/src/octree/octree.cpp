#include "octree.h"

std::vector<VoxelOC::DrawTask> ObjectBuffer;

AABB GetObjectBounds(int index)
{
    VoxelOC::DrawTask task = ObjectBuffer.at(index);

    DirectX::XMVECTOR voxelSizeOffset = {task.BasePosAndScale.w, task.BasePosAndScale.w, task.BasePosAndScale.w};

    auto minBoundVec = DirectX::XMVectorSubtract({task.BasePosAndScale.x, task.BasePosAndScale.y, task.BasePosAndScale.z}, voxelSizeOffset);
    auto maxBoundVec = DirectX::XMVectorAdd({task.BasePosAndScale.x, task.BasePosAndScale.y, task.BasePosAndScale.z}, voxelSizeOffset);

    DirectX::XMFLOAT3 minBound;
    DirectX::XMFLOAT3 maxBound;

    DirectX::XMStoreFloat3(&minBound, minBoundVec);
    DirectX::XMStoreFloat3(&maxBound, maxBoundVec);

    return {DirectX::XMFLOAT3{}, DirectX::XMFLOAT3{}};
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
