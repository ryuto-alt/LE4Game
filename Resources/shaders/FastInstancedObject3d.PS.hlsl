// 超軽量インスタンシング用ピクセルシェーダー
// PBR計算を排除し、シンプルなランバート拡散反射のみ

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

// 簡易マテリアル
struct FastMaterial
{
    float32_t4 baseColorFactor;
    float32_t metallicFactor;
    float32_t roughnessFactor;
    float32_t normalScale;
    float32_t occlusionStrength;
    float32_t3 emissiveFactor;
    float32_t alphaCutoff;
    int32_t hasBaseColorTexture;
    int32_t hasMetallicRoughnessTexture;
    int32_t hasNormalTexture;
    int32_t hasOcclusionTexture;
    int32_t hasEmissiveTexture;
    int32_t enableLighting;
    int32_t alphaMode;
    int32_t doubleSided;
    float32_t4x4 uvTransform;
    int32_t enableEnvironmentMap;
    float32_t environmentMapIntensity;
};

// ディレクショナルライト
struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float32_t intensity;
    float32_t3 ambientColor;
    float32_t ambientIntensity;
};

ConstantBuffer<FastMaterial> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b3);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // ベースカラー
    float32_t4 baseColor = gMaterial.baseColorFactor * input.color;

    // テクスチャサンプリング
    if (gMaterial.hasBaseColorTexture)
    {
        float32_t4 texColor = gTexture.Sample(gSampler, input.texcoord);
        baseColor *= texColor;
    }

    // アルファテスト
    if (gMaterial.alphaMode == 1 && baseColor.a < gMaterial.alphaCutoff)
    {
        discard;
    }

    // ライティング計算（超シンプル）
    if (gMaterial.enableLighting)
    {
        float32_t3 normal = normalize(input.normal);
        float32_t3 lightDir = normalize(-gDirectionalLight.direction);

        // ランバート拡散反射（NdotLのみ）
        float32_t NdotL = max(dot(normal, lightDir), 0.0);

        // アンビエント + 拡散反射
        float32_t3 ambient = gDirectionalLight.ambientColor * gDirectionalLight.ambientIntensity;
        float32_t3 diffuse = gDirectionalLight.color.rgb * gDirectionalLight.intensity * NdotL;

        float32_t3 lighting = ambient + diffuse;
        output.color.rgb = baseColor.rgb * lighting;
    }
    else
    {
        output.color.rgb = baseColor.rgb;
    }

    output.color.a = baseColor.a;

    return output;
}
