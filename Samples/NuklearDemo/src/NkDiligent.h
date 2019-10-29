/*     Copyright 2019 Diligent Graphics LLC
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

#pragma once 

namespace Diligent
{
    class IRenderDevice;
    class IDeviceContext;
    enum TEXTURE_FORMAT : uint16_t;
}

NK_API struct nk_diligent_context* nk_diligent_init(Diligent::IRenderDevice* device,
                                                    unsigned int             width,
                                                    unsigned int             height,
                                                    Diligent::TEXTURE_FORMAT BackBufferFmt,
                                                    Diligent::TEXTURE_FORMAT DepthBufferFmt,
                                                    unsigned int             max_vertex_buffer_size,
                                                    unsigned int             max_index_buffer_size);

NK_API struct nk_context* nk_diligent_get_nk_ctx(struct nk_diligent_context* nk_dlg_ctx);

NK_API void nk_diligent_font_stash_begin(struct nk_diligent_context* nk_dlg_ctx,
                                         struct nk_font_atlas**      atlas);

NK_API void nk_diligent_font_stash_end(struct nk_diligent_context* nk_dlg_ctx,
                                       Diligent::IDeviceContext*   device_ctx);

NK_API void nk_diligent_render(struct nk_diligent_context* nk_dlg_ctx,
                               Diligent::IDeviceContext*   device_ctx,
                               enum nk_anti_aliasing       AA);

NK_API void nk_diligent_resize(struct nk_diligent_context* nk_dlg_ctx,
                               Diligent::IDeviceContext*   device_ctx,
                               unsigned int                width,
                               unsigned int                height);

NK_API void nk_diligent_shutdown(struct nk_diligent_context* nk_dlg_ctx);
