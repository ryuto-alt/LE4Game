#pragma once

#include <unordered_map>
#include <string>
#include <list>
#include <random>
#include <memory>
#include "DirectXCommon.h"
#include "SRVManager.h"
#include "Mymath.h"
#include "Mymath.h"
#include "Camera.h"

// 前方宣言
class ParticleEmitter;

// パーティクル1粒の情報
struct Particle {
    // 座標
    Vector3 position;
    // 速度
    Vector3 velocity;
    // 加速度
    Vector3 accel;
    // 色
    Vector4 color;
    // 初期サイズ
    float startSize;
    // 最終サイズ
    float endSize;
    // 現在サイズ
    float size;
    // 初期色
    Vector4 startColor;
    // 最終色
    Vector4 endColor;
    // 回転
    float rotation;
    // 回転速度
    float rotationVelocity;
    // 経過時間
    float lifeTime;
    // 寿命
    float lifeTimeMax;

    // 生存フラグ
    bool isDead = false;
};

// インスタンシング描画用データ
struct ParticleForGPU {
    // WVP行列
    Matrix4x4 WVP;
    // World行列
    Matrix4x4 World;
    // 色
    Vector4 color;
};

// パーティクルグループ（テクスチャごとにグループ化）
struct ParticleGroup {
    // マテリアルデータ（テクスチャファイルパスとテクスチャのSRVインデックス）
    std::string textureFilePath;
    uint32_t textureSrvIndex;

    // パーティクルのリスト
    std::list<Particle> particles;

    // インスタンシングデータのSRVインデックス
    uint32_t instanceSrvIndex;

    // インスタンシングリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> instanceResource;

    // インスタンス数
    uint32_t instanceCount;

    // インスタンシングデータを書き込むためのポインタ
    ParticleForGPU* instanceData;
};

// パーティクルマネージャクラス
class ParticleManager {
private:
    // DirectXCommon
    DirectXCommon* dxCommon_ = nullptr;

    // SRVマネージャ
    SrvManager* srvManager_ = nullptr;

    // 乱数生成器
    std::mt19937 randomEngine_;

    // パーティクルグループコンテナ
    std::unordered_map<std::string, ParticleGroup> particleGroups;

    // 描画用ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;

    // 描画用パイプラインステート
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;

    // 頂点バッファビュー
    D3D12_VERTEX_BUFFER_VIEW vbView{};

    // 頂点リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;

    // マテリアル用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
    Material* materialData = nullptr;

    // ディレクショナルライト用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource;
    DirectionalLight* directionalLightData = nullptr;

    // ビルボード行列
    Matrix4x4 billboardMatrix{};

    // コピー禁止
    ParticleManager(const ParticleManager&) = delete;
    ParticleManager& operator=(const ParticleManager&) = delete;

    // 頂点データ構造体
    struct VertexData {
        Vector3 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    // グラフィックスパイプラインの初期化
    void InitializeGraphicsPipeline();

    // ビルボード行列の計算
    void CalculateBillboardMatrix(const Camera* camera);

    // フレンドクラス
    friend class ParticleEmitter;

    // シングルトンインスタンス
    static ParticleManager* instance_;

    // コンストラクタ（シングルトン）
    ParticleManager() = default;
    // デストラクタ
    ~ParticleManager();

public:
    // シングルトンインスタンスの取得
    static ParticleManager* GetInstance();

    // 終了処理
    static void Finalize();

    // リソースの強制解放
    void ForceReleaseResources() {
        OutputDebugStringA("ParticleManager::ForceReleaseResources - Starting resource cleanup\n");
        
        // 全パーティクルグループのリソース解放
        for (auto& [name, group] : particleGroups) {
            OutputDebugStringA(("ParticleManager: Releasing particle group - " + name + "\n").c_str());
            if (group.instanceResource && group.instanceData) {
                group.instanceResource->Unmap(0, nullptr);
                group.instanceData = nullptr;
            }
            group.instanceResource.Reset();
        }
        particleGroups.clear();
        OutputDebugStringA("ParticleManager: All particle groups cleared\n");

        // その他のマップされたリソースの解放
        if (materialResource && materialData) {
            materialResource->Unmap(0, nullptr);
            materialData = nullptr;
            OutputDebugStringA("ParticleManager: Material resource unmapped\n");
        }
        materialResource.Reset();
        OutputDebugStringA("ParticleManager: Material resource reset\n");
        
        if (directionalLightResource && directionalLightData) {
            directionalLightResource->Unmap(0, nullptr);
            directionalLightData = nullptr;
            OutputDebugStringA("ParticleManager: DirectionalLight resource unmapped\n");
        }
        directionalLightResource.Reset();
        OutputDebugStringA("ParticleManager: DirectionalLight resource reset\n");

        if (vertexResource) {
            vertexResource.Reset();
            OutputDebugStringA("ParticleManager: Vertex resource reset\n");
        }

        // パイプラインステートとルートシグネチャの解放
        if (pipelineState) {
            pipelineState.Reset();
            OutputDebugStringA("ParticleManager: Pipeline state reset\n");
        }
        if (rootSignature) {
            rootSignature.Reset();
            OutputDebugStringA("ParticleManager: Root signature reset\n");
        }
        
        OutputDebugStringA("ParticleManager::ForceReleaseResources - Resource cleanup completed\n");
    }

    // 初期化
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    // 更新
    void Update(const Camera* camera);

    // 描画
    void Draw();

    // パーティクルグループの作成
    void CreateParticleGroup(const std::string& name, const std::string& textureFilePath);

    // パーティクルの発生（シンプル版）
    void Emit(const std::string& name, const Vector3& position, uint32_t count);

    // パーティクルの発生（詳細設定版）
    void Emit(
        const std::string& name,
        const Vector3& position,
        uint32_t count,
        const Vector3& velocityMin,
        const Vector3& velocityMax,
        const Vector3& accelMin,
        const Vector3& accelMax,
        float startSizeMin,
        float startSizeMax,
        float endSizeMin,
        float endSizeMax,
        const Vector4& startColorMin,
        const Vector4& startColorMax,
        const Vector4& endColorMin,
        const Vector4& endColorMax,
        float rotationMin,
        float rotationMax,
        float rotationVelocityMin,
        float rotationVelocityMax,
        float lifeTimeMin,
        float lifeTimeMax);

    // デバッグ用：パーティクル数の取得
    uint32_t GetParticleCount(const std::string& name) {
        auto it = particleGroups.find(name);
        if (it != particleGroups.end()) {
            return static_cast<uint32_t>(it->second.particles.size());
        }
        return 0;
    }

    // デバッグ用：シンプルな四角形を描画
    void DrawSimpleQuad();
};