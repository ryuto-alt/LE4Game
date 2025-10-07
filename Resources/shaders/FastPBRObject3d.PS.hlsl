// 超高速PBRピクセルシェーダー（最適化版）
struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t3 worldPosition : POSITION0;
};

struct PBRMaterial
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

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float32_t intensity;
    float32_t3 ambientColor;
    float32_t ambientIntensity;
};

struct CameraData
{
    float32_t3 worldPosition;
    float32_t fisheyeStrength;
};

ConstantBuffer<PBRMaterial> gPBRMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<CameraData> gCameraData : register(b3);

Texture2D<float32_t4> gBaseColorTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // UV座標
    float32_t2 uv = input.texcoord;

    // Base Color
    float32_t4 baseColor = gPBRMaterial.baseColorFactor;

    if (gPBRMaterial.hasBaseColorTexture)
    {
        baseColor *= gBaseColorTexture.Sample(gSampler, uv);
    }

    // Alpha Test
    if (gPBRMaterial.alphaMode == 1 && baseColor.a < gPBRMaterial.alphaCutoff)
    {
        discard;
    }

    // ライティング無効時は即座に返す
    if (!gPBRMaterial.enableLighting)
    {
        output.color = baseColor;
        return output;
    }

    // 簡略化されたライティング計算
    float32_t3 normal = normalize(input.normal);
    float32_t3 L = normalize(-gDirectionalLight.direction);

    // シンプルなLambert拡散
    float32_t NdotL = max(dot(normal, L), 0.0);

    // アンビエント + 拡散反射
    float32_t3 ambient = gDirectionalLight.ambientColor * gDirectionalLight.ambientIntensity;
    float32_t3 diffuse = gDirectionalLight.color.rgb * gDirectionalLight.intensity * NdotL;

    // 簡易スペキュラ（metallicの影響のみ）
    float32_t metallic = gPBRMaterial.metallicFactor;
    float32_t3 specular = diffuse * metallic * 0.5;

    float32_t3 color = baseColor.rgb * (ambient + diffuse + specular);

    // エミッシブ
    color += gPBRMaterial.emissiveFactor;

    output.color = float32_t4(color, baseColor.a);
    return output;
}
