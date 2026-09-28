#pragma once
#pragma once

#include "Common.h"
#include "Model.h"
#include "CVector3.h"

class Light
{
public:
    enum class Type { Point, Directional, Spot };

    Light(Model* model, CVector3 colour, float strength, Type type = Type::Point)
        : mModel(model), mColour(colour), mStrength(strength), mType(type) {
    }

    // Getters
    Model* GetModel()    const { return mModel; }
    CVector3 GetColour()   const { return mColour * mStrength; }
    CVector3 GetPosition() const { return mModel->Position(); }
    CVector3 GetFacing()   const { return mModel->WorldMatrix().GetZAxis(); }
    Type     GetType()     const { return mType; }
    float    GetStrength() const { return mStrength; }

    // Setters
    void SetColour(CVector3 colour) { mColour = colour; }
    void SetStrength(float strength) { mStrength = strength; }

    void Render() { mModel->Render(); }

private:
    Model* mModel = nullptr;
    CVector3 mColour = { 1, 1, 1 };
    float    mStrength = 1.0f;
    Type     mType = Type::Point;
};