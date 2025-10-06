#include "Camera.h"
#include "RenderingPipeline.h"

// 静的メンバ変数の実体化
Camera* Object3dCommon::defaultCamera_ = nullptr;

Camera::Camera() :
    fovY_(1.57f), // 90度（π/2ラジアン）
    aspectRatio_(16.0f / 9.0f),
    nearClip_(0.01f),  
    farClip_(100.0f),
    mouseSensitivity_(0.003f),
    cameraMode_(CAMERA_MODE_ORBIT),  // デフォルトはオービットカメラ
    stickSensitivity_(2.0f),
    orbitTarget_(Vector3{0.0f, 0.0f, 0.0f}),
    orbitDistance_(5.0f),
    orbitHeight_(2.0f),
    orbitAngleX_(-0.2f),  // 軽く上から見下ろす角度（約-11度）
    orbitAngleY_(0.52f),  // 30度右から見る角度
    windowHandle_(nullptr),
    lastMousePos_({0, 0}),
    windowRect_({0, 0, 0, 0})
{
    // トランスフォームの初期設定
    transform_.scale = { 1.0f, 1.0f, 1.0f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    transform_.translate = { 0.0f, 0.0f, -5.0f };

    // マウス視点移動の初期設定
    mouseLookEnabled_ = false;  // 初期状態でOFF（TABキーで切り替え可能）

    // 初期更新
    Update();
}

Camera::~Camera() {
    // マウス制御をクリーンアップ
    if (mouseLookEnabled_ && windowHandle_) {
        ShowCursor(TRUE);
        ClipCursor(nullptr);
    }
    
    // デフォルトカメラが自分自身だった場合はnullptrにする
    if (Object3dCommon::GetDefaultCamera() == this) {
        Object3dCommon::SetDefaultCamera(nullptr);
    }
}

void Camera::Update() {
    // ワールド行列の計算
    worldMatrix_ = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);

    // ビュー行列の計算（従来の方法に戻す）
    viewMatrix_ = Inverse(worldMatrix_);

    // プロジェクション行列の計算
    projectionMatrix_ = MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);

    // ビュープロジェクション行列の計算
    viewProjectionMatrix_ = Multiply(viewMatrix_, projectionMatrix_);

    // フラスタムの更新
    frustum_.BuildFromViewProjection(viewProjectionMatrix_);
}

// セッター
void Camera::SetRotate(const Vector3& rotate) {
    transform_.rotate = rotate;
}

void Camera::SetTranslate(const Vector3& translate) {
    transform_.translate = translate;
}

void Camera::SetFov(float fov) {
    fovY_ = fov;
}

void Camera::SetFovY(float fovY) {
    SetFov(fovY);
}

void Camera::SetAspectRatio(float aspectRatio) {
    aspectRatio_ = aspectRatio;
}

void Camera::SetNearClip(float nearClip) {
    nearClip_ = nearClip;
}

void Camera::SetFarClip(float farClip) {
    farClip_ = farClip;
}

// ゲッター
const Matrix4x4& Camera::GetWorldMatrix() const {
    return worldMatrix_;
}

const Matrix4x4& Camera::GetViewMatrix() const {
    return viewMatrix_;
}

const Matrix4x4& Camera::GetProjectionMatrix() const {
    return projectionMatrix_;
}

const Matrix4x4& Camera::GetViewProjectionMatrix() const {
    return viewProjectionMatrix_;
}

const Vector3& Camera::GetRotate() const {
    return transform_.rotate;
}

const Vector3& Camera::GetTranslate() const {
    return transform_.translate;
}

float Camera::GetFovY() const {
    return fovY_;
}

float Camera::GetAspectRatio() const {
    return aspectRatio_;
}

float Camera::GetNearClip() const {
    return nearClip_;
}

float Camera::GetFarClip() const {
    return farClip_;
}

// マウス視点移動関連
void Camera::ProcessMouseInput(float deltaX, float deltaY) {
    // マウス視点移動が無効な場合は処理しない
    if (!mouseLookEnabled_) {
        return;
    }
    
#ifdef _DEBUG
    // フリーカメラモードの時は従来のカメラ回転
    if (cameraMode_ == CAMERA_MODE_FREE) {
        transform_.rotate.y += deltaX * mouseSensitivity_;  // 水平回転
        transform_.rotate.x += deltaY * mouseSensitivity_;  // 垂直回転
        
        // X軸回転を制限
        if (transform_.rotate.x > 1.57f) transform_.rotate.x = 1.57f;
        if (transform_.rotate.x < -1.57f) transform_.rotate.x = -1.57f;
        
        return;
    }
#endif
    
    // オービット角度を変更
    orbitAngleY_ += deltaX * mouseSensitivity_;  // 水平回転
    orbitAngleX_ += deltaY * mouseSensitivity_;  // 垂直回転
    
    // X軸回転を適切な範囲に制限（上下の回転制限）
    // 上限: +45度（0.785ラジアン）、下限: -30度（-0.52ラジアン）
    if (orbitAngleX_ > 0.785f) {
        orbitAngleX_ = 0.785f;
    }
    if (orbitAngleX_ < -0.52f) {
        orbitAngleX_ = -0.52f;
    }
    
    // Y軸回転を-π～πの範囲に正規化
    while (orbitAngleY_ > 3.14159f) {
        orbitAngleY_ -= 2.0f * 3.14159f;
    }
    while (orbitAngleY_ < -3.14159f) {
        orbitAngleY_ += 2.0f * 3.14159f;
    }
}

void Camera::SetMouseSensitivity(float sensitivity) {
    mouseSensitivity_ = sensitivity;
}

void Camera::SetCameraMode(int mode) {
    cameraMode_ = mode;
}

int Camera::GetCameraMode() const {
    return cameraMode_;
}

void Camera::ToggleCameraMode() {
    cameraMode_ = (cameraMode_ + 1) % 2;  // 0と 1を切り替え
}

#ifdef _DEBUG
// デバッグ用フリーカメラモード切り替え
void Camera::ToggleFreeCameraMode() {
    if (cameraMode_ == CAMERA_MODE_FREE) {
        cameraMode_ = CAMERA_MODE_ORBIT;  // オービットカメラに戻す
    } else {
        cameraMode_ = CAMERA_MODE_FREE;   // フリーカメラにする
    }
}

bool Camera::IsFreeCameraMode() const {
    return cameraMode_ == CAMERA_MODE_FREE;
}
#endif

// カメラ移動関連（フリーカメラモード用）
void Camera::MoveForward(float distance) {
    Vector3 forward = GetForwardVector();
    transform_.translate.x += forward.x * distance;
    transform_.translate.y += forward.y * distance;
    transform_.translate.z += forward.z * distance;
}

void Camera::MoveRight(float distance) {
    Vector3 right = GetRightVector();
    transform_.translate.x += right.x * distance;
    transform_.translate.y += right.y * distance;
    transform_.translate.z += right.z * distance;
}

void Camera::MoveUp(float distance) {
    Vector3 up = GetUpVector();
    transform_.translate.x += up.x * distance;
    transform_.translate.y += up.y * distance;
    transform_.translate.z += up.z * distance;
}

Vector3 Camera::GetForwardVector() const {
    // カメラのワールド行列からZ軸方向（前方向）を取得
    // DirectXは左手座標系でZ+が前方向
    return { worldMatrix_.m[2][0], worldMatrix_.m[2][1], worldMatrix_.m[2][2] };
}

Vector3 Camera::GetRightVector() const {
    // カメラのワールド行列からX軸方向（右方向）を取得
    return { worldMatrix_.m[0][0], worldMatrix_.m[0][1], worldMatrix_.m[0][2] };
}

Vector3 Camera::GetUpVector() const {
    // カメラのワールド行列からY軸方向（上方向）を取得
    return { worldMatrix_.m[1][0], worldMatrix_.m[1][1], worldMatrix_.m[1][2] };
}

// マウス視点制御機能
void Camera::ToggleMouseLook() {
#ifdef _DEBUG
    mouseLookEnabled_ = !mouseLookEnabled_;
    UpdateMouseControl();
#endif
}

bool Camera::IsMouseLookEnabled() const {
    return mouseLookEnabled_;
}

void Camera::SetMouseLookEnabled(bool enabled) {
    mouseLookEnabled_ = enabled;
    UpdateMouseControl();
}

void Camera::UpdateMouseControl() {
    if (!windowHandle_) {
        return;
    }

    if (mouseLookEnabled_) {
        // マウス視点移動ON時
        // マウスカーソルを非表示
        ShowCursor(FALSE);
        
        // ウィンドウの中央座標を取得
        GetClientRect(windowHandle_, &windowRect_);
        POINT centerPoint = {
            windowRect_.right / 2,
            windowRect_.bottom / 2
        };
        
        // クライアント座標をスクリーン座標に変換
        ClientToScreen(windowHandle_, &centerPoint);
        
        // マウスを中央に固定
        SetCursorPos(centerPoint.x, centerPoint.y);
        lastMousePos_ = centerPoint;
        
        // マウスをウィンドウ内にクリップ
        RECT clipRect = windowRect_;
        ClientToScreen(windowHandle_, reinterpret_cast<POINT*>(&clipRect.left));
        ClientToScreen(windowHandle_, reinterpret_cast<POINT*>(&clipRect.right));
        ClipCursor(&clipRect);
        
    } else {
        // マウス視点移動OFF時
        // マウスカーソルを表示
        ShowCursor(TRUE);
        
        // マウスクリップを解除
        ClipCursor(nullptr);
    }
}

void Camera::SetWindowHandle(HWND hwnd) {
    windowHandle_ = hwnd;
    if (windowHandle_) {
        UpdateMouseControl();
    }
}

// デフォルトカメラの設定・取得
void Object3dCommon::SetDefaultCamera(Camera* camera) {
    defaultCamera_ = camera;
}

Camera* Object3dCommon::GetDefaultCamera() {
    return defaultCamera_;
}

// 右スティック視点移動関連
void Camera::ProcessRightStickInput(float deltaX, float deltaY) {
#ifdef _DEBUG
    // フリーカメラモードの時は従来のカメラ回転
    if (cameraMode_ == CAMERA_MODE_FREE) {
        transform_.rotate.y += deltaX * stickSensitivity_ * 0.01f;  // 水平回転
        transform_.rotate.x += deltaY * stickSensitivity_ * 0.01f;  // 垂直回転
        
        // X軸回転を制限
        if (transform_.rotate.x > 1.57f) transform_.rotate.x = 1.57f;
        if (transform_.rotate.x < -1.57f) transform_.rotate.x = -1.57f;
        
        return;
    }
#endif
    
    // オービット角度を変更
    orbitAngleY_ += deltaX * stickSensitivity_ * 0.01f;  // 水平回転
    orbitAngleX_ += deltaY * stickSensitivity_ * 0.01f;  // 垂直回転
    
    // X軸回転を適切な範囲に制限（上下の回転制限）
    // 上限: +45度（0.785ラジアン）、下限: -30度（-0.52ラジアン）
    if (orbitAngleX_ > 0.785f) {
        orbitAngleX_ = 0.785f;
    }
    if (orbitAngleX_ < -0.52f) {
        orbitAngleX_ = -0.52f;
    }
    
    // Y軸回転を-π～πの範囲に正規化
    while (orbitAngleY_ > 3.14159f) {
        orbitAngleY_ -= 2.0f * 3.14159f;
    }
    while (orbitAngleY_ < -3.14159f) {
        orbitAngleY_ += 2.0f * 3.14159f;
    }
}

void Camera::SetStickSensitivity(float sensitivity) {
    stickSensitivity_ = sensitivity;
}

float Camera::GetStickSensitivity() const {
    return stickSensitivity_;
}

// オービットカメラ機能
void Camera::SetOrbitTarget(const Vector3& target) {
    orbitTarget_ = target;
}

void Camera::SetOrbitDistance(float distance) {
    orbitDistance_ = distance;
}

void Camera::SetOrbitHeight(float height) {
    orbitHeight_ = height;
}

void Camera::UpdateOrbitCamera() {
    // オービット角度に基づいてカメラの位置を計算
    float cosY = cosf(orbitAngleY_);
    float sinY = sinf(orbitAngleY_);
    float cosX = cosf(orbitAngleX_);
    float sinX = sinf(orbitAngleX_);

    // 簡素化されたオービット座標系計算（安定した距離と高さ）
    Vector3 offset;
    offset.x = orbitDistance_ * sinY * cosX;
    offset.y = orbitDistance_ * sinX;
    offset.z = orbitDistance_ * cosY * cosX;

    // ターゲット位置の高さを考慮してカメラ位置を決定
    Vector3 targetWithHeight = orbitTarget_;
    targetWithHeight.y += orbitHeight_;

    // 最終的なカメラ位置
    Vector3 newPosition = targetWithHeight + offset;

    // カメラの位置を設定
    transform_.translate = newPosition;

    // カメラの向きをターゲット（プレイヤー）を向くように設定
    Vector3 lookAtTarget = orbitTarget_;
    lookAtTarget.y += 1.0f;  // プレイヤーの少し上を見る

    Vector3 direction = lookAtTarget - newPosition;
    float length = sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (length > 0.0001f) {
        direction.x /= length;
        direction.y /= length;
        direction.z /= length;

        // Y方向の制限（真下を向かないようにする）
        if (direction.y < -0.8f) {
            direction.y = -0.8f;
            // 正規化し直す
            float newLength = sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
            if (newLength > 0.0001f) {
                direction.x /= newLength;
                direction.y /= newLength;
                direction.z /= newLength;
            }
        }

        // 方向ベクトルから回転角度を計算
        transform_.rotate.y = atan2f(direction.x, direction.z);
        transform_.rotate.x = asinf(-direction.y);
        transform_.rotate.z = 0.0f;
    }
}

// フラスタムカリング関連
const Frustum& Camera::GetFrustum() const {
    return frustum_;
}

bool Camera::IsPointInFrustum(const Vector3& point) const {
    return frustum_.IsPointInside(point);
}

bool Camera::IsAABBInFrustum(const Vector3& min, const Vector3& max) const {
    return frustum_.IsAABBInside(min, max);
}

bool Camera::IsSphereInFrustum(const Vector3& center, float radius) const {
    return frustum_.IsSphereInside(center, radius);
}