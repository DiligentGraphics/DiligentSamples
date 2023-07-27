/*
 *  Copyright 2019-2023 Diligent Graphics LLC
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

using Diligent;
using System;
using System.CommandLine;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.Windows.Forms;
using System.Numerics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using IDeviceContext = Diligent.IDeviceContext;

namespace Tutorial03_Texturing;

public enum GraphicsApi
{
    D3D11,
    D3D12,
    VK,
    GL
}

static class Program
{
    struct Vertex
    {
        public Vector3 Position;
        public Vector2 Texcoord;

        public Vertex(Vector3 position, Vector2 texcoord)
        {
            Position = position;
            Texcoord = texcoord;
        }
    }

    static Matrix4x4 CreatePerspectiveFieldOfView(float fieldOfView, float aspectRatio, float nearPlaneDistance, float farPlaneDistance, bool isOpenGL)
    {
        if (fieldOfView <= 0.0f || fieldOfView >= MathF.PI)
            throw new ArgumentOutOfRangeException(nameof(fieldOfView));

        if (nearPlaneDistance <= 0.0f)
            throw new ArgumentOutOfRangeException(nameof(nearPlaneDistance));

        if (farPlaneDistance <= 0.0f)
            throw new ArgumentOutOfRangeException(nameof(farPlaneDistance));

        if (nearPlaneDistance >= farPlaneDistance)
            throw new ArgumentOutOfRangeException(nameof(nearPlaneDistance));

        float yScale = 1.0f / MathF.Tan(fieldOfView * 0.5f);
        float xScale = yScale / aspectRatio;

        Matrix4x4 result = new()
        {
            M11 = xScale,
            M22 = yScale
        };

        if (isOpenGL)
        {
            result.M33 = (farPlaneDistance + nearPlaneDistance) / (farPlaneDistance - nearPlaneDistance);
            result.M43 = -2 * nearPlaneDistance * farPlaneDistance / (farPlaneDistance - nearPlaneDistance);
            result.M34 = 1.0f;
        }
        else
        {
            result.M33 = farPlaneDistance / (farPlaneDistance - nearPlaneDistance);
            result.M43 = -nearPlaneDistance * farPlaneDistance / (farPlaneDistance - nearPlaneDistance);
            result.M34 = 1.0f;
        }

        return result;
    }

    static unsafe ITextureView LoadTexture(IRenderDevice renderDevice, IDeviceContext deviceContext, string fileName)
    {
        var bitmap = new Bitmap(fileName);
        var bitmapData = bitmap.LockBits(new Rectangle(0, 0, bitmap.Width, bitmap.Height),
            ImageLockMode.ReadOnly,
            PixelFormat.Format32bppArgb);

        var pixelData = new byte[bitmapData.Stride * bitmapData.Height];
        Marshal.Copy(bitmapData.Scan0, pixelData, 0, bitmapData.Stride * bitmapData.Height);
        bitmap.UnlockBits(bitmapData);

        for (var pixelIdx = 0; pixelIdx < bitmap.Width * bitmap.Height; pixelIdx++)
            (pixelData[4 * pixelIdx + 0], pixelData[4 * pixelIdx + 2]) = (pixelData[4 * pixelIdx + 2], pixelData[4 * pixelIdx + 0]);

        fixed (byte* pixelDataPtr = pixelData)
        {
            var texture = renderDevice.CreateTexture(new TextureDesc()
            {
                Type = ResourceDimension.Tex2d,
                Width = (uint)bitmap.Width,
                Height = (uint)bitmap.Height,
                Format = TextureFormat.RGBA8_UNorm_sRGB,
                BindFlags = BindFlags.ShaderResource
            }, new TextureData()
            {
                Context = deviceContext,
                SubResources = new TextureSubResData[]
                {
                    new()
                    {
                        Data = (IntPtr)pixelDataPtr,
                        Stride = (ulong)bitmapData.Stride
                    }
                }
            });
            return texture.GetDefaultView(TextureViewType.ShaderResource);
        }
    }

    static GraphicsApi? ParseCommandLine(string[] args)
    {
        var apiOption = new Option<GraphicsApi>(
            name: "--mode",
            description: "Graphics API for running.");

        var rootCommand = new RootCommand("Tutorial03_Texturing");
        rootCommand.AddOption(apiOption);

        var defaultGraphicsAPI = GraphicsApi.D3D11;
        rootCommand.SetHandler((e) =>
        {
            defaultGraphicsAPI = e;
        }, apiOption);
        rootCommand.Invoke(args);
        return defaultGraphicsAPI;
    }

    static void SetMessageCallback(IEngineFactory engineFactory)
    {
        engineFactory.SetMessageCallback((severity, message, function, file, line) =>
        {
            switch (severity)
            {
                case DebugMessageSeverity.Warning:
                case DebugMessageSeverity.Error:
                case DebugMessageSeverity.FatalError:
                    Console.WriteLine($"Diligent Engine: {severity} in {function}() ({file}, {line}): {message}");
                    break;
                case DebugMessageSeverity.Info:
                    Console.WriteLine($"Diligent Engine: {severity} {message}");
                    break;
                default:
                    throw new ArgumentOutOfRangeException(nameof(severity), severity, null);
            }
        });
    }

    static void CreateRenderDeviceAndSwapChain(IEngineFactory engineFactory, out IRenderDevice renderDevice, out IDeviceContext deviceContext, out ISwapChain swapChain, Form window)
    {
        IDeviceContext[] contextsOut;
        switch (engineFactory)
        {
            case IEngineFactoryD3D11 engineFactoryD3D11:
                engineFactoryD3D11.CreateDeviceAndContextsD3D11(new()
                {
                    EnableValidation = true
                }, out renderDevice, out contextsOut);
                swapChain = engineFactoryD3D11.CreateSwapChainD3D11(renderDevice, contextsOut[0], new(), new(), new() { Wnd = window.Handle });
                window.Text += " (Direct3D11)";
                break;
            case IEngineFactoryD3D12 engineFactoryD3D12:
                engineFactoryD3D12.CreateDeviceAndContextsD3D12(new()
                {
                    EnableValidation = true
                }, out renderDevice, out contextsOut);
                swapChain = engineFactoryD3D12.CreateSwapChainD3D12(renderDevice, contextsOut[0], new(), new(), new() { Wnd = window.Handle });
                window.Text += " (Direct3D12)";
                break;
            case IEngineFactoryVk engineFactoryVk:
                engineFactoryVk.CreateDeviceAndContextsVk(new()
                {
                    EnableValidation = false
                }, out renderDevice, out contextsOut);
                swapChain = engineFactoryVk.CreateSwapChainVk(renderDevice, contextsOut[0], new(), new() { Wnd = window.Handle });
                window.Text += " (Vulkan)";
                break;
            case IEngineFactoryOpenGL engineFactoryOpenGL:
                engineFactoryOpenGL.CreateDeviceAndSwapChainGL(new()
                {
                    EnableValidation = true,
                    Window = new() { Wnd = window.Handle }
                }, out renderDevice, out var glContext, new(), out swapChain);
                contextsOut = glContext != null ? new[] { glContext } : null;
                window.Text += " (OpenGL)";
                break;
            default:
                throw new Exception("Unexpected exception");
        }

        deviceContext = contextsOut[0];
    }

    static void Main(string[] args)
    {
        using var window = new Form()
        {
            Text = "Tutorial03: Texturing",
            Name = "DiligentWindow",
            FormBorderStyle = FormBorderStyle.Sizable,
            ClientSize = new Size(1024, 720),
            StartPosition = FormStartPosition.CenterScreen,
            MinimumSize = new Size(200, 200)
        };

        using IEngineFactory engineFactory = ParseCommandLine(args) switch
        {
            GraphicsApi.D3D11 => Native.CreateEngineFactory<IEngineFactoryD3D11>(),
            GraphicsApi.D3D12 => Native.CreateEngineFactory<IEngineFactoryD3D12>(),
            GraphicsApi.VK => Native.CreateEngineFactory<IEngineFactoryVk>(),
            GraphicsApi.GL => Native.CreateEngineFactory<IEngineFactoryOpenGL>(),
            _ => throw new ArgumentOutOfRangeException($"Not expected graphics API")
        };

        SetMessageCallback(engineFactory);
        CreateRenderDeviceAndSwapChain(engineFactory, out var renderDeviceOut, out var deviceContextOut, out var swapChainOut, window);
        using var renderDevice = renderDeviceOut;
        using var deviceContext = deviceContextOut;
        using var swapChain = swapChainOut;

        var cubeVertices = new Vertex[] {
            new (new(-1,-1,-1), new(0,1)),
            new (new(-1,+1,-1), new(0,0)),
            new (new(+1,+1,-1), new(1,0)),
            new (new(+1,-1,-1), new(1,1)),

            new (new(-1,-1,-1), new(0,1)),
            new (new(-1,-1,+1), new(0,0)),
            new (new(+1,-1,+1), new(1,0)),
            new (new(+1,-1,-1), new(1,1)),

            new (new(+1,-1,-1), new(0,1)),
            new (new(+1,-1,+1), new(1,1)),
            new (new(+1,+1,+1), new(1,0)),
            new (new(+1,+1,-1), new(0,0)),

            new (new(+1,+1,-1), new(0,1)),
            new (new(+1,+1,+1), new(0,0)),
            new (new(-1,+1,+1), new(1,0)),
            new (new(-1,+1,-1), new(1,1)),

            new (new(-1,+1,-1), new(1,0)),
            new (new(-1,+1,+1), new(0,0)),
            new (new(-1,-1,+1), new(0,1)),
            new (new(-1,-1,-1), new(1,1)),

            new (new(-1,-1,+1), new(1,1)),
            new (new(+1,-1,+1), new(0,1)),
            new (new(+1,+1,+1), new(0,0)),
            new (new(-1,+1,+1), new(1,0))
        };

        var cubeIndices = new uint[]
        {
            2,0,1,    2,3,0,
            4,6,5,    4,7,6,
            8,10,9,   8,11,10,
            12,14,13, 12,15,14,
            16,18,17, 16,19,18,
            20,21,22, 20,22,23
        };

        using var vertexBuffer = renderDevice.CreateBuffer(new()
        {
            Name = "Cube vertex buffer",
            Usage = Usage.Immutable,
            BindFlags = BindFlags.VertexBuffer,
            Size = (ulong)(Unsafe.SizeOf<Vertex>() * cubeVertices.Length)
        }, cubeVertices);

        using var indexBuffer = renderDevice.CreateBuffer(new()
        {
            Name = "Cube index buffer",
            Usage = Usage.Immutable,
            BindFlags = BindFlags.IndexBuffer,
            Size = (ulong)(Unsafe.SizeOf<uint>() * cubeIndices.Length)
        }, cubeIndices);

        using var uniformBuffer = renderDevice.CreateBuffer(new()
        {
            Name = "Uniform buffer",
            Size = (ulong)Unsafe.SizeOf<Matrix4x4>(),
            Usage = Usage.Dynamic,
            BindFlags = BindFlags.UniformBuffer,
            CPUAccessFlags = CpuAccessFlags.Write
        });

        using var textureSrv = LoadTexture(renderDevice, deviceContext, "assets/DGLogo.png");

        using var shaderSourceFactory = engineFactory.CreateDefaultShaderSourceStreamFactory("assets");

        using var vs = renderDevice.CreateShader(new()
        {
            FilePath = "cube.vsh",
            ShaderSourceStreamFactory = shaderSourceFactory,
            Desc = new()
            {
                Name = "Cube VS",
                ShaderType = ShaderType.Vertex,
                UseCombinedTextureSamplers = true
            },
            SourceLanguage = ShaderSourceLanguage.Hlsl
        }, out _);

        using var ps = renderDevice.CreateShader(new()
        {
            FilePath = "cube.psh",
            ShaderSourceStreamFactory = shaderSourceFactory,
            Desc = new()
            {
                Name = "Cube PS",
                ShaderType = ShaderType.Pixel,
                UseCombinedTextureSamplers = true
            },
            SourceLanguage = ShaderSourceLanguage.Hlsl
        }, out _);

        using var pipeline = renderDevice.CreateGraphicsPipelineState(new()
        {
            PSODesc = new()
            {
                Name = "Cube PSO",
                ResourceLayout = new()
                {
                    DefaultVariableType = ShaderResourceVariableType.Static,
                    Variables = new ShaderResourceVariableDesc[] {
                        new ()
                        {
                            ShaderStages = ShaderType.Pixel,
                            Name = "g_Texture",
                            Type = ShaderResourceVariableType.Mutable
                        }
                    },
                    ImmutableSamplers = new ImmutableSamplerDesc[] {
                        new ()
                        {
                            Desc = new()
                            {
                                MinFilter = FilterType.Linear, MagFilter = FilterType.Linear, MipFilter = FilterType.Linear,
                                AddressU = TextureAddressMode.Clamp, AddressV = TextureAddressMode.Clamp, AddressW = TextureAddressMode.Clamp
                            },
                            SamplerOrTextureName = "g_Texture",
                            ShaderStages = ShaderType.Pixel
                        }
                    }
                }
            },
            Vs = vs,
            Ps = ps,
            GraphicsPipeline = new()
            {
                InputLayout = new()
                {
                    LayoutElements = new[] {
                        new LayoutElement
                        {
                            InputIndex = 0,
                            NumComponents = 3,
                            ValueType = Diligent.ValueType.Float32,
                            IsNormalized = false,
                        },
                        new LayoutElement
                        {
                            InputIndex = 1,
                            NumComponents = 2,
                            ValueType = Diligent.ValueType.Float32,
                            IsNormalized = false,
                        }
                    }
                },
                PrimitiveTopology = PrimitiveTopology.TriangleList,
                RasterizerDesc = new() { CullMode = CullMode.Back },
                DepthStencilDesc = new() { DepthEnable = true },
                NumRenderTargets = 1,
                RTVFormats = new[] { swapChain.GetDesc().ColorBufferFormat },
                DSVFormat = swapChain.GetDesc().DepthBufferFormat
            }
        });
        pipeline.GetStaticVariableByName(ShaderType.Vertex, "Constants").Set(uniformBuffer, SetShaderResourceFlags.None);

        using var shaderResourceBinding = pipeline.CreateShaderResourceBinding(true);
        shaderResourceBinding.GetVariableByName(ShaderType.Pixel, "g_Texture").Set(textureSrv, SetShaderResourceFlags.None);

        var clock = new Stopwatch();
        clock.Start();

        window.Paint += (sender, e) =>
        {
            var rtv = swapChain.GetCurrentBackBufferRTV();
            var dsv = swapChain.GetDepthBufferDSV();

            var isOpenGL = renderDevice.GetDeviceInfo().Type == RenderDeviceType.Gl || renderDevice.GetDeviceInfo().Type == RenderDeviceType.Gles;
            var worldMatrix = Matrix4x4.CreateRotationY(clock.ElapsedMilliseconds / 1000.0f) * Matrix4x4.CreateRotationX(-MathF.PI * 0.1f);
            var viewMatrix = Matrix4x4.CreateTranslation(0.0f, 0.0f, 5.0f);
            var projMatrix = CreatePerspectiveFieldOfView(MathF.PI / 4.0f, swapChain.GetDesc().Width / (float)swapChain.GetDesc().Height, 0.01f, 100.0f, isOpenGL);
            var wvpMatrix = Matrix4x4.Transpose(worldMatrix * viewMatrix * projMatrix);

            var mapUniformBuffer = deviceContext.MapBuffer<Matrix4x4>(uniformBuffer, MapType.Write, MapFlags.Discard);
            mapUniformBuffer[0] = wvpMatrix;
            deviceContext.UnmapBuffer(uniformBuffer, MapType.Write);

            deviceContext.SetRenderTargets(new[] { rtv }, dsv, ResourceStateTransitionMode.Transition);
            deviceContext.ClearRenderTarget(rtv, new(0.35f, 0.35f, 0.35f, 1.0f), ResourceStateTransitionMode.Transition);
            deviceContext.ClearDepthStencil(dsv, ClearDepthStencilFlags.Depth, 1.0f, 0, ResourceStateTransitionMode.Transition);
            deviceContext.SetPipelineState(pipeline);
            deviceContext.SetVertexBuffers(0, new[] { vertexBuffer }, new[] { 0ul }, ResourceStateTransitionMode.Transition);
            deviceContext.SetIndexBuffer(indexBuffer, 0, ResourceStateTransitionMode.Transition);
            deviceContext.CommitShaderResources(shaderResourceBinding, ResourceStateTransitionMode.Transition);
            deviceContext.DrawIndexed(new()
            {
                IndexType = Diligent.ValueType.UInt32,
                NumIndices = 36,
                Flags = DrawFlags.VerifyAll
            });
            swapChain.Present(0);
            window.Invalidate(new Rectangle(0, 0, 1, 1)); //HACK for Vulkan
        };

        window.Resize += (sender, e) =>
        {
            var control = (Control)sender!;
            swapChain.Resize((uint)control.Width, (uint)control.Height, SurfaceTransform.Optimal);
        };
        Application.Run(window);
    }
}
