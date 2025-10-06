// PBRインスタンシングオブジェクト3D頂点シェーダー

struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t3 worldPosition : POSITION0;
    float32_t3 tangent : TANGENT0;
    float32_t3 bitangent : BITANGENT0;
    float32_t4 color : COLOR0;
};

struct ViewProjectionMatrix
{
    float32_t4x4 viewProjection;
};
ConstantBuffer<ViewProjectionMatrix> gViewProjectionMatrix : register(b0);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;

    // インスタンスデータ
    float32_t4x4 world : WORLD;
    float32_t4 color : COLOR;
    uint instanceId : SV_InstanceID;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    // ワールド座標の計算
    float32_t4 worldPos = mul(input.position, input.world);
    output.worldPosition = worldPos.xyz;

    // スクリーン座標の計算
    output.position = mul(worldPos, gViewProjectionMatrix.viewProjection);

    // テクスチャ座標
    output.texcoord = input.texcoord;

    // 法線の変換
    output.normal = normalize(mul(input.normal, (float32_t3x3)input.world));

    // タンジェントとバイタンジェント（簡易版）
    output.tangent = float32_t3(1.0, 0.0, 0.0);
    output.bitangent = float32_t3(0.0, 1.0, 0.0);

    // インスタンスカラー
    output.color = input.color;

    return output;
}
