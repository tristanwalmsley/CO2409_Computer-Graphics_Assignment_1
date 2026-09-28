#pragma once
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <string>

class GraphicsDevice
{
public:
    static GraphicsDevice& Get()
    {
        static GraphicsDevice instance;
        return instance;
    }

    bool Init(HWND hwnd, int width, int height);
    void Shutdown();

    ID3D11Device*           GetDevice()            const { return mDevice; }
    ID3D11DeviceContext*    GetContext()           const { return mContext; }
    IDXGISwapChain*         GetSwapChain()         const { return mSwapChain; }
    ID3D11RenderTargetView* GetBackBuffer()        const { return mBackBufferRenderTarget; }
    ID3D11DepthStencilView* GetDepthStencil()      const { return mDepthStencil; }
    int                     GetWidth()             const { return mWidth; }
    int                     GetHeight()            const { return mHeight; }

    // Keep last error accessible
    std::string lastError;

private:
    GraphicsDevice() = default;
    ~GraphicsDevice() { Shutdown(); }
    GraphicsDevice(const GraphicsDevice&) = delete;
    GraphicsDevice& operator=(const GraphicsDevice&) = delete;

    ID3D11Device*            mDevice                = nullptr;
    ID3D11DeviceContext*     mContext               = nullptr;
    IDXGISwapChain*          mSwapChain             = nullptr;
    ID3D11RenderTargetView*  mBackBufferRenderTarget = nullptr;
    ID3D11Texture2D*         mDepthStencilTexture   = nullptr;
    ID3D11DepthStencilView*  mDepthStencil          = nullptr;
    int mWidth  = 960;
    int mHeight = 1280;
};