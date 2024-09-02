/*
 *  Copyright 2019-2024 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#pragma once

#include <mutex>
#include <vector>
#include <unordered_map>
#include <atomic>

#include "../../../DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "../../../DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "../../../DiligentCore/Common/interface/RefCntAutoPtr.hpp"
#include "../../../DiligentCore/Common/interface/ObjectBase.hpp"
#include "../../../DiligentCore/Graphics/GraphicsTools/interface/BufferSuballocator.h"
#include "../../../DiligentCore/Graphics/GraphicsTools/interface/DynamicTextureAtlas.h"
#include "../../../DiligentCore/Graphics/GraphicsTools/interface/VertexPoolX.hpp"

namespace Diligent
{

namespace GLTF
{

/// GLTF resource manager
class ResourceManager final : public ObjectBase<IObject>
{
public:
    using TBase = ObjectBase<IObject>;

    /// Vertex layout key used to select the vertex pool.
    ///
    /// \remarks
    ///     When vertex data is split between multiple buffers, the offsets in each buffer must be
    ///     consistent. For example, suppose we store position in buffer 0 (12 bytes) and normals + UVs
    ///     in buffer 1 (20 bytes).
    ///     If the the first allocation contains 100 vertices, the offsets for the second allocation will be
    ///     1200 and 2000 bytes correspondingly.
    ///     If these offsets are not consistent, the vertex shader will read incorrect data.
    ///     Vertex layout key is used to group compatible layouts in the same vertex pool.
    struct VertexLayoutKey
    {
        struct ElementDesc
        {
            const Uint32     Size;
            const BIND_FLAGS BindFlags;

            constexpr ElementDesc(Uint32 _Size, BIND_FLAGS _BindFlags) noexcept :
                Size{_Size},
                BindFlags{_BindFlags}
            {}

            constexpr bool operator==(const ElementDesc& RHS) const
            {
                return Size == RHS.Size && BindFlags == RHS.BindFlags;
            }
            constexpr bool operator!=(const ElementDesc& RHS) const
            {
                return !(*this == RHS);
            }
        };
        std::vector<ElementDesc> Elements;

        bool operator==(const VertexLayoutKey& rhs) const
        {
            return Elements == rhs.Elements;
        }
        bool operator!=(const VertexLayoutKey& rhs) const
        {
            return Elements != rhs.Elements;
        }
        explicit operator bool() const
        {
            return Elements.empty();
        }

        struct Hasher
        {
            size_t operator()(const VertexLayoutKey& Key) const;
        };
    };

    /// Default vertex pool description that is used to create vertex pools
    /// not explicitly specified in the create info.
    struct DefaultVertexPoolDesc
    {
        /// The name of the vertex pool.
        const char* Name = nullptr;

        /// The initial vertex count in the pool.
        /// If zero, additional vertex pools will not be created.
        Uint32 VertexCount = 0;

        /// The vertex pool buffers usage.
        USAGE Usage = USAGE_DEFAULT;

        /// The vertex pool buffers CPU access flags.
        CPU_ACCESS_FLAGS CPUAccessFlags = CPU_ACCESS_NONE;

        /// The vertex pool buffers mode.
        BUFFER_MODE Mode = BUFFER_MODE_UNDEFINED;
    };

    /// Resource manager create info.
    struct CreateInfo
    {
        /// Index buffer suballocator create info.
        BufferSuballocatorCreateInfo IndexAllocatorCI;

        /// A pointer to an array of NumVertexPools vertex pool create infos.
        const VertexPoolCreateInfo* pVertexPoolCIs = nullptr;

        /// A pointer to an array of NumTexAtlases texture atlas create infos.
        const DynamicTextureAtlasCreateInfo* pTexAtlasCIs = nullptr;

        /// The number of elements in pVertexPoolCIs array.
        Uint32 NumVertexPools = 0;

        /// The number of elements in pTexAtlasCIs array.
        Uint32 NumTexAtlases = 0;

        /// Default texture atlas description that is used to create texture
        /// atlas not explicitly specified in pTexAtlasCIs.
        /// If DefaultAtlasDesc.Desc.Type is RESOURCE_DIM_UNDEFINED,
        /// additional atlases will not be created.
        DynamicTextureAtlasCreateInfo DefaultAtlasDesc;

        /// Default vertex pool description that is used to create vertex pools
        /// not explicitly specified in pVertexPoolCIs.
        /// If DefaultPoolDesc.VertexCount is 0, additional pools will not be
        /// created.
        DefaultVertexPoolDesc DefaultPoolDesc;
    };

    /// Creates a new resource manager instance.
    static RefCntAutoPtr<ResourceManager> Create(IRenderDevice*    pDevice,
                                                 const CreateInfo& CI);

    /// Allocates texture space in the texture atlas that matches the specified format.

    /// \param[in]  Fmt       - Texture format.
    /// \param[in]  Width     - Texture width.
    /// \param[in]  Height    - Texture height.
    /// \param[in]  CacheId   - Optional cache ID.
    /// \param[in]  pUserData - Optional user data to set in the texture atlas suballocation.
    ///
    /// \remarks    If the texture atlas for the given format does not exist and if the default
    ///             atlas description allows creating new atlases (Desc.Type != RESOURCE_DIM_UNDEFINED),
    ///             new atlas will be added. Otherwise, the function will return null.
    RefCntAutoPtr<ITextureAtlasSuballocation> AllocateTextureSpace(TEXTURE_FORMAT Fmt,
                                                                   Uint32         Width,
                                                                   Uint32         Height,
                                                                   const char*    CacheId   = nullptr,
                                                                   IObject*       pUserData = nullptr);

    /// Finds texture allocation in the texture atlas that matches the specified cache ID.
    RefCntAutoPtr<ITextureAtlasSuballocation> FindTextureAllocation(const char* CacheId);

    /// Allocates indices in the index buffer.
    RefCntAutoPtr<IBufferSuballocation> AllocateIndices(Uint32 Size, Uint32 Alignment = 4);

    /// Allocates vertices in the vertex pool that matches the specified layout.

    /// \param[in]  LayoutKey   - Vertex layout key, see VertexLayoutKey.
    /// \param[in]  VertexCount - The number of vertices to allocate.
    ///
    /// \remarks    If the vertex pool for the given key does not exist and if the default
    ///             pool description allows creating new pools (VertexCount != 0),
    ///             new pool will be added.
    ///
    ///             If existing pools run out of space, a new vertex pool will be created and
    ///             vertices will be allocated from this pool.
    ///
    ///             If no pull exists for the given key and the default
    ///             pool description does not allow creating new pools
    ///             (VertexCount == 0), the function returns null.
    RefCntAutoPtr<IVertexPoolAllocation> AllocateVertices(const VertexLayoutKey& LayoutKey, Uint32 VertexCount);


    /// Returns the combined texture atlas version, i.e. the sum of the texture versions of all
    /// atlases.
    Uint32 GetTextureVersion() const;

    /// Returns the index buffer version.
    Uint32 GetIndexBufferVersion() const;

    /// Returns the combined vertex pool version, i.e. the sum all vertex pool versions.
    Uint32 GetVertexPoolsVersion() const;

    /// Updates the index buffer, if necessary.
    IBuffer* UpdateIndexBuffer(IRenderDevice* pDevice, IDeviceContext* pContext, Uint32 Index = 0);

    /// Updates all index buffers.
    void UpdateIndexBuffers(IRenderDevice* pDevice, IDeviceContext* pContext);

    /// Returns the number of index buffers.
    size_t GetIndexBufferCount() const;

    /// Returns the index allocator index.
    Uint32 GetIndexAllocatorIndex(IBufferSuballocator* pAllocator) const;

    /// Updates the vertex buffers, if necessary.
    void UpdateVertexBuffers(IRenderDevice* pDevice, IDeviceContext* pContext);

    /// Returns a pointer to the index buffer.
    IBuffer* GetIndexBuffer(Uint32 Index = 0) const;

    /// Returns a pointer to the vertex pool for the given key and index.
    /// If the pool does not exist, null is returned.
    ///
    /// \remarks    If multiple vertex pools with the same key may exist,
    ///             an application can use the GetVertexPools() method to
    ///             get all pools for the given key.
    IVertexPool* GetVertexPool(const VertexLayoutKey& Key, Uint32 Index = 0);

    /// Returns the number of vertex pools for the given key.
    size_t GetVertexPoolCount(const VertexLayoutKey& Key) const;

    /// Returns all vertex pools for the given key.
    std::vector<IVertexPool*> GetVertexPools(const VertexLayoutKey& Key) const;

    /// Returns index of the vertex pool with the give key.
    /// If the pool does not exist, InvalidIndex (0xFFFFFFFF) is returned.
    Uint32 GetVertexPoolIndex(const VertexLayoutKey& Key, IVertexPool* pPool) const;

    /// Updates the atlas texture for the given format.
    /// If the atlas does not exist, null is returned.
    ITexture* UpdateTexture(TEXTURE_FORMAT Fmt, IRenderDevice* pDevice, IDeviceContext* pContext);

    /// Updates all atlas textures.
    void UpdateTextures(IRenderDevice* pDevice, IDeviceContext* pContext);

    /// Returns the atlas texture for the given format.
    /// If the atlas does not exist, null is returned.
    ITexture* GetTexture(TEXTURE_FORMAT Fmt) const;

    /// Updates all vertex buffers, index buffer and atlas textures.
    ///
    /// \remarks    This method is equivalent to calling UpdateIndexBuffer(),
    ///             UpdateVertexBuffers() and UpdateTextures().
    void UpdateAllResources(IRenderDevice* pDevice, IDeviceContext* pContext);

    // NB: can't return reference here!
    TextureDesc GetAtlasDesc(TEXTURE_FORMAT Fmt);

    /// Returns the texture atlas allocation alignment for the given format.
    Uint32 GetAllocationAlignment(TEXTURE_FORMAT Fmt, Uint32 Width, Uint32 Height);

    /// Returns the index buffer usage stats.
    BufferSuballocatorUsageStats GetIndexBufferUsageStats();

    /// Returns the texture atlas usage stats.
    ///
    /// If fmt is not TEX_FORMAT_UNKNOWN, returns the stats for the atlas matching the specified format.
    /// Otherwise, returns the net usage stats for all atlases.
    DynamicTextureAtlasUsageStats GetAtlasUsageStats(TEXTURE_FORMAT Fmt = TEX_FORMAT_UNKNOWN);

    /// Returns the vertex pool usage stats.
    ///
    /// If the key is not equal the default key, returns the stats for the vertex pool matching the key.
    /// Otherwise, returns the net usage stats for all pools.
    VertexPoolUsageStats GetVertexPoolUsageStats(const VertexLayoutKey& Key = VertexLayoutKey{});

    /// Parameters of the TransitionResourceStates() method.
    struct TransitionResourceStatesInfo
    {
        /// Vertex buffers transition info.
        struct VertexBuffersInfo
        {
            /// Old state that is passed to the OldState member of the StateTransitionDesc structure.
            RESOURCE_STATE OldState = RESOURCE_STATE_UNKNOWN;

            /// New state that is passed to the NewState member of the StateTransitionDesc structure.
            ///
            /// If NewState is RESOURCE_STATE_UNKNOWN, the vertex buffers states will not be changed.
            RESOURCE_STATE NewState = RESOURCE_STATE_UNKNOWN;

            /// Flags that are passed to the Flags member of the StateTransitionDesc structure.
            STATE_TRANSITION_FLAGS Flags = STATE_TRANSITION_FLAG_UPDATE_STATE;
        } VertexBuffers;

        /// Index buffer transition info.
        struct IndexBufferInfo
        {
            /// Old state that is passed to the OldState member of the StateTransitionDesc structure.
            RESOURCE_STATE OldState = RESOURCE_STATE_UNKNOWN;

            /// New state that is passed to the NewState member of the StateTransitionDesc structure.
            ///
            /// If NewState is RESOURCE_STATE_UNKNOWN, the index buffer state will not be changed.
            RESOURCE_STATE NewState = RESOURCE_STATE_UNKNOWN;

            /// Flags that are passed to the Flags member of the StateTransitionDesc structure.
            STATE_TRANSITION_FLAGS Flags = STATE_TRANSITION_FLAG_UPDATE_STATE;
        } IndexBuffer;

        /// Texture atlases transition info.
        struct TextureAtlasesInfo
        {
            /// Old state that is passed to the OldState member of the StateTransitionDesc structure.
            RESOURCE_STATE OldState = RESOURCE_STATE_UNKNOWN;

            /// New state that is passed to the NewState member of the StateTransitionDesc structure.
            ///
            /// If NewState is RESOURCE_STATE_UNKNOWN, the texture atlases states will not be changed.
            RESOURCE_STATE NewState = RESOURCE_STATE_UNKNOWN;

            /// Flags that are passed to the Flags member of the StateTransitionDesc structure.
            STATE_TRANSITION_FLAGS Flags = STATE_TRANSITION_FLAG_UPDATE_STATE;
        } TextureAtlases;
    };

    /// Transitions resource states of all vertex buffers, index buffer and texture atlases.
    ///
    /// \param[in]  pDevice  - Pointer to the render device.
    /// \param[in]  pContext - Pointer to the device context.
    /// \param[in]  Info     - Resource state transition info, see Diligent::ResourceManager::TransitionResourceStatesInfo.
    ///
    /// \remarks    This function is thread-safe.
    void TransitionResourceStates(IRenderDevice* pDevice, IDeviceContext* pContext, const TransitionResourceStatesInfo& Info);

    /// Returns the formats of the allocated texture atlases.
    std::vector<TEXTURE_FORMAT> GetAllocatedAtlasFormats() const;

private:
    template <typename AllocatorType, typename ObjectType>
    friend class Diligent::MakeNewRCObj;

    ResourceManager(IReferenceCounters* pRefCounters,
                    IRenderDevice*      pDevice,
                    const CreateInfo&   CI);

    RefCntAutoPtr<IVertexPool>         CreateVertexPoolForLayout(const VertexLayoutKey& Key) const;
    RefCntAutoPtr<IBufferSuballocator> CreateIndexBufferAllocator(IRenderDevice* pDevice) const;

private:
    const RENDER_DEVICE_TYPE m_DeviceType;

    const std::string     m_DefaultVertPoolName;
    DefaultVertexPoolDesc m_DefaultVertPoolDesc;

    const std::string             m_DefaultAtlasName;
    DynamicTextureAtlasCreateInfo m_DefaultAtlasDesc;

    const BufferSuballocatorCreateInfo m_IndexAllocatorCI;

    mutable std::mutex                              m_IndexAllocatorsMtx;
    std::vector<RefCntAutoPtr<IBufferSuballocator>> m_IndexAllocators;

    std::unordered_map<VertexLayoutKey, VertexPoolCreateInfoX, VertexLayoutKey::Hasher> m_VertexPoolCIs;

    using VertexPoolsHashMapType = std::unordered_map<VertexLayoutKey, std::vector<RefCntAutoPtr<IVertexPool>>, VertexLayoutKey::Hasher>;
    mutable std::mutex     m_VertexPoolsMtx;
    VertexPoolsHashMapType m_VertexPools;

    using AtlasesHashMapType = std::unordered_map<TEXTURE_FORMAT, RefCntAutoPtr<IDynamicTextureAtlas>, std::hash<Uint32>>;
    mutable std::mutex m_AtlasesMtx;
    AtlasesHashMapType m_Atlases;

    using TexAllocationsHashMapType = std::unordered_map<std::string, RefCntWeakPtr<ITextureAtlasSuballocation>>;
    std::mutex                m_TexAllocationsMtx;
    TexAllocationsHashMapType m_TexAllocations;

    std::vector<StateTransitionDesc> m_Barriers;
};

} // namespace GLTF

} // namespace Diligent
