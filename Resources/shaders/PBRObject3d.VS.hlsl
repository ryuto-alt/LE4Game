// PBRオブジェクト3D頂点シェーダー
// インクルードエラーを回避するため直接定義

struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t3 worldPosition : POSITION0;
    float32_t3 tangent : TANGENT0;
    float32_t3 bitangent : BITANGENT0;
};

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

    // 位置の変換
    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.worldPosition = mul(input.position, gTransformationMatrix.World).xyz;

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

    // テクスチャ座標
    output.texcoord = input.texcoord;

    // 法線の変換
    output.normal = normalize(mul(input.normal, (float32_t3x3)gTransformationMatrix.World));

    // タンジェントとバイタンジェントは後で対応
    output.tangent = float32_t3(1.0, 0.0, 0.0);
    output.bitangent = float32_t3(0.0, 1.0, 0.0);

    return output;
}