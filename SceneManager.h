#pragma once
#include "Common.h"
#include "Mesh.h"
#include "Model.h"
#include "Camera.h"
#include "Light.h"
#include "Texture.h"
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

class SceneManager
{
public:
    static SceneManager& Get()
    {
        static SceneManager instance;
        return instance;
    }

    // Resource loading
    Mesh*    LoadMesh   (const std::string& id, const std::string& file, bool tangents = false);
    Texture* LoadTexture(const std::string& id, const std::string& file);
    Model*   AddModel   (const std::string& id, const std::string& meshId,
                         CVector3 position = {0,0,0}, CVector3 rotation = {0,0,0}, float scale = 1.0f);
    Light*   AddLight   (const std::string& id, const std::string& meshId,
                         CVector3 colour, float strength, Light::Type type = Light::Type::Point);
    Camera*  AddCamera  (const std::string& id, CVector3 position = {0,0,0},
                         CVector3 rotation = {0,0,0});

    // Accessors by id
    Mesh*    GetMesh   (const std::string& id);
    Texture* GetTexture(const std::string& id);
    Model*   GetModel  (const std::string& id);
    Light*   GetLight  (const std::string& id);
    Camera*  GetCamera (const std::string& id);

    // Access all of a type (for iterating)
    std::vector<std::unique_ptr<Model>>&   GetAllModels()  { return mModelList;  }
    std::vector<std::unique_ptr<Light>>&   GetAllLights()  { return mLightList;  }
    std::vector<std::unique_ptr<Camera>>&  GetAllCameras() { return mCameraList; }

    void ReleaseAll();
    ~SceneManager() { ReleaseAll(); }

private:
    SceneManager() = default;
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    std::unordered_map<std::string, std::unique_ptr<Mesh>>    mMeshes;
    std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
    std::unordered_map<std::string, std::unique_ptr<Model>>   mModels;
    std::unordered_map<std::string, std::unique_ptr<Light>>   mLights;
    std::unordered_map<std::string, std::unique_ptr<Camera>>  mCameras;

    // Keep ordered lists for iteration
    std::vector<std::unique_ptr<Model>>  mModelList;
    std::vector<std::unique_ptr<Light>>  mLightList;
    std::vector<std::unique_ptr<Camera>> mCameraList;
};