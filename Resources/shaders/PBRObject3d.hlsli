// PBRオブジェクト3D共通ヘッダー
struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t3 worldPosition : POSITION0;
    float32_t3 tangent : TANGENT0;
    float32_t3 bitangent : BITANGENT0;
};

// PBRマテリアル構造体
struct PBRMaterial
{
    // Base Color
    float32_t4 baseColorFactor;
    
    // Metallic & Roughness
    float32_t metallicFactor;
    float32_t roughnessFactor;
    float32_t normalScale;
    float32_t occlusionStrength;
    
    // Emissive
    float32_t3 emissiveFactor;
    float32_t alphaCutoff;
    
    // Flags
    int32_t hasBaseColorTexture;
    int32_t hasMetallicRoughnessTexture;
    int32_t hasNormalTexture;
    int32_t hasOcclusionTexture;
    int32_t hasEmissiveTexture;
    int32_t enableLighting;
    int32_t alphaMode; // 0=OPAQUE, 1=MASK, 2=BLEND
    int32_t doubleSided;
    
    // UV変換
    float32_t4x4 uvTransform;
    
    // パディング
    float32_t padding[2];
};

// 光源定義
struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float32_t intensity;
    float32_t3 ambientColor;    // アンビエントライトの色
    float32_t ambientIntensity; // アンビエントライトの強度
};

// PBR計算のヘルパー関数
float32_t3 FresnelSchlick(float32_t cosTheta, float32_t3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float32_t DistributionGGX(float32_t3 N, float32_t3 H, float32_t roughness)
{
    float32_t a = roughness * roughness;
    float32_t a2 = a * a;
    float32_t NdotH = max(dot(N, H), 0.0);
    float32_t NdotH2 = NdotH * NdotH;
    
    float32_t num = a2;
    float32_t denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = 3.14159265359 * denom * denom;
    
    return num / denom;
}

float32_t GeometrySchlickGGX(float32_t NdotV, float32_t roughness)
{
    float32_t r = (roughness + 1.0);
    float32_t k = (r * r) / 8.0;
    
    float32_t num = NdotV;
    float32_t denom = NdotV * (1.0 - k) + k;
    
    return num / denom;
}

float32_t GeometrySmith(float32_t3 N, float32_t3 V, float32_t3 L, float32_t roughness)
{
    float32_t NdotV = max(dot(N, V), 0.0);
    float32_t NdotL = max(dot(N, L), 0.0);
    float32_t ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float32_t ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

// 法線マップの適用
float32_t3 ApplyNormalMap(float32_t3 normalMap, float32_t3 normal, float32_t3 tangent, float32_t3 bitangent, float32_t scale)
{
    // 法線マップのデコード
    float32_t3 normalMapDecoded = normalMap * 2.0 - 1.0;
    normalMapDecoded.xy *= scale;
    
    // タンジェント空間からワールド空間への変換
    float32_t3x3 TBN = float32_t3x3(normalize(tangent), normalize(bitangent), normalize(normal));
    return normalize(mul(normalMapDecoded, TBN));
}

// ガンマ補正
float32_t3 GammaToLinear(float32_t3 color)
{
    return pow(color, 2.2);
}

float32_t3 LinearToGamma(float32_t3 color)
{
    return pow(color, 1.0 / 2.2);
}