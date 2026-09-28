#include "SceneManager.h"

Mesh* SceneManager::LoadMesh(const std::string& id, const std::string& file, bool tangents)
{
    auto mesh = std::make_unique<Mesh>(file, tangents);
    Mesh* ptr = mesh.get();
    mMeshes[id] = std::move(mesh);
    return ptr;
}

Texture* SceneManager::LoadTexture(const std::string& id, const std::string& file)
{
    auto tex = std::make_unique<Texture>();
    if (!tex->Load(file)) return nullptr;
    Texture* ptr = tex.get();
    mTextures[id] = std::move(tex);
    return ptr;
}

Model* SceneManager::AddModel(const std::string& id, const std::string& meshId,
                               CVector3 position, CVector3 rotation, float scale)
{
    Mesh* mesh = GetMesh(meshId);
    if (!mesh) return nullptr;
    auto model = std::make_unique<Model>(mesh, position, rotation, scale);
    Model* ptr = model.get();
    mModels[id] = std::move(model);
    mModelList.push_back(std::make_unique<Model>(mesh, position, rotation, scale));
    return ptr;
}

Light* SceneManager::AddLight(const std::string& id, const std::string& meshId,
                               CVector3 colour, float strength, Light::Type type)
{
    Mesh* mesh = GetMesh(meshId);
    if (!mesh) return nullptr;
    auto model = std::make_unique<Model>(mesh);
    auto light = std::make_unique<Light>(model.get(), colour, strength, type);
    // Store model under same id so it stays alive
    mModels[id + "_model"] = std::move(model);
    Light* ptr = light.get();
    mLights[id] = std::move(light);
    mLightList.push_back(std::make_unique<Light>(ptr->GetModel(), colour, strength, type));
    return ptr;
}

Camera* SceneManager::AddCamera(const std::string& id, CVector3 position, CVector3 rotation)
{
    auto cam = std::make_unique<Camera>(position, rotation);
    Camera* ptr = cam.get();
    mCameras[id] = std::move(cam);
    mCameraList.push_back(std::make_unique<Camera>(position, rotation));
    return ptr;
}

Mesh*    SceneManager::GetMesh   (const std::string& id) { auto it = mMeshes.find(id);    return it != mMeshes.end()    ? it->second.get() : nullptr; }
Texture* SceneManager::GetTexture(const std::string& id) { auto it = mTextures.find(id);  return it != mTextures.end()  ? it->second.get() : nullptr; }
Model*   SceneManager::GetModel  (const std::string& id) { auto it = mModels.find(id);    return it != mModels.end()    ? it->second.get() : nullptr; }
Light*   SceneManager::GetLight  (const std::string& id) { auto it = mLights.find(id);    return it != mLights.end()    ? it->second.get() : nullptr; }
Camera*  SceneManager::GetCamera (const std::string& id) { auto it = mCameras.find(id);   return it != mCameras.end()   ? it->second.get() : nullptr; }

void SceneManager::ReleaseAll()
{
    mModelList.clear();
    mLightList.clear();
    mCameraList.clear();
    mModels.clear();
    mLights.clear();
    mCameras.clear();
    mTextures.clear();
    mMeshes.clear();
}