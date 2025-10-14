#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>

class DirectXCommon;
class SrvManager;
class AudioManager;

// トランジションエフェクト用パラメータ構造体
struct TransitionParams {
    float progress;        // トランジション進行度 (0.0 - 1.0)
    float aspectRatio;     // アスペクト比
    float padding[2];      // パディング
};

// トランジションエフェクトクラス
class TransitionEffect {
public:
    TransitionEffect() = default;
    ~TransitionEffect();

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void Finalize();

    // トランジション描画の開始（レンダーターゲットに描画）
    void PreDraw();

    // トランジション描画の終了（バックバッファに適用）
    void PostDraw();

    // トランジション進行度を設定
    void SetProgress(float progress);

    // トランジション開始（ノイズ音の再生も開始）
    void StartTransition();

    // トランジション停止（ノイズ音の停止も実行）
    void StopTransition();

    // ノイズ音の音量を設定
    void SetNoiseVolume(float volume);

    // レンダーターゲットのリサイズ
    void ResizeRenderTarget();

    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const { return rtvHandle_; }
    Microsoft::WRL::ComPtr<ID3D12Resource> GetRenderTarget() const { return renderTargetResource_; }

private:
    void CreateRenderTarget();
    void CreatePipeline();

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    // レンダーターゲット
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTargetResource_;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};
    D3D12_GPU_DESCRIPTOR_HANDLE srvGPUHandle_{};
    UINT srvIndex_ = 0;
    bool srvAllocated_ = false;

    // RTV用DescriptorHeap
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;

    // パイプライン
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    // トランジションパラメータ
    Microsoft::WRL::ComPtr<ID3D12Resource> transitionParamsResource_;
    TransitionParams* transitionParamsData_ = nullptr;
    TransitionParams currentParams_{};

    // オーディオ関連
    AudioManager* audioManager_ = nullptr;
    std::string noiseAudioName_ = "transitionNoise";
    bool isTransitioning_ = false;
};
