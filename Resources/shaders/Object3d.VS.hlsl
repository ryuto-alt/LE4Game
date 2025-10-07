#include"object3d.hlsli"

struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct CameraData
{
    float32_t3 worldPosition;
    float32_t fisheyeStrength;
};
ConstantBuffer<CameraData> gCameraData : register(b3);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;

    // ワールド座標を計算
    float32_t4 worldPos = mul(input.position, gTransformationMatrix.World);
    output.worldPos = worldPos.xyz;

    // 法線の計算を精密に行い、正規化を確実に行う
    float32_t3 worldNormal = mul(input.normal, (float32_t3x3) gTransformationMatrix.World);
    output.normal = normalize(worldNormal);

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

    return output;
}