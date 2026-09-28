#pragma once
#include "Common.h"
#include "GraphicsHelpers.h"
#include <string>

class Texture
{
public:
    Texture() = default;

    bool Load(const std::string& filename)
    {
        return LoadTexture(filename, &mResource, &mSRV);
    }

    void Release()
    {
        if (mSRV)      { mSRV     ->Release(); mSRV      = nullptr; }
        if (mResource) { mResource->Release(); mResource = nullptr; }
    }

    ID3D11ShaderResourceView* GetSRV() const { return mSRV; }

    ~Texture() { Release(); }

private:
    ID3D11Resource*           mResource = nullptr;
    ID3D11ShaderResourceView* mSRV      = nullptr;
};