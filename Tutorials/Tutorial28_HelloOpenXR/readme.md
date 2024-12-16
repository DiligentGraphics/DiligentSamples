# Tutorial28 - Hello OpenXR

This tutorial demonstrates how to use Diligent Engine with OpenXR API to render a simple scene in a VR headset.


## Prerequisites

This tutorial is based on [OpenXT Tutorial 03 by Khronos](https://openxr-tutorial.com//android/vulkan/3-graphics.html).
Please refer to the original tutorial for detailed explanation of all OpenXR-related steps.
The rest of this document focuses on Diligent Engine-specific steps and assumes that the reader is familiar with OpenXR API.


## Diligent Engine Integration

### Initialization

OpenXR instance should enable the following extensions:

* `XR_KHR_D3D11_enable` for Direct3D11
* `XR_KHR_D3D12_enable` for Direct3D12
* `XR_KHR_opengl_enable` for OpenGL
* `XR_KHR_opengl_es_enable` for OpenGL ES
* `XR_KHR_vulkan_enable2` for Vulkan

Diligent Engine should be initialized after the OpenXR instance is created and system id is obtained.
Prepare the `OpenXRAttribs` structure:

```cpp
OpenXRAttribs XRAttribs;
memcpy(&XRAttribs.Instance, &m_xrInstance, sizeof(m_xrInstance));
memcpy(&XRAttribs.SystemId, &m_xrSystemId, sizeof(m_xrSystemId));
XRAttribs.GetInstanceProcAddr = xrGetInstanceProcAddr;
```

Set the pointer to `OpenXRAttribs` structure in the engine creation info
and create the device and immediate context as usual. For example:

```cpp
EngineVkCreateInfo EngineCI;
EngineCI.pXRAttribs = &XRAttribs;

pFactoryVk->CreateDeviceAndContextsVk(EngineCI, &pDevice, &m_pImmediateContext);
```


### Creating OpenXR Session

To obtain graphics binding required to create the session, use the `GetOpenXRGraphicsBinding`
function defined in `OpenXRUtilities.h` (link with `Diligent-GraphicsTools`):

```cpp
RefCntAutoPtr<IDataBlob> pGraphicsBinding;
GetOpenXRGraphicsBinding(m_Device, m_pImmediateContext, &pGraphicsBinding);

XrSessionCreateInfo SessionCI{XR_TYPE_SESSION_CREATE_INFO};
SessionCI.next        = pGraphicsBinding->GetConstDataPtr();
SessionCI.createFlags = 0;
SessionCI.systemId    = m_xrSystemId;

xrCreateSession(m_xrInstance, &SessionCI, &m_xrSession);
```


### Creating Swapchain

To enumerate swapchain images, use the `AllocateOpenXRSwapchainImageData` function that
allocates an array of appropriate structures for each device type (`XrSwapchainImageVulkanKHR`,
`XrSwapchainImageD3D11KHR`, etc.):

```cpp
// Get the number of images in the swapchain.
uint32_t SwapchainImageCount = 0;
xrEnumerateSwapchainImages(xrSwapchain, 0, &SwapchainImageCount, nullptr);
// Allocate the memory for the swapchain image data.
RefCntAutoPtr<IDataBlob> pSwapchainImageData;
AllocateOpenXRSwapchainImageData(m_DeviceType, SwapchainImageCount, &pSwapchainImageData);
// Get the swapchain image data.
xrEnumerateSwapchainImages(xrSwapchain, SwapchainImageCount, &SwapchainImageCount,
                           pSwapchainImageData->GetDataPtr<XrSwapchainImageBaseHeader>());
```

To create a texture corresponding to the swapchain image, use the `GetOpenXRSwapchainImage` function:

```cpp
std::string Name = "Swapchain Image " + std::to_string(j);
TextureDesc ImgDesc;
ImgDesc.Name      = Name.c_str();
ImgDesc.Type      = RESOURCE_DIM_TEX_2D;
ImgDesc.Format    = Format;
ImgDesc.Width     = SwapchainCI.width;
ImgDesc.Height    = SwapchainCI.height;
ImgDesc.MipLevels = 1;
ImgDesc.BindFlags = (IsDepth ? BIND_DEPTH_STENCIL : BIND_RENDER_TARGET) | BIND_SHADER_RESOURCE;

RefCntAutoPtr<ITexture> pImage;
GetOpenXRSwapchainImage(m_Device, pSwapchainImageData->GetConstDataPtr<XrSwapchainImageBaseHeader>(), j, ImgDesc, &pImage);
```

Note that since Vulkan does not provide a way to query image properties, the `ImgDesc` structure must be filled manually.

Render target / depth stencil views can be created as usual:

```cpp
TextureViewDesc ViewDesc;
ViewDesc.ViewType = TEXTURE_VIEW_RENDER_TARGET;
RefCntAutoPtr<ITextureView> pRTV;
pImage->CreateView(ViewDesc, &pRTV);
```

### Rendering

Render target and depth stencil views obtained from the swapchain images can be used as usual.
Note that OpenXR spec guarantees that the swapchain images acquired by xrAcquireSwapchainImage are in
COLOR_ATTACHMENT_OPTIMAL/DEPTH_STENCIL_ATTACHMENT_OPTIMAL state, so you may explicitly set these
states at the beginning of the frame:

```cpp
pRTV->GetTexture()->SetState(RESOURCE_STATE_RENDER_TARGET);
pDSV->GetTexture()->SetState(RESOURCE_STATE_DEPTH_WRITE);
```

At the end of the frame, it is important to recycle resources.
Normally, this is done by the engine when the primiary swap chain is presented.
Since we are rendering to OpenXR swapchains, we need to perform these operations manually.

```cpp
m_pImmediateContext->FinishFrame();
m_Device.ReleaseStaleResources();
```
