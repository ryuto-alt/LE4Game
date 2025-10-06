#pragma once
#include "Mymath.h"
#include "Mymath.h"
#include "Frustum.h"
#include <Windows.h>

// カメラモードの定数
enum CameraMode {
    CAMERA_MODE_FREE = 0,     // フリーカメラ（自由移動）
    CAMERA_MODE_FIXED = 1,    // 固定カメラ
    CAMERA_MODE_ORBIT = 2     // オービットカメラ（プレイヤー追従）
};

// カメラクラス - 3Dオブジェクトからカメラ機能を分離
class Camera {
public:
    // コンストラクタ・デストラクタ
    Camera();
    ~Camera();

    // 更新処理 - 毎フレーム行列を更新する
    void Update();

    // セッター
    void SetRotate(const Vector3& rotate);
    void SetTranslate(const Vector3& translate);
    void SetFov(float fov);
    void SetFovY(float fovY);  // 後方互換性のため
    void SetAspectRatio(float aspectRatio);
    void SetNearClip(float nearClip);
    void SetFarClip(float farClip);

    // マウス視点移動関連
    void ProcessMouseInput(float deltaX, float deltaY);
    void SetMouseSensitivity(float sensitivity);
    void SetCameraMode(int mode);
    int GetCameraMode() const;
    void ToggleCameraMode();
    
#ifdef _DEBUG
    // デバッグ用フリーカメラモード切り替え
    void ToggleFreeCameraMode();
    bool IsFreeCameraMode() const;
#endif
    
    // 右スティック視点移動関連
    void ProcessRightStickInput(float deltaX, float deltaY);
    void SetStickSensitivity(float sensitivity);
    float GetStickSensitivity() const;
    
    // オービットカメラ機能
    void SetOrbitTarget(const Vector3& target);
    void SetOrbitDistance(float distance);
    void SetOrbitHeight(float height);
    void UpdateOrbitCamera();
    Vector3 GetOrbitTarget() const { return orbitTarget_; }
    float GetOrbitDistance() const { return orbitDistance_; }
    float GetOrbitHeight() const { return orbitHeight_; }
    
    // マウス視点制御機能
    void ToggleMouseLook();
    bool IsMouseLookEnabled() const;
    void SetMouseLookEnabled(bool enabled);
    void UpdateMouseControl();
    void SetWindowHandle(HWND hwnd);
    
    // カメラ移動関連（フリーカメラモード用）
    void MoveForward(float distance);
    void MoveRight(float distance);
    void MoveUp(float distance);
    Vector3 GetForwardVector() const;
    Vector3 GetRightVector() const;
    Vector3 GetUpVector() const;

    // ゲッター
    const Matrix4x4& GetWorldMatrix() const;
    const Matrix4x4& GetViewMatrix() const;
    const Matrix4x4& GetProjectionMatrix() const;
    const Matrix4x4& GetViewProjectionMatrix() const;
    const Vector3& GetRotate() const;
    const Vector3& GetTranslate() const;
    float GetFovY() const;
    float GetAspectRatio() const;
    float GetNearClip() const;
    float GetFarClip() const;

    // フラスタムカリング関連
    const Frustum& GetFrustum() const;
    bool IsPointInFrustum(const Vector3& point) const;
    bool IsAABBInFrustum(const Vector3& min, const Vector3& max) const;
    bool IsSphereInFrustum(const Vector3& center, float radius) const;

private:
    // ビュー行列関連データ
    Transform transform_;       // カメラのトランスフォーム
    Matrix4x4 worldMatrix_;     // カメラのワールド行列
    Matrix4x4 viewMatrix_;      // ビュー行列

    // プロジェクション行列関連データ
    Matrix4x4 projectionMatrix_; // プロジェクション行列
    float fovY_;                // 視野角
    float aspectRatio_;         // アスペクト比
    float nearClip_;            // ニアクリップ距離
    float farClip_;             // ファークリップ距離

    // 合成行列 - ビュー行列とプロジェクション行列の積
    Matrix4x4 viewProjectionMatrix_;

    // マウス視点移動関連
    float mouseSensitivity_;    // マウス感度
    int cameraMode_;            // カメラモード (0: フリーカメラ, 1: 固定カメラ)
    
    // 右スティック視点移動関連
    float stickSensitivity_;    // スティック感度
    
    // オービットカメラ関連
    Vector3 orbitTarget_;       // 回転の中心点
    float orbitDistance_;       // ターゲットからの距離
    float orbitHeight_;         // ターゲットからの高さオフセット
    float orbitAngleX_;         // オービット縦回転角度（上下）
    float orbitAngleY_;         // オービット横回転角度（左右）
    
    // マウス制御用変数
    bool mouseLookEnabled_;     // マウス視点移動が有効かどうか
    HWND windowHandle_;         // ウィンドウハンドル
    POINT lastMousePos_;        // 前回のマウス位置
    RECT windowRect_;           // ウィンドウ矩形

    // フラスタムカリング
    Frustum frustum_;           // 視錐台
};

// 静的なデフォルトカメラの定義
class Object3dCommon {
public:
    // デフォルトカメラの設定・取得
    static void SetDefaultCamera(Camera* camera);
    static Camera* GetDefaultCamera();

private:
    // デフォルトカメラ
    static Camera* defaultCamera_;
};