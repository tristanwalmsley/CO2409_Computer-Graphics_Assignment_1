//--------------------------------------------------------------------------------------
// Commonly used definitions across entire project
//--------------------------------------------------------------------------------------
#ifndef _COMMON_H_INCLUDED_
#define _COMMON_H_INCLUDED_

#define NOMINMAX // windows.h is very old and for compatibilty reasons includes a very badly written
                 // definition of min and max. It clashes with the STL std::min and std::max.
                 // Use this line before windows.h to remove the problematic legacy definitions
#include <windows.h>
#include <d3d11.h>
#include <string>

#include "CVector3.h"
#include "CMatrix4x4.h"
#include "GraphicsDevice.h"

#define gBackBufferRenderTarget  GraphicsDevice::Get().GetBackBuffer()
#define gDepthStencil            GraphicsDevice::Get().GetDepthStencil()
#define gSwapChain               GraphicsDevice::Get().GetSwapChain()
#define gViewportWidth           GraphicsDevice::Get().GetWidth()
#define gViewportHeight          GraphicsDevice::Get().GetHeight()


//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
// Make global Variables from various files available to other files. "extern" means
// this variable is defined in another file somewhere. We should use classes and avoid
// use of globals, but done this way to keep code simpler so the DirectX content is
// clearer. However, try to architect your own code in a better way.

// Windows variables
extern HWND gHWnd;

#define gD3DDevice  GraphicsDevice::Get().GetDevice()
#define gD3DContext GraphicsDevice::Get().GetContext()

// Input constsnts
extern const float ROTATION_SPEED;
extern const float MOVEMENT_SPEED;


// A global error message to help track down fatal errors - set it to a useful message
// when a serious error occurs
extern std::string gLastError;



//--------------------------------------------------------------------------------------
// Constant Buffers
//--------------------------------------------------------------------------------------
// Variables sent over to the GPU each frame

// Data that remains constant for an entire frame, updated from C++ to the GPU shaders *once per frame*
// We hold them together in a structure and send the whole thing to a "constant buffer" on the GPU each frame when
// we have finished updating the scene. There is a structure in the shader code that exactly matches this one
struct PerFrameConstants
{
    // These are the matrices used to position the camera
    CMatrix4x4 viewMatrix;
    CMatrix4x4 projectionMatrix;
    CMatrix4x4 viewProjectionMatrix; // The above two matrices multiplied together to combine their effects

    // Light 1 - point light (flashing)
    CVector3   gLight1Position; // 3 floats: x, y z
    float      padding1; // Pad above variable to float4 (HLSL requirement - which we must duplicate in this the C++ version of the structure)
    CVector3   gLight1Colour;
    float      padding2;

    // Light 2 - point light (colour-cycling)
    CVector3   gLight2Position;
    float      padding3;
    CVector3   gLight2Colour;
    float      padding4;

    // Light 3 - directional light
    CVector3   gLight3Direction;
    float      padding5;
    CVector3   gLight3Colour;
    float      padding6;

    // Light 4 - spotlight
    CVector3   gLight4Position;
    float      padding7;
    CVector3   gLight4Colour;

    float      parallaxDepth;

    CVector3   gLight4Facing;       // Spotlight facing direction (normal)
    float      gLight4CosHalfAngle; // cos(Spot light cone angle / 2). Precalculate in C++ the spotlight angle in this form to save doing in the shader
    CMatrix4x4 gLight4ViewMatrix;    // For shadow mapping we treat lights like cameras so we need camera matrices for them (prepared on the C++ side)
    CMatrix4x4 gLight4ProjectionMatrix;

    CVector3   gAmbientColour;
    float      gSpecularPower;

    CVector3   gCameraPosition;
    float      gFrameTime;
};

extern PerFrameConstants gPerFrameConstants;      // This variable holds the CPU-side constant buffer described above
extern ID3D11Buffer*     gPerFrameConstantBuffer; // This variable controls the GPU-side constant buffer matching to the above structure



// This is the matrix that positions the next thing to be rendered in the scene. Unlike the structure above this data can be
// updated and sent to the GPU several times every frame (once per model). However, apart from that it works in the same way.
struct PerModelConstants
{
    CMatrix4x4 worldMatrix;
    CVector3   objectColour; // Allows each light model to be tinted to match the light colour they cast
    float      padding6;
};
extern PerModelConstants gPerModelConstants;      // This variable holds the CPU-side constant buffer described above
extern ID3D11Buffer*     gPerModelConstantBuffer; // This variable controls the GPU-side constant buffer related to the above structure


#endif //_COMMON_H_INCLUDED_
