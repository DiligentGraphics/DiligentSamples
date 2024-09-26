#pragma once
#include <array>
#include <vector>
#include <concepts>

// 1 byte
struct BitMask
{
    BitMask(uint8_t mask = 0) :
        Mask(mask)
    { }

    ~BitMask()
    {
        Reset();
    }

    inline bool IsBitSet(int bitPosition) const
    {
        return Mask & 1 << bitPosition;
    }

    inline void SetBit(int bitPosition)
    {
        Mask |= 1 << bitPosition;
    }

    void Reset()
    {
        Mask = 0;
    }

    bool operator==(const BitMask& other) const
    {
        return Mask == other.Mask;
    }

    bool operator!=(const BitMask& other) const
    {
        return Mask != other.Mask;
    }

    bool operator==(const int other) const
    {
        return Mask == other;
    }

    bool operator!=(const int other) const
    {
        return Mask != other;
    }

    bool operator<(const BitMask& other) const
    {
        return Mask < other.Mask;
    }

    bool operator>(const BitMask& other) const
    {
        return Mask > other.Mask;
    }

    bool operator<(const int other) const
    {
        return Mask < other;
    }

    bool operator>(const int other) const
    {
        return Mask > other;
    }
    
    uint8_t Mask = 0;
};

class SparseVoxelDAG;

// 64 bytes
struct DAGNode
{
    DAGNode(uint8_t layer = 0) :
        Layer(layer)
    { }

    DAGNode(const DAGNode& other) :
        Layer(other.Layer), 
        Childmask(other.Childmask), 
        Children(other.Children)
    { }

    DAGNode(DAGNode&& other) noexcept : Layer(other.Layer), Childmask(other.Childmask), Children(other.Children)
    {
        other.Reset();
    }

    ~DAGNode()
    {
        Reset();
    }

    void Reset()
    {
        Childmask.Reset();

        for (auto& childIdx : Children)
        {
            childIdx = -1;
        }
    }

    // Copy assignment operator
    DAGNode& operator=(const DAGNode& other)
    {
        Layer = other.Layer;
        Childmask = other.Childmask;
        Children  = other.Children;
    }

    // Move assignment operator
    DAGNode& operator=(DAGNode&& other) noexcept
    {
        Layer     = other.Layer;
        Childmask = other.Childmask;
        Children  = other.Children;

        other.Reset();
    }

    bool operator==(const DAGNode& other) const
    {
        // Return false if child occupation is not equal
        if (Childmask != other.Childmask)
            return false;

        // Return false if childs are not equal
        for (int i = 0; i < Children.size(); ++i)
        {
            if (Children[i] != other.Children[i])
                return false;
        }

        return true;
    }

    bool operator!=(const DAGNode& other) const
    {
        // Return true if child occupation is not equal
        if (Childmask != other.Childmask)
            return true;

        // Return true if childs are not equal
        for (int i = 0; i < Children.size(); ++i)
        {
            if (Children[i] != other.Children[i])
                return true;
        }

        return false;
    }

    void AddNode(unsigned int childIdx, std::vector<std::vector<DAGNode>>& g_NodeData)
    {
        if (Childmask.IsBitSet(childIdx)) // If this node already has a child in this position, put it into this node
        {
            g_NodeData.at(Layer + 1).at(Children.at(childIdx)).AddNode(childIdx, g_NodeData);
            return;
        }

        Childmask.SetBit(childIdx);                                 // Add child to bitmask
        Children.at(childIdx) = g_NodeData.at(Layer + 1).size();    // Add the next index in the global data buffer
        g_NodeData.at(Layer + 1).push_back(DAGNode(Layer + 1));     // Add the new child node into the global data buffer
    }

    bool IsLeaf() const
    {
        return Childmask == 0;
    }

    bool IsRoot() const
    {
        return Layer == 0;
    }

    uint8_t Layer = 0;   // Layer of this node -> corresponds to the index of the layer arrays
    BitMask Childmask{}; // Bitmask for children
    std::array<int64_t, 8> Children{}; // Indices to children
};

template <typename T>
struct Vector3
{
    T x, y, z = {};

    Vector3()
    {
        x = {};
        y = {};
        z = {};
    }

    Vector3(T x, T y, T z) :
        x(x), y(y), z(z)
    { }


    bool operator<(const Vector3<T>& other) const
    {
        if (x != other.x)
            return x < other.x;

        if (y != other.y)
            return y < other.y;

        if (z != other.z)
            return z < other.z;

        return false;
    }

    bool operator==(const Vector3<T>& other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }
};

template<typename T>
struct Vector4
{
    T x, y, z, w;

    Vector4()
    {
        x = {};
        y = {};
        z = {};
        w = {};
    }

    bool operator<(const Vector4<T>& other) const
    {
        if (x != other.x)
            return x < other.x;

        if (y != other.y)
            return y < other.y;

        if (z != other.z)
            return z < other.z;

        if (w != other.w)
            return w < other.w;

        return false;
    }

    bool operator==(const Vector4<T>& other) const
    {
        return x == other.x && y == other.y && z == other.z && w == other.w;
    }
};

struct AABB
{
    Vector3<float> min = {};
    Vector3<float> max = {};
};

// Upper Left Front: -> child 0
// Upper Left Back: -> child 1
// Upper Right Front: -> child 2
// Upper Right Back: -> child 3

// Lower Left Front: -> child 4
// Lower Left Back: -> child 5
// Lower Right Front: -> child 6
// Lower Right Back: -> child 7

class SparseVoxelDAG
{
public:

    SparseVoxelDAG(AABB dimensions, uint32_t graphDepth = 10) :
        bounds(dimensions), graphDepth(graphDepth)
    {
        // Create DAG data
        p_GraphData = new std::vector<std::vector<DAGNode>>(graphDepth);

        // Create root
        p_GraphData->at(0).push_back(DAGNode());
        p_RootNode = &p_GraphData->at(0).at(0);
    }

    bool Intersects(const AABB& first, const AABB& second)
    {
        return (first.min.x <= second.max.x && first.max.x >= second.min.x) &&
            (first.min.y <= second.max.y && first.max.y >= second.min.y) &&
            (first.min.z <= second.max.z && first.max.z >= second.min.z);
    }

    void InsertVoxel(AABB voxelDimension)
    {
        if (!Intersects(bounds, voxelDimension))
            return;

        Vector3<float> center = {
            (voxelDimension.min.x + voxelDimension.max.x) * 0.5f,
            (voxelDimension.min.y + voxelDimension.max.y) * 0.5f,
            (voxelDimension.min.z + voxelDimension.max.z) * 0.5f
        };

        for (int i = 0; i < 8; ++i)
        {
            Vector3<float> newMin, newMax;

            newMin.x = (i & 1) ? center.x : bounds.min.x;
            newMin.y = (i & 2) ? center.y : bounds.min.y;
            newMin.z = (i & 4) ? center.z : bounds.min.z;
            newMax.x = (i & 1) ? bounds.max.x : center.x;
            newMax.y = (i & 2) ? bounds.max.y : center.y;
            newMax.z = (i & 4) ? bounds.max.z : center.z;

            if (Intersects(AABB{ newMin, newMax }, voxelDimension))
            {
                p_RootNode->AddNode(i, *p_GraphData);
                return;
            }
        }
    }

public:

    // Data container
    std::vector<std::vector<DAGNode>>* p_GraphData = nullptr;
    
    DAGNode* p_RootNode = nullptr;

private:

    // General data
    AABB           bounds{};
    uint32_t       graphDepth = 0;
    Vector3<float> basePosition{};
};
