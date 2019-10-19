/*     Copyright 2019 Diligent Graphics LLC
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
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

#include "PlatformDefinitions.h"
#include "SampleBase.h"
#include "Errors.h"

namespace Diligent
{

void SampleBase::GetEngineInitializationAttribs(DeviceType DevType, EngineCreateInfo& EngineCI)
{
    switch (DevType)
    {
#if D3D11_SUPPORTED
        case DeviceType::D3D11:
        {
            //EngineD3D11CreateInfo& EngineD3D11CI = static_cast<EngineD3D11CreateInfo&>(EngineCI);
        }
        break;
#endif

#if D3D12_SUPPORTED
        case DeviceType::D3D12:
        {
            EngineD3D12CreateInfo& EngineD3D12CI = static_cast<EngineD3D12CreateInfo &>(EngineCI);
            EngineD3D12CI.GPUDescriptorHeapDynamicSize[0]         = 32768;
            EngineD3D12CI.GPUDescriptorHeapSize[1]                = 128;
            EngineD3D12CI.GPUDescriptorHeapDynamicSize[1]         = 2048-128;
            EngineD3D12CI.DynamicDescriptorAllocationChunkSize[0] = 32;
            EngineD3D12CI.DynamicDescriptorAllocationChunkSize[1] = 8; // D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
        }
        break;
#endif

#if VULKAN_SUPPORTED
        case DeviceType::Vulkan:
        {
            // EngineVkCreateInfo& EngVkAttribs = static_cast<EngineVkCreateInfo&>(EngineCI);
        }
        break;
#endif

#if GL_SUPPORTED
        case DeviceType::OpenGL:
        {
            // Nothing to do
        }
        break;
#endif

#if GLES_SUPPORTED
        case DeviceType::OpenGLES:
        {
            // Nothing to do
        }
        break;
#endif

#if METAL_SUPPORTED
        case DeviceType::Metal:
        {
            // Nothing to do
        }
        break;
#endif

        default:
            LOG_ERROR_AND_THROW("Unknown device type");
            break;
    }
}

}
