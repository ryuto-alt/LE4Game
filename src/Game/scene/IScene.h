#pragma once

#include "DirectXCommon.h"
#include "Input.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "Camera.h"
#include "SrvManager.h"
#include "AudioManager.h"
#include "WinApp.h"
#include <memory>

class SceneManager;
class GameObjectManager;
class LightManager;
class GameObject;
class UnoEngine;

class IScene {
public:
    virtual ~IScene() = default;

    virtual void Initialize() = 0;
    virtual void Update() = 0;
    virtual void Draw() = 0;
    virtual void Finalize() = 0;

    void SetSceneManager(SceneManager* sceneManager) { sceneManager_ = sceneManager; }

    void SetDirectXCommon(DirectXCommon* dxCommon) { dxCommon_ = dxCommon; }
    void SetInput(Input* input) { input_ = input; }
    void SetSpriteCommon(SpriteCommon* spriteCommon) { spriteCommon_ = spriteCommon; }
    void SetSrvManager(SrvManager* srvManager) { srvManager_ = srvManager; }
    void SetCamera(Camera* camera) { camera_ = camera; }

protected:
    SceneManager* sceneManager_ = nullptr;

    DirectXCommon* dxCommon_ = nullptr;
    Input* input_ = nullptr;
    SpriteCommon* spriteCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Camera* camera_ = nullptr;
};