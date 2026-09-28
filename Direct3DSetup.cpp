#include "Direct3DSetup.h"
#include "GraphicsDevice.h"
#include "Common.h"

extern HWND gHWnd;

bool InitDirect3D()
{
    return GraphicsDevice::Get().Init(gHWnd, viewportWidth, viewportHeight);
}

void ShutdownDirect3D()
{
    GraphicsDevice::Get().Shutdown();
}