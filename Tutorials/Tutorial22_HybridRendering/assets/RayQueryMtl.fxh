#include <metal_stdlib>
#include <simd/simd.h>
#include <metal_raytracing>

using namespace metal;
using namespace raytracing;

enum RAY_FLAG : uint
{
    RAY_FLAG_NONE                               = 0x00,
    RAY_FLAG_FORCE_OPAQUE                       = 0x01,
    RAY_FLAG_FORCE_NON_OPAQUE                   = 0x02,
    RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH    = 0x04,
    RAY_FLAG_SKIP_CLOSEST_HIT_SHADER            = 0x08,
    RAY_FLAG_CULL_BACK_FACING_TRIANGLES         = 0x10,
    RAY_FLAG_CULL_FRONT_FACING_TRIANGLES        = 0x20,
    RAY_FLAG_CULL_OPAQUE                        = 0x40,
    RAY_FLAG_CULL_NON_OPAQUE                    = 0x80,
    RAY_FLAG_SKIP_TRIANGLES                     = 0x100, // DXR 1.1
    RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES         = 0x200, // DXR 1.1
};

enum COMMITTED_STATUS : uint
{
    COMMITTED_NOTHING,
    COMMITTED_TRIANGLE_HIT,
    COMMITTED_PROCEDURAL_PRIMITIVE_HIT
};

struct RayDesc
{
    float3  Origin;
    float3  Direction;
    float   TMin;
    float   TMax;
};


// Wrapper for Metal ray tracing api which is emulates the HLSL RayQuery class (inline ray tracing).
template <uint TRayFlag>
struct RayQuery
{
private:
    intersector< triangle_data, instancing >          m_RTIntersector;
    intersection_result< triangle_data, instancing >  m_LastIntersection;

    ray     m_CurrentRay;
    uint    m_RayFlags = 0;

public:
    RayQuery ()
    {}

    void TraceRayInline(instance_acceleration_structure AccelerationStructure,
                        uint                            RayFlags,               // RAY_FLAG
                        uint                            InstanceInclusionMask,
                        const RayDesc                   Ray)
    {
        m_CurrentRay.origin       = Ray.Origin;
        m_CurrentRay.direction    = Ray.Direction;
        m_CurrentRay.min_distance = Ray.TMin;
        m_CurrentRay.max_distance = Ray.TMax;

        m_RayFlags = TRayFlag | RayFlags;

        m_RTIntersector.set_triangle_cull_mode(m_RayFlags & RAY_FLAG_CULL_BACK_FACING_TRIANGLES  ? triangle_cull_mode::back :
                                               m_RayFlags & RAY_FLAG_CULL_FRONT_FACING_TRIANGLES ? triangle_cull_mode::front :
                                                                                                   triangle_cull_mode::none);
        m_RTIntersector.set_geometry_cull_mode(m_RayFlags & RAY_FLAG_SKIP_TRIANGLES             ? geometry_cull_mode::triangle :
                                               m_RayFlags & RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES ? geometry_cull_mode::bounding_box :
                                                                                                  geometry_cull_mode::none);

        // override per-instance and per-geometry setting 
        m_RTIntersector.force_opacity(m_RayFlags & RAY_FLAG_FORCE_OPAQUE     ? forced_opacity::opaque :
                                      m_RayFlags & RAY_FLAG_FORCE_NON_OPAQUE ? forced_opacity::non_opaque :
                                                                               forced_opacity::none);

        // If the following function is set to true, set the intersector object to search for any intersection. If not, it will search for the closest.
        m_RTIntersector.accept_any_intersection((m_RayFlags & RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH) != 0);
        
        m_LastIntersection = m_RTIntersector.intersect(m_CurrentRay,
                                                       AccelerationStructure,
                                                       InstanceInclusionMask); // mask requires 'instancing' flag
    }

    bool Proceed()
    {
        // TRUE means there is a candidate for a hit that requires shader consideration.
        // FALSE means that the search for hit candidates is finished.
        return false;
    }

    float3 WorldRayOrigin()     { return m_CurrentRay.origin; }
    float3 WorldRayDirection()  { return m_CurrentRay.direction; }
    uint   RayFlags()           { return m_RayFlags; }
    float  RayTMin()            { return m_CurrentRay.min_distance; }

    COMMITTED_STATUS CommittedStatus()
    {
        switch (m_LastIntersection.type)
        {
            case intersection_type::triangle:     return COMMITTED_TRIANGLE_HIT;
            case intersection_type::bounding_box: return COMMITTED_PROCEDURAL_PRIMITIVE_HIT;
            case intersection_type::none:         break;
        }
        return COMMITTED_NOTHING;
    }

    float2 CommittedTriangleBarycentrics() { return m_LastIntersection.triangle_barycentric_coord; } // requires 'triangle_data' flag
    bool   CommittedTriangleFrontFace()    { return m_LastIntersection.triangle_front_facing; }      // requires 'triangle_data' flag
    float  CommittedRayT()                 { return m_LastIntersection.distance; }
    uint   CommittedInstanceID()           { return m_LastIntersection.instance_id; }
    uint   CommittedGeometryIndex()        { return m_LastIntersection.geometry_id; }
    uint   CommittedPrimitiveIndex()       { return m_LastIntersection.primitive_id; }
};

#define mul(lhs, rhs)                    ((lhs) * (rhs))
#define lerp(x, y, factor)               mix((x), (y), (factor))
#define RaytracingAccelerationStructure  instance_acceleration_structure
