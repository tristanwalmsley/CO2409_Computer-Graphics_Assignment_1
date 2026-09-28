#include "GraphicsDevice.h"

bool GraphicsDevice::Init(HWND hwnd, int width, int height)
{
    mWidth  = width;
    mHeight = height;

    DXGI_SWAP_CHAIN_DESC swapDesc = {};
    swapDesc.OutputWindow = hwnd;
    swapDesc.Windowed     = TRUE;
    swapDesc.BufferCount  = 1;
    swapDesc.BufferDesc.Width  = width;
    swapDesc.BufferDesc.Height = height;
    swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.BufferDesc.RefreshRate.Numerator   = 60;
    swapDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.SampleDesc.Count   = 1;
    swapDesc.SampleDesc.Quality = 0;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, 0, 0, 0,
                                               D3D11_SDK_VERSION, &swapDesc, &mSwapChain,
                                               &mDevice, nullptr, &mContext);
    if (FAILED(hr)) { lastError = "Error creating Direct3D device"; return false; }

    ID3D11Texture2D* backBuffer;
    hr = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBuffer);
    if (FAILED(hr)) { lastError = "Error getting back buffer"; return false; }

    hr = mDevice->CreateRenderTargetView(backBuffer, NULL, &mBackBufferRenderTarget);
    backBuffer->Release();
    if (FAILED(hr)) { lastError = "Error creating render target view"; return false; }

    D3D11_TEXTURE2D_DESC dbDesc = {};
    dbDesc.Width  = width;
    dbDesc.Height = height;
    dbDesc.MipLevels = 1;
    dbDesc.ArraySize = 1;
    dbDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dbDesc.SampleDesc.Count = 1;
    dbDesc.Usage = D3D11_USAGE_DEFAULT;
    dbDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    hr = mDevice->CreateTexture2D(&dbDesc, nullptr, &mDepthStencilTexture);
    if (FAILED(hr)) { lastError = "Error creating depth buffer"; return false; }

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = dbDesc.Format;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    hr = mDevice->CreateDepthStencilView(mDepthStencilTexture, &dsvDesc, &mDepthStencil);
    if (FAILED(hr)) { lastError = "Error creating depth stencil view"; return false; }

    return true;
}

void GraphicsDevice::Shutdown()
{
    if (mContext)               { mContext->ClearState(); mContext->Release();               mContext               = nullptr; }
    if (mDepthStencil)          { mDepthStencil->Release();                                  mDepthStencil          = nullptr; }
    if (mDepthStencilTexture)   { mDepthStencilTexture->Release();                           mDepthStencilTexture   = nullptr; }
    if (mBackBufferRenderTarget){ mBackBufferRenderTarget->Release();                        mBackBufferRenderTarget = nullptr; }
    if (mSwapChain)             { mSwapChain->Release();                                     mSwapChain             = nullptr; }
    if (mDevice)                { mDevice->Release();                                        mDevice                = nullptr; }
}