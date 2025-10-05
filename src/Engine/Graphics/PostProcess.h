#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <string>

class DirectXCommon;
class SrvManager;

class PostProcess {
public:
    struct HorrorParams {
        float time;
        float noiseIntensity;
        float distortionAmount;
        float bloodAmount;
        float vignetteIntensity;
        float padding[3];  // 16バイト境界に合わせる
    };

    PostProcess() = default;
    ~PostProcess();

    void Finalize();

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void PreDraw();
    void PostDraw();

    void SetHorrorParams(float time, float noise, float distortion, float blood, float vignette = 0.0f);

    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const { return rtvHandle_; }
    ID3D12Resource* GetRenderTarget() const { return renderTargetResource_.Get(); }

private:
    void CreateRenderTarget();
    void CreatePipeline();

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    // Render Target
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTargetResource_;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};
    D3D12_GPU_DESCRIPTOR_HANDLE srvGPUHandle_{};
    uint32_t srvIndex_ = 0;
    bool srvAllocated_ = false;

    // RTV用のDescriptorHeap
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;

    // Pipeline
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    // Constant Buffer
    Microsoft::WRL::ComPtr<ID3D12Resource> horrorParamsResource_;
    HorrorParams* horrorParamsData_ = nullptr;

    HorrorParams currentParams_{};

    uint32_t backBufferIndex_ = 0;
};
