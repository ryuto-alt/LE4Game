#pragma once
#include "DirectXCommon.h"

class SpriteCommon
{

public:
	// コンストラクタ
	SpriteCommon() = default;
	// デストラクタ
	~SpriteCommon();

	// 初期化
	void Initialize(DirectXCommon* dxCommon);

	// 共通描画設定
	void CommonDraw();

	DirectXCommon* GetDxCommon()const { return dxCommon_; }


	// ルートシグネチャを取得
	Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature() const { return rootSignature; }
	
	// 通常のパイプラインを取得
	Microsoft::WRL::ComPtr<ID3D12PipelineState> GetGraphicsPipelineState() const { return graphicsPipelineState; }
	
	// スキニング用パイプラインを取得
	Microsoft::WRL::ComPtr<ID3D12PipelineState> GetSkinningPipelineState() const { return skinningPipelineState; }
	
	// PBRパイプラインを取得
	Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPBRPipelineState() const { return pbrPipelineState; }
	
	// PBRスキニングパイプラインを取得
	Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPBRSkinningPipelineState() const { return pbrSkinningPipelineState; }

private:
	// ルートシグネチャの作成
	void RootSignatureInitialize();

	// グラフィックスパイプライン
	void GraphicsPipelineInitialize();
	
	// スキニング用パイプライン
	void SkinningPipelineInitialize();
	
	// PBR用パイプライン
	void PBRPipelineInitialize();
	
	// PBRスキニング用パイプライン
	void PBRSkinningPipelineInitialize();

private:
	DirectXCommon* dxCommon_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> skinningPipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pbrPipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pbrSkinningPipelineState = nullptr;
};