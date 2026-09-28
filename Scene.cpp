//--------------------------------------------------------------------------------------
// Scene geometry and layout preparation
// Scene rendering & update
//--------------------------------------------------------------------------------------

#include "Scene.h"
#include "Mesh.h"
#include "Model.h"
#include "Camera.h"
#include "State.h"
#include "Shader.h"
#include "Input.h"
#include "Common.h"
#include "Light.h"
#include "SceneManager.h"
#include "GraphicsDevice.h"

#include "CVector2.h"
#include "CVector3.h"
#include "CMatrix4x4.h"
#include "MathHelpers.h"
#include "GraphicsHelpers.h"
#include "ColourRGBA.h"

#include <sstream>
#include <memory>
#include <cmath>


//--------------------------------------------------------------------------------------
// Scene Data
//--------------------------------------------------------------------------------------

// Convenience pointers into SceneManager - these are file-scope only (static)
static Model*  gCharacter    = nullptr;
static Model*  gCrate        = nullptr;
static Model*  gFloor        = nullptr;
static Model*  gSphere       = nullptr;
static Model*  gTeapot       = nullptr;
static Model*  gCube         = nullptr;
static Model*  gPortal       = nullptr;
static Model*  gSphere2      = nullptr;
static Camera* gCamera       = nullptr;
static Camera* gPortalCamera = nullptr;
static Camera  gCubeMapCameras[6];

// Texture pointers - file-scope only
static Texture* gCharacterTex    = nullptr;
static Texture* gCrateTex        = nullptr;
static Texture* gFloorTex        = nullptr;
static Texture* gFloorNormalTex  = nullptr;
static Texture* gSphereTex       = nullptr;
static Texture* gSphereNormalTex = nullptr;
static Texture* gTeapotTex       = nullptr;
static Texture* gTeapotNormalTex = nullptr;
static Texture* gCubeTex         = nullptr;
static Texture* gCubeNormalTex   = nullptr;
static Texture* gLightTex        = nullptr;

// Scene settings - file-scope only
static CVector3   gAmbientColour    = { 0.2f, 0.2f, 0.3f };
static float      gSpecularPower    = 256.0f;
static ColourRGBA gBackgroundColor  = { 0.2f, 0.2f, 0.3f, 1.0f };
static float      gSpotlightConeAngle = 90.0f;
static CVector3   gLight3Direction  = { 0.0f, -1.0f, 0.5f };
static float      gParallaxDepth    = 0.1f;
static float      gFrameTimeGlobal  = 0.0f;
static bool       lockFPS           = true;

const float ROTATION_SPEED = 2.0f;  // 2 radians per second for rotation
const float MOVEMENT_SPEED = 50.0f; // 50 units per second for movement


// Light behaviour state
static float flashTimer = 0.0f;
static bool  flashOn    = true;

// Cube map size constant
static const int gCubeMapSize = 256;


//--------------------------------------------------------------------------------------
// Constant Buffers
// These are extern in Common.h because Model.cpp and GraphicsHelpers.cpp need them
//--------------------------------------------------------------------------------------

PerFrameConstants gPerFrameConstants;
ID3D11Buffer*     gPerFrameConstantBuffer = nullptr;

PerModelConstants gPerModelConstants;
ID3D11Buffer*     gPerModelConstantBuffer = nullptr;


//--------------------------------------------------------------------------------------
// Shadow Map Resources - file-scope only
//--------------------------------------------------------------------------------------

static int                        gShadowMapSize       = 4096;
static ID3D11Texture2D*           gShadowMap1Texture      = nullptr;
static ID3D11DepthStencilView*    gShadowMap1DepthStencil = nullptr;
static ID3D11ShaderResourceView*  gShadowMap1SRV          = nullptr;
static ID3D11Texture2D*           gShadowMap2Texture      = nullptr;
static ID3D11DepthStencilView*    gShadowMap2DepthStencil = nullptr;
static ID3D11ShaderResourceView*  gShadowMap2SRV          = nullptr;


//--------------------------------------------------------------------------------------
// Portal Resources - file-scope only
//--------------------------------------------------------------------------------------

static int                        gPortalWidth            = 256;
static int                        gPortalHeight           = 256;
static ID3D11Texture2D*           gPortalTexture          = nullptr;
static ID3D11RenderTargetView*    gPortalRenderTarget      = nullptr;
static ID3D11ShaderResourceView*  gPortalTextureSRV        = nullptr;
static ID3D11Texture2D*           gPortalDepthStencil      = nullptr;
static ID3D11DepthStencilView*    gPortalDepthStencilView  = nullptr;


//--------------------------------------------------------------------------------------
// Cube Map Resources - file-scope only
//--------------------------------------------------------------------------------------

static ID3D11RenderTargetView*    mDynamicCubeMapRTV[6]   = {};
static ID3D11ShaderResourceView*  mDynamicCubeMapSRV       = nullptr;
static ID3D11DepthStencilView*    mDynamicCubeMapDSV[6]   = {};


//--------------------------------------------------------------------------------------
// Light Helper Functions
//--------------------------------------------------------------------------------------

// Returns a view matrix treating the given spotlight as a camera
static CMatrix4x4 CalculateLightViewMatrix(int lightIndex)
{
    std::string id = "light" + std::to_string(lightIndex);
    return InverseAffine(SceneManager::Get().GetLight(id)->GetModel()->WorldMatrix());
}

// Returns a projection matrix for the spotlight cone
static CMatrix4x4 CalculateLightProjectionMatrix(int lightIndex)
{
    return MakeProjectionMatrix(1.0f, ToRadians(gSpotlightConeAngle));
}


//--------------------------------------------------------------------------------------
// Cube Map Setup
//--------------------------------------------------------------------------------------

// Builds a view matrix looking from eye toward target with given up vector
static CMatrix4x4 MakeLookAtMatrix(const CVector3& eye, const CVector3& target, const CVector3& up)
{
    CVector3 zAxis = Normalise(target - eye);
    CVector3 xAxis = Normalise(Cross(up, zAxis));
    CVector3 yAxis = Cross(zAxis, xAxis);

    CMatrix4x4 matrix;
    matrix.SetRow(0, xAxis);
    matrix.SetRow(1, yAxis);
    matrix.SetRow(2, -zAxis);
    matrix.SetRow(3, { -Dot(xAxis, eye), -Dot(yAxis, eye), Dot(zAxis, eye) });

    return matrix;
}

// Sets up the 6 cube map cameras pointing along each axis from the given position
static void BuildCubeFaceCamera(float x, float y, float z)
{
    CVector3 center = { x, y, z };

    CVector3 targets[6] =
    {
        { x + 1.0f, y,        z        }, // +X
        { x - 1.0f, y,        z        }, // -X
        { x,        y + 1.0f, z        }, // +Y
        { x,        y - 1.0f, z        }, // -Y
        { x,        y,        z + 1.0f }, // +Z
        { x,        y,        z - 1.0f }, // -Z
    };

    CVector3 ups[6] =
    {
        {  0.0f,  1.0f,  0.0f }, // +X
        {  0.0f,  1.0f,  0.0f }, // -X
        {  0.0f,  0.0f, -1.0f }, // +Y - looking up, so use -Z as up
        {  0.0f,  0.0f,  1.0f }, // -Y - looking down, so use +Z as up
        {  0.0f,  1.0f,  0.0f }, // +Z
        {  0.0f,  1.0f,  0.0f }, // -Z
    };

    for (int i = 0; i < 6; ++i)
    {
        gCubeMapCameras[i].SetFOV(0.5f * PI);
        gCubeMapCameras[i].SetNearClip(0.1f);
        gCubeMapCameras[i].SetFarClip(1000.0f);
        gCubeMapCameras[i].SetPosition(center);

        CVector3 dir = Normalise(targets[i] - center);
        float pitch = asin(-dir.y);
        float yaw   = atan2(-dir.x, -dir.z);
        gCubeMapCameras[i].SetRotation({ pitch, yaw, 0.0f });
    }
}

// Creates the GPU textures, RTVs, SRVs and DSVs for the dynamic cube map
static void BuildDynamicCubeMapViews()
{
    // Colour texture array (6 faces, full mip chain)
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width            = gCubeMapSize;
    texDesc.Height           = gCubeMapSize;
    texDesc.MipLevels        = 0; // 0 = full mip chain generated automatically
    texDesc.ArraySize        = 6;
    texDesc.SampleDesc.Count = 1;
    texDesc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.Usage            = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texDesc.MiscFlags        = D3D11_RESOURCE_MISC_TEXTURECUBE | D3D11_RESOURCE_MISC_GENERATE_MIPS;

    ID3D11Texture2D* cubeTex = nullptr;
    gD3DDevice->CreateTexture2D(&texDesc, nullptr, &cubeTex);

    // One RTV per face
    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format                         = texDesc.Format;
    rtvDesc.ViewDimension                  = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
    rtvDesc.Texture2DArray.MipSlice        = 0;
    rtvDesc.Texture2DArray.ArraySize       = 1;
    for (int i = 0; i < 6; ++i)
    {
        rtvDesc.Texture2DArray.FirstArraySlice = i;
        gD3DDevice->CreateRenderTargetView(cubeTex, &rtvDesc, &mDynamicCubeMapRTV[i]);
    }

    // Single SRV covering all 6 faces
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format                    = texDesc.Format;
    srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels     = -1; // All mip levels
    gD3DDevice->CreateShaderResourceView(cubeTex, &srvDesc, &mDynamicCubeMapSRV);
    cubeTex->Release(); // SRV holds the reference

    // Depth texture array (6 faces, 1 mip)
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width              = gCubeMapSize;
    depthDesc.Height             = gCubeMapSize;
    depthDesc.MipLevels          = 1;
    depthDesc.ArraySize          = 6;
    depthDesc.SampleDesc.Count   = 1;
    depthDesc.Format             = DXGI_FORMAT_D32_FLOAT;
    depthDesc.Usage              = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags          = D3D11_BIND_DEPTH_STENCIL;
    depthDesc.MiscFlags          = D3D11_RESOURCE_MISC_TEXTURECUBE;

    ID3D11Texture2D* depthTex = nullptr;
    HRESULT hr = gD3DDevice->CreateTexture2D(&depthDesc, nullptr, &depthTex);
    if (FAILED(hr) || !depthTex)
    {
        OutputDebugStringA("Failed to create cube map depth texture\n");
        return;
    }

    // One DSV per face
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format                         = depthDesc.Format;
    dsvDesc.Flags                          = 0;
    dsvDesc.ViewDimension                  = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
    dsvDesc.Texture2DArray.ArraySize       = 1;
    for (int i = 0; i < 6; ++i)
    {
        dsvDesc.Texture2DArray.FirstArraySlice = i;
        gD3DDevice->CreateDepthStencilView(depthTex, &dsvDesc, &mDynamicCubeMapDSV[i]);
    }
    depthTex->Release(); // DSVs hold the reference
}


//--------------------------------------------------------------------------------------
// Initialise Scene Geometry, Constant Buffers and States
//--------------------------------------------------------------------------------------

bool InitGeometry()
{
    auto& scene = SceneManager::Get();

    // Load meshes
    try
    {
        scene.LoadMesh("character", "Troll.x");
        scene.LoadMesh("floor",     "Floor.x",          true);
        scene.LoadMesh("sphere",    "Sphere.x",         true);
        scene.LoadMesh("teapot",    "Teapot.x",         true);
        scene.LoadMesh("crate",     "CargoContainer.x");
        scene.LoadMesh("light",     "Light.x");
        scene.LoadMesh("portal",    "Portal.x");
        scene.LoadMesh("cube",      "Cube.x",           true);
    }
    catch (std::runtime_error e)
    {
        gLastError = e.what();
        return false;
    }

    // Load shaders
    if (!LoadShaders())
    {
        gLastError = "Error loading shaders";
        return false;
    }

    // Create constant buffers
    gPerFrameConstantBuffer = CreateConstantBuffer(sizeof(gPerFrameConstants));
    gPerModelConstantBuffer = CreateConstantBuffer(sizeof(gPerModelConstants));
    if (!gPerFrameConstantBuffer || !gPerModelConstantBuffer)
    {
        gLastError = "Error creating constant buffers";
        return false;
    }

    // Load textures via SceneManager (handles lifetime and release automatically)
    gCharacterTex    = scene.LoadTexture("characterTex",    "TrollDiffuseSpecular.dds");
    gCrateTex        = scene.LoadTexture("crateTex",        "CargoA.dds");
    gFloorTex        = scene.LoadTexture("floorTex",        "MetalDiffuseSpecular.dds");
    gFloorNormalTex  = scene.LoadTexture("floorNormalTex",  "MetalNormal.dds");
    gSphereTex       = scene.LoadTexture("sphereTex",       "PatternDiffuseSpecular.dds");
    gSphereNormalTex = scene.LoadTexture("sphereNormalTex", "PatternNormal.dds");
    gTeapotTex       = scene.LoadTexture("teapotTex",       "WoodDiffuseSpecular.dds");
    gTeapotNormalTex = scene.LoadTexture("teapotNormalTex", "WoodNormal.dds");
    gCubeTex         = scene.LoadTexture("cubeTex",         "CobbleDiffuseSpecular.dds");
    gCubeNormalTex   = scene.LoadTexture("cubeNormalTex",   "CobbleNormalHeight.dds");
    gLightTex        = scene.LoadTexture("lightTex",        "Flare.jpg");

    if (!gCharacterTex || !gCrateTex    || !gFloorTex    || !gFloorNormalTex  ||
        !gSphereTex    || !gSphereNormalTex || !gTeapotTex || !gTeapotNormalTex ||
        !gCubeTex      || !gCubeNormalTex || !gLightTex)
    {
        gLastError = "Error loading textures";
        return false;
    }


    //**** Shadow Map Setup ****//

    D3D11_TEXTURE2D_DESC shadowDesc = {};
    shadowDesc.Width            = gShadowMapSize;
    shadowDesc.Height           = gShadowMapSize;
    shadowDesc.MipLevels        = 1;
    shadowDesc.ArraySize        = 1;
    shadowDesc.Format           = DXGI_FORMAT_R32_TYPELESS; // Typeless so depth buffer and shader see it differently
    shadowDesc.SampleDesc.Count = 1;
    shadowDesc.Usage            = D3D11_USAGE_DEFAULT;
    shadowDesc.BindFlags        = D3D10_BIND_DEPTH_STENCIL | D3D10_BIND_SHADER_RESOURCE;

    if (FAILED(gD3DDevice->CreateTexture2D(&shadowDesc, NULL, &gShadowMap1Texture)) ||
        FAILED(gD3DDevice->CreateTexture2D(&shadowDesc, NULL, &gShadowMap2Texture)))
    {
        gLastError = "Error creating shadow map textures";
        return false;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC shadowDSV = {};
    shadowDSV.Format             = DXGI_FORMAT_D32_FLOAT;
    shadowDSV.ViewDimension      = D3D11_DSV_DIMENSION_TEXTURE2D;
    shadowDSV.Texture2D.MipSlice = 0;
    shadowDSV.Flags              = 0;

    if (FAILED(gD3DDevice->CreateDepthStencilView(gShadowMap1Texture, &shadowDSV, &gShadowMap1DepthStencil)) ||
        FAILED(gD3DDevice->CreateDepthStencilView(gShadowMap2Texture, &shadowDSV, &gShadowMap2DepthStencil)))
    {
        gLastError = "Error creating shadow map depth stencil views";
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC shadowSRV = {};
    shadowSRV.Format                    = DXGI_FORMAT_R32_FLOAT; // Shaders see depth as a red float
    shadowSRV.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    shadowSRV.Texture2D.MostDetailedMip = 0;
    shadowSRV.Texture2D.MipLevels       = 1;

    if (FAILED(gD3DDevice->CreateShaderResourceView(gShadowMap1Texture, &shadowSRV, &gShadowMap1SRV)) ||
        FAILED(gD3DDevice->CreateShaderResourceView(gShadowMap2Texture, &shadowSRV, &gShadowMap2SRV)))
    {
        gLastError = "Error creating shadow map shader resource views";
        return false;
    }


    //**** Portal Texture Setup ****//

    D3D11_TEXTURE2D_DESC portalColourDesc = {};
    portalColourDesc.Width            = gPortalWidth;
    portalColourDesc.Height           = gPortalHeight;
    portalColourDesc.MipLevels        = 1;
    portalColourDesc.ArraySize        = 1;
    portalColourDesc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    portalColourDesc.SampleDesc.Count = 1;
    portalColourDesc.Usage            = D3D11_USAGE_DEFAULT;
    portalColourDesc.BindFlags        = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;

    if (FAILED(gD3DDevice->CreateTexture2D(&portalColourDesc, NULL, &gPortalTexture)))
    {
        gLastError = "Error creating portal texture";
        return false;
    }
    if (FAILED(gD3DDevice->CreateRenderTargetView(gPortalTexture, NULL, &gPortalRenderTarget)))
    {
        gLastError = "Error creating portal render target view";
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC portalSRV = {};
    portalSRV.Format                    = portalColourDesc.Format;
    portalSRV.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    portalSRV.Texture2D.MostDetailedMip = 0;
    portalSRV.Texture2D.MipLevels       = 1;

    if (FAILED(gD3DDevice->CreateShaderResourceView(gPortalTexture, &portalSRV, &gPortalTextureSRV)))
    {
        gLastError = "Error creating portal shader resource view";
        return false;
    }


    //**** Portal Depth Buffer Setup ****//

    D3D11_TEXTURE2D_DESC portalDepthDesc = {};
    portalDepthDesc.Width            = gPortalWidth;
    portalDepthDesc.Height           = gPortalHeight;
    portalDepthDesc.MipLevels        = 1;
    portalDepthDesc.ArraySize        = 1;
    portalDepthDesc.Format           = DXGI_FORMAT_D32_FLOAT;
    portalDepthDesc.SampleDesc.Count = 1;
    portalDepthDesc.Usage            = D3D11_USAGE_DEFAULT;
    portalDepthDesc.BindFlags        = D3D10_BIND_DEPTH_STENCIL;

    if (FAILED(gD3DDevice->CreateTexture2D(&portalDepthDesc, NULL, &gPortalDepthStencil)))
    {
        gLastError = "Error creating portal depth stencil texture";
        return false;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC portalDSV = {};
    portalDSV.Format             = portalDepthDesc.Format;
    portalDSV.ViewDimension      = D3D11_DSV_DIMENSION_TEXTURE2D;
    portalDSV.Texture2D.MipSlice = 0;
    portalDSV.Flags              = 0;

    if (FAILED(gD3DDevice->CreateDepthStencilView(gPortalDepthStencil, &portalDSV, &gPortalDepthStencilView)))
    {
        gLastError = "Error creating portal depth stencil view";
        return false;
    }


    // Create GPU sampler, blend, rasteriser and depth states
    if (!CreateStates())
    {
        gLastError = "Error creating states";
        return false;
    }

    return true;
}


//--------------------------------------------------------------------------------------
// Initialise Scene Layout
//--------------------------------------------------------------------------------------

bool InitScene()
{
    auto& scene = SceneManager::Get();

    // Models
    gCharacter = scene.AddModel("character", "character", { 15,  0,    0 }, { 0, ToRadians(215.0f), 0 }, 6.0f);
    gCrate     = scene.AddModel("crate",     "crate",     { 40,  0,   30 }, { 0, ToRadians(-20.0f), 0 }, 6.0f);
    gFloor     = scene.AddModel("floor",     "floor");
    gSphere    = scene.AddModel("sphere",    "sphere",    { -25, 10,   0 }, { 0, 0, 0 }, 0.75f);
    gTeapot    = scene.AddModel("teapot",    "teapot",    {  40, 10, -40 }, { 0, 0, 0 }, 1.5f);
    gCube      = scene.AddModel("cube",      "cube",      {  20, 15, -100}, { 0, 0, 0 }, 3.0f);
    gPortal    = scene.AddModel("portal",    "portal",    { 100, 20,  40 }, { 0, ToRadians(-130.0f), 0 });
    gSphere2   = scene.AddModel("sphere2",   "sphere",    {  20, 15,   0 }, { 0, 0, 0 }, 0.75f);

    // Cameras
    gPortalCamera = scene.AddCamera("portalCam", { 45, 45,  85 }, { ToRadians(20.0f),  ToRadians(215.0f), 0 });
    gCamera       = scene.AddCamera("mainCam",   { 15, 30, -70 }, { ToRadians(13.0f),  0,                 0 });

    // Cube map render targets
    BuildDynamicCubeMapViews();

    // Lights
    auto* l0 = scene.AddLight("light0", "light", { 0.8f, 0.8f, 1.0f },  3.0f,  Light::Type::Point);
    auto* l1 = scene.AddLight("light1", "light", { 1.0f, 0.8f, 0.2f }, 10.0f,  Light::Type::Point);
    auto* l2 = scene.AddLight("light2", "light", { 0.6f, 0.6f, 0.5f },  0.2f,  Light::Type::Directional);
    auto* l3 = scene.AddLight("light3", "light", { 1.0f, 0.8f, 0.2f }, 60.0f,  Light::Type::Spot);

    l0->GetModel()->SetPosition({  30,  40, -40 });
    l0->GetModel()->SetScale(pow(3.0f,  0.7f));
    l0->GetModel()->FaceTarget(gCharacter->Position());

    l1->GetModel()->SetPosition({ -20, 150, -70 });
    l1->GetModel()->SetScale(pow(10.0f, 0.7f));
    l1->GetModel()->FaceTarget({ 0, 0, 0 });

    l3->GetModel()->SetPosition({ -50,  30, 100 });
    l3->GetModel()->SetScale(pow(60.0f, 0.7f));
    l3->GetModel()->FaceTarget({ 0, 0, 0 });

    return true;
}


//--------------------------------------------------------------------------------------
// Release All Scene Resources
//--------------------------------------------------------------------------------------

void ReleaseResources()
{
    ReleaseStates();
    ReleaseShaders();

    // Shadow maps
    if (gShadowMap1DepthStencil) { gShadowMap1DepthStencil->Release(); gShadowMap1DepthStencil = nullptr; }
    if (gShadowMap1SRV)          { gShadowMap1SRV->Release();          gShadowMap1SRV          = nullptr; }
    if (gShadowMap1Texture)      { gShadowMap1Texture->Release();      gShadowMap1Texture      = nullptr; }
    if (gShadowMap2DepthStencil) { gShadowMap2DepthStencil->Release(); gShadowMap2DepthStencil = nullptr; }
    if (gShadowMap2SRV)          { gShadowMap2SRV->Release();          gShadowMap2SRV          = nullptr; }
    if (gShadowMap2Texture)      { gShadowMap2Texture->Release();      gShadowMap2Texture      = nullptr; }

    // Portal
    if (gPortalDepthStencilView) { gPortalDepthStencilView->Release(); gPortalDepthStencilView = nullptr; }
    if (gPortalDepthStencil)     { gPortalDepthStencil->Release();     gPortalDepthStencil     = nullptr; }
    if (gPortalTextureSRV)       { gPortalTextureSRV->Release();       gPortalTextureSRV       = nullptr; }
    if (gPortalRenderTarget)     { gPortalRenderTarget->Release();     gPortalRenderTarget     = nullptr; }
    if (gPortalTexture)          { gPortalTexture->Release();          gPortalTexture          = nullptr; }

    // Cube map
    for (int i = 0; i < 6; ++i)
    {
        if (mDynamicCubeMapRTV[i]) { mDynamicCubeMapRTV[i]->Release(); mDynamicCubeMapRTV[i] = nullptr; }
        if (mDynamicCubeMapDSV[i]) { mDynamicCubeMapDSV[i]->Release(); mDynamicCubeMapDSV[i] = nullptr; }
    }
    if (mDynamicCubeMapSRV) { mDynamicCubeMapSRV->Release(); mDynamicCubeMapSRV = nullptr; }

    // Constant buffers
    if (gPerModelConstantBuffer) { gPerModelConstantBuffer->Release(); gPerModelConstantBuffer = nullptr; }
    if (gPerFrameConstantBuffer) { gPerFrameConstantBuffer->Release(); gPerFrameConstantBuffer = nullptr; }

    // All meshes, models, textures, lights and cameras managed by SceneManager
    SceneManager::Get().ReleaseAll();
}


//--------------------------------------------------------------------------------------
// Scene Rendering
//--------------------------------------------------------------------------------------

// Renders the scene from the given spotlight's point of view, writing depth values only
static void RenderDepthBufferFromLight(int lightIndex)
{
    gPerFrameConstants.viewMatrix           = CalculateLightViewMatrix(lightIndex);
    gPerFrameConstants.projectionMatrix     = CalculateLightProjectionMatrix(lightIndex);
    gPerFrameConstants.viewProjectionMatrix = gPerFrameConstants.viewMatrix * gPerFrameConstants.projectionMatrix;
    UpdateConstantBuffer(gPerFrameConstantBuffer, gPerFrameConstants);

    gD3DContext->VSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer);
    gD3DContext->PSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer);

    // Depth-only shaders, no textures required
    gD3DContext->VSSetShader(gBasicTransformVertexShader, nullptr, 0);
    gD3DContext->PSSetShader(gDepthOnlyPixelShader,       nullptr, 0);

    gD3DContext->OMSetBlendState(gNoBlendingState,      nullptr, 0xffffff);
    gD3DContext->OMSetDepthStencilState(gUseDepthBufferState, 0);
    gD3DContext->RSSetState(gCullBackState);

    gSphere->Render();
    gTeapot->Render();
    gCharacter->Render();
    gCrate->Render();
    gFloor->Render();
    gCube->Render();
}


// Renders the full scene from the given camera. skipModel is excluded (used to hide
// the reflective sphere from its own cube map capture)
void RenderSceneFromCamera(Camera* camera, Model* skipModel /*= nullptr*/)
{
    // Cache SRVs for this frame to avoid repeated pointer dereferences
    auto srvChar = gCharacterTex   ->GetSRV();
    auto srvCrate= gCrateTex       ->GetSRV();
    auto srvF    = gFloorTex       ->GetSRV();
    auto srvFN   = gFloorNormalTex ->GetSRV();
    auto srvS    = gSphereTex      ->GetSRV();
    auto srvSN   = gSphereNormalTex->GetSRV();
    auto srvT    = gTeapotTex      ->GetSRV();
    auto srvTN   = gTeapotNormalTex->GetSRV();
    auto srvC    = gCubeTex        ->GetSRV();
    auto srvCN   = gCubeNormalTex  ->GetSRV();
    auto srvL    = gLightTex       ->GetSRV();

    auto& scene = SceneManager::Get();

    // Upload camera matrices to the GPU constant buffer
    gPerFrameConstants.viewMatrix           = camera->ViewMatrix();
    gPerFrameConstants.projectionMatrix     = camera->ProjectionMatrix();
    gPerFrameConstants.viewProjectionMatrix = camera->ViewProjectionMatrix();
    UpdateConstantBuffer(gPerFrameConstantBuffer, gPerFrameConstants);

    gD3DContext->VSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer);
    gD3DContext->PSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer);

    ID3D11ShaderResourceView* nullSRV = nullptr;

    // Clear slots 1 and 2 before models that don't use normal/shadow maps
    gD3DContext->PSSetShaderResources(1, 1, &nullSRV);
    gD3DContext->PSSetShaderResources(2, 1, &nullSRV);


    //// Opaque lit models ////

    gD3DContext->VSSetShader(gPixelLightingVertexShader, nullptr, 0);
    gD3DContext->PSSetShader(gPixelLightingPixelShader,  nullptr, 0);
    gD3DContext->OMSetBlendState(gNoBlendingState,            nullptr, 0xffffff);
    gD3DContext->OMSetDepthStencilState(gUseDepthBufferState, 0);
    gD3DContext->RSSetState(gCullBackState);
    gD3DContext->PSSetSamplers(0, 1, &gAnisotropic4xSampler);

    // Character
    gD3DContext->PSSetShaderResources(0, 1, &srvChar);
    gCharacter->Render();

    // Crate
    gD3DContext->PSSetShaderResources(0, 1, &srvCrate);
    gCrate->Render();

    // Floor - normal mapped with shadow map
    gD3DContext->VSSetShader(gNormalMappingVertexShader, nullptr, 0);
    gD3DContext->PSSetShader(gNormalMappingPixelShader,  nullptr, 0);
    gD3DContext->PSSetShaderResources(0, 1, &srvF);
    gD3DContext->PSSetShaderResources(1, 1, &srvFN);
    gD3DContext->PSSetShaderResources(2, 1, &gShadowMap1SRV);
    gFloor->Render();
    gD3DContext->PSSetShaderResources(1, 1, &nullSRV);
    gD3DContext->PSSetShaderResources(2, 1, &nullSRV);

    // Sphere - normal mapped
    gD3DContext->PSSetShaderResources(0, 1, &srvS);
    gD3DContext->PSSetShaderResources(1, 1, &srvSN);
    gSphere->Render();
    gD3DContext->PSSetShaderResources(1, 1, &nullSRV);

    // Portal object - uses the portal render texture as its diffuse
    gD3DContext->VSSetShader(gPixelLightingVertexShader, nullptr, 0);
    gD3DContext->PSSetShader(gPixelLightingPixelShader,  nullptr, 0);
    gD3DContext->PSSetShaderResources(0, 1, &gPortalTextureSRV);
    gPortal->Render();

    // Reflective sphere - environment cube map
    gD3DContext->PSSetShader(gCubeMapPixelShader, nullptr, 0);
    gD3DContext->OMSetBlendState(gNoBlendingState,            nullptr, 0xffffff);
    gD3DContext->OMSetDepthStencilState(gUseDepthBufferState, 0);
    gD3DContext->RSSetState(gCullBackState);
    gD3DContext->PSSetShaderResources(0, 1, &mDynamicCubeMapSRV);
    if (gSphere2 != skipModel)
    {
        gSphere2->Render();
    }
    gD3DContext->PSSetShaderResources(0, 1, &nullSRV);


    //// Additive / transparent models ////

    // Light billboards
    gD3DContext->VSSetShader(gBasicTransformVertexShader, nullptr, 0);
    gD3DContext->PSSetShader(gLightModelPixelShader,      nullptr, 0);
    gD3DContext->PSSetShaderResources(0, 1, &srvL);
    gD3DContext->PSSetSamplers(0, 1, &gTrilinearSampler);
    gD3DContext->OMSetBlendState(gAdditiveBlendingState,   nullptr, 0xffffff);
    gD3DContext->OMSetDepthStencilState(gDepthReadOnlyState, 0);
    gD3DContext->RSSetState(gCullNoneState);
    for (auto& light : scene.GetAllLights())
    {
        gPerModelConstants.objectColour = light->GetColour();
        light->Render();
    }

    // Teapot - colour-shift normal mapped, additive blending
    gD3DContext->VSSetShader(gNormalMappingVertexShader,              nullptr, 0);
    gD3DContext->PSSetShader(gColourShiftNormalMappingPixelShader,    nullptr, 0);
    gD3DContext->OMSetBlendState(gAdditiveBlendingState,              nullptr, 0xffffff);
    gD3DContext->OMSetDepthStencilState(gDepthReadOnlyState,          0);
    gD3DContext->RSSetState(gCullNoneState);
    gD3DContext->PSSetShaderResources(0, 1, &srvT);
    gD3DContext->PSSetShaderResources(1, 1, &srvTN);
    gD3DContext->PSSetSamplers(0, 1, &gAnisotropic4xSampler);
    gTeapot->Render();

    // Cube - parallax mapped, back-face culled
    gD3DContext->VSSetShader(gParallaxMappingVertexShader, nullptr, 0);
    gD3DContext->PSSetShader(gParallaxMappingPixelShader,  nullptr, 0);
    gD3DContext->PSSetShaderResources(0, 1, &srvC);
    gD3DContext->PSSetShaderResources(1, 1, &srvCN);
    gD3DContext->PSSetSamplers(0, 1, &gAnisotropic4xSampler);
    gD3DContext->OMSetBlendState(gNoBlendingState,            nullptr, 0xffffff);
    gD3DContext->OMSetDepthStencilState(gUseDepthBufferState, 0);
    gD3DContext->RSSetState(gCullBackState);
    gCube->Render();
}


// Top-level render function - renders portal, shadow map, cube map, then the main scene
void RenderScene()
{
    auto& scene = SceneManager::Get();

    // Upload light data to the per-frame constant buffer
    auto* l0 = scene.GetLight("light0");
    auto* l1 = scene.GetLight("light1");
    auto* l2 = scene.GetLight("light2");
    auto* l3 = scene.GetLight("light3");

    gPerFrameConstants.gLight1Position        = l0->GetPosition();
    gPerFrameConstants.gLight1Colour          = l0->GetColour();
    gPerFrameConstants.gLight2Position        = l1->GetPosition();
    gPerFrameConstants.gLight2Colour          = l1->GetColour();
    gPerFrameConstants.gLight3Direction       = Normalise(gLight3Direction);
    gPerFrameConstants.gLight3Colour          = l2->GetColour();
    gPerFrameConstants.gLight4Position        = l3->GetPosition();
    gPerFrameConstants.gLight4Colour          = l3->GetColour();
    gPerFrameConstants.gLight4Facing          = Normalise(l3->GetModel()->WorldMatrix().GetZAxis());
    gPerFrameConstants.gLight4CosHalfAngle    = cos(ToRadians(gSpotlightConeAngle / 2));
    gPerFrameConstants.gLight4ViewMatrix      = CalculateLightViewMatrix(3);
    gPerFrameConstants.gLight4ProjectionMatrix= CalculateLightProjectionMatrix(3);
    gPerFrameConstants.gAmbientColour         = gAmbientColour;
    gPerFrameConstants.gSpecularPower         = gSpecularPower;
    gPerFrameConstants.gFrameTime             = gFrameTimeGlobal;
    gPerFrameConstants.parallaxDepth          = gParallaxDepth;


    //// Portal Pass ////

    gD3DContext->OMSetRenderTargets(1, &gPortalRenderTarget, gPortalDepthStencilView);
    gD3DContext->ClearRenderTargetView(gPortalRenderTarget,          &gBackgroundColor.r);
    gD3DContext->ClearDepthStencilView(gPortalDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    D3D11_VIEWPORT portalVP = { 0, 0, (FLOAT)gPortalWidth, (FLOAT)gPortalHeight, 0.0f, 1.0f };
    gD3DContext->RSSetViewports(1, &portalVP);
    RenderSceneFromCamera(gPortalCamera, nullptr);


    //// Shadow Map Pass ////

    D3D11_VIEWPORT shadowVP = { 0, 0, (FLOAT)gShadowMapSize, (FLOAT)gShadowMapSize, 0.0f, 1.0f };
    gD3DContext->RSSetViewports(1, &shadowVP);
    gD3DContext->OMSetRenderTargets(0, nullptr, gShadowMap1DepthStencil);
    gD3DContext->ClearDepthStencilView(gShadowMap1DepthStencil, D3D11_CLEAR_DEPTH, 1.0f, 0);
    RenderDepthBufferFromLight(3);


    //// Cube Map Pass ////

    BuildCubeFaceCamera(gSphere2->Position().x, gSphere2->Position().y, gSphere2->Position().z);

    D3D11_VIEWPORT cubeVP = { 0, 0, (FLOAT)gCubeMapSize, (FLOAT)gCubeMapSize, 0.0f, 1.0f };
    for (int i = 0; i < 6; ++i)
    {
        gD3DContext->OMSetRenderTargets(1, &mDynamicCubeMapRTV[i], mDynamicCubeMapDSV[i]);
        gD3DContext->ClearRenderTargetView(mDynamicCubeMapRTV[i],          &gBackgroundColor.r);
        gD3DContext->ClearDepthStencilView(mDynamicCubeMapDSV[i], D3D11_CLEAR_DEPTH, 1.0f, 0);
        gD3DContext->RSSetViewports(1, &cubeVP);

        gPerFrameConstants.gCameraPosition = gCubeMapCameras[i].Position();
        RenderSceneFromCamera(&gCubeMapCameras[i], gSphere2);
    }
    gD3DContext->GenerateMips(mDynamicCubeMapSRV);


    //// Main Scene Pass ////

    auto* backBuffer   = GraphicsDevice::Get().GetBackBuffer();
    auto* depthStencil = GraphicsDevice::Get().GetDepthStencil();
    gD3DContext->OMSetRenderTargets(1, &backBuffer, depthStencil);
    gD3DContext->ClearRenderTargetView(backBuffer,   &gBackgroundColor.r);
    gD3DContext->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH, 1.0f, 0);

    D3D11_VIEWPORT mainVP = { 0, 0, (FLOAT)gViewportWidth, (FLOAT)gViewportHeight, 0.0f, 1.0f };
    gD3DContext->RSSetViewports(1, &mainVP);

    // Bind shadow maps before the main render
    gD3DContext->PSSetShaderResources(3, 1, &gShadowMap1SRV);
    gD3DContext->PSSetShaderResources(4, 1, &gShadowMap2SRV);
    gD3DContext->PSSetSamplers(1, 1, &gPointSampler);

    RenderSceneFromCamera(gCamera, nullptr);

    // Unbind shadow maps to avoid D3D11 warnings next frame
    ID3D11ShaderResourceView* nullView = nullptr;
    gD3DContext->PSSetShaderResources(3, 1, &nullView);
    gD3DContext->PSSetShaderResources(4, 1, &nullView);


    // Present the finished frame (1 = vsync locked, 0 = uncapped)
    gSwapChain->Present(lockFPS ? 1 : 0, 0);
}


//--------------------------------------------------------------------------------------
// Scene Update
//--------------------------------------------------------------------------------------

void UpdateScene(float frameTime)
{
    // Allow the character to be moved with keyboard
    gCharacter->Control(frameTime, Key_I, Key_K, Key_J, Key_L, Key_U, Key_O, Key_Period, Key_Comma);

    // Accumulate total time for shader animations
    static float totalTime = 0.0f;
    totalTime += frameTime;
    gFrameTimeGlobal = totalTime;

    // Light 0 - flashes on and off every 0.5 seconds
    flashTimer += frameTime;
    if (flashTimer > 0.5f)
    {
        flashOn    = !flashOn;
        flashTimer = 0.0f;
    }
    SceneManager::Get().GetLight("light0")->SetColour(
        flashOn ? CVector3{ 0.8f, 0.8f, 1.0f } : CVector3{ 0.0f, 0.0f, 0.0f });

    // Light 1 - cycles through colours using sine waves
    static float colourTime = 0.0f;
    colourTime += frameTime;
    SceneManager::Get().GetLight("light1")->SetColour({
        sin(colourTime)          * 0.5f + 0.5f,
        sin(colourTime + 2.09f)  * 0.5f + 0.5f,
        sin(colourTime + 4.18f)  * 0.5f + 0.5f,
    });

    // Toggle parallax depth on key 1
    if (KeyHit(Key_1))
        gParallaxDepth = (gParallaxDepth > 0.0f) ? 0.0f : 0.1f;

    // Camera controls
    gCamera->Control(frameTime, Key_Up, Key_Down, Key_Left, Key_Right, Key_W, Key_S, Key_A, Key_D);

    // Toggle FPS cap
    if (KeyHit(Key_P)) lockFPS = !lockFPS;

    // Display frame time and FPS in the window title bar
    static float titleTimer  = 0.0f;
    static int   frameCount  = 0;
    titleTimer += frameTime;
    ++frameCount;
    if (titleTimer > 0.5f)
    {
        float avgFrameTime = titleTimer / frameCount;
        std::ostringstream ss;
        ss.precision(2);
        ss << std::fixed << avgFrameTime * 1000;
        std::string title = "CO2409 Assignment - Tristan Walmsley   "
                          + ss.str() + "ms  |  FPS: "
                          + std::to_string(static_cast<int>(1.0f / avgFrameTime + 0.5f));
        SetWindowTextA(gHWnd, title.c_str());
        titleTimer = 0.0f;
        frameCount = 0;
    }
}