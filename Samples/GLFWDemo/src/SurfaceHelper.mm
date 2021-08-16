#define GLFW_EXPOSE_NATIVE_COCOA 1

#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

// MoltenVk implement Vulkan API on top of Metal.
// We must add CAMetalLayer to use Metal to render into view.
// Same code implemented into glfwCreateWindowSurface, but DiligentEngine create vulkan surface inside a ISwapChain implementation and can not use the GLFW function.
void* GetNSWindowView(GLFWwindow* wnd)
{
    id Window = glfwGetCocoaWindow(wnd);
    id View   = [Window contentView];

    NSBundle* bundle = [NSBundle bundleWithPath:@"/System/Library/Frameworks/QuartzCore.framework"];
    if (!bundle)
        return nullptr;

    id Layer = [[bundle classNamed:@"CAMetalLayer"] layer];
    if (!Layer)
        return nullptr;

    [View setLayer:Layer];
    [View setWantsLayer:YES];

    // Disable retina display scaling
    CGSize viewScale = [View convertSizeToBacking: CGSizeMake(1.0, 1.0)];
    [Layer setContentsScale: MIN(viewScale.width, viewScale.height)];

    return View;
}
