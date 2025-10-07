#include "Skybox.hlsli"

struct VertexShaderInput {
    float4 position : POSITION0;
};

struct TransformationMatrix {
    float4x4 WVP;
    float4x4 World;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct CameraData
{
    float3 worldPosition;
    float fisheyeStrength;
};
ConstantBuffer<CameraData> gCameraData : register(b3);

VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;

    // WVP行列で変換
    output.position = mul(input.position, gTransformationMatrix.WVP);

    // 魚眼レンズ効果の適用（画面端での伸びを抑制）
    if (gCameraData.fisheyeStrength > 0.0)
    {
        // NDC座標に変換
        float2 ndc = output.position.xy / output.position.w;

        // 中心からの距離を計算
        float r = length(ndc);

        // 魚眼歪みを適用（より穏やかな曲線）
        // tanhを使用して端での極端な伸びを抑制
        float distortion = tanh(r * gCameraData.fisheyeStrength * 0.5) / (r + 0.0001);

        // 歪みを適用
        output.position.xy = ndc * distortion * output.position.w;
    }

    // Skybox特有の処理：zとwを同じ値にして最遠方に配置
    output.position = output.position.xyww;

    // texcoordは頂点位置をそのまま使用（3次元ベクトル）
    output.texcoord = input.position.xyz;

    return output;
}