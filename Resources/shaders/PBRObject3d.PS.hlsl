// PBRオブジェクト3Dピクセルシェーダー
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

    // パディングとして追加のフラグを配置
    int32_t enableEnvironmentMap; // 環境マップの有効/無効
    float32_t environmentMapIntensity; // 環境マップの反射強度 (0.0～1.0)
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

struct SpotLight
{
    float32_t4 color;
    float32_t3 position;
    float32_t intensity;
    float32_t3 direction;
    float32_t innerCone;
    float32_t3 attenuation;
    float32_t outerCone;
};

struct CameraData
{
    float32_t3 worldPosition;
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

// ガンマ補正
float32_t3 GammaToLinear(float32_t3 color)
{
    return pow(color, 2.2);
}

float32_t3 LinearToGamma(float32_t3 color)
{
    return pow(color, 1.0 / 2.2);
}

// Constant Buffers
ConstantBuffer<PBRMaterial> gPBRMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<SpotLight> gSpotLight : register(b2);
ConstantBuffer<CameraData> gCameraData : register(b3);

// Textures
Texture2D<float32_t4> gBaseColorTexture : register(t0);
TextureCube<float32_t4> gEnvironmentMap : register(t2);

// Samplers
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // UV座標の変換
    float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gPBRMaterial.uvTransform);
    float32_t2 uv = transformedUV.xy;
    
    // Base Color の取得
    float32_t4 baseColor = gPBRMaterial.baseColorFactor;
    
    // デバッグ: baseColorFactorが正しく設定されているか確認
    // 一時的にベースカラーを固定値に変更してテスト
    // baseColor = float32_t4(1.0f, 0.0f, 0.0f, 1.0f); // 赤色でテスト
    
    if (gPBRMaterial.hasBaseColorTexture)
    {
        float32_t4 baseColorTexture = gBaseColorTexture.Sample(gSampler, uv);
        // テクスチャはsRGB形式で読み込まれているため、ガンマ補正は不要
        baseColor *= baseColorTexture;
    }
    
    // Alpha Test
    if (gPBRMaterial.alphaMode == 1) // MASK
    {
        if (baseColor.a < gPBRMaterial.alphaCutoff)
        {
            discard;
        }
    }
    
    // Metallic & Roughness の取得（マテリアル係数のみ使用）
    float32_t metallic = gPBRMaterial.metallicFactor;
    float32_t roughness = gPBRMaterial.roughnessFactor;
    
    // 法線の取得（法線マップなし）
    float32_t3 normal = normalize(input.normal);
    
    // Occlusion の取得（係数のみ使用）
    float32_t occlusion = gPBRMaterial.occlusionStrength;
    
    // Emissive の取得（係数のみ使用）
    float32_t3 emissive = gPBRMaterial.emissiveFactor;

    // ライティングが無効の場合はベースカラーをそのまま出力
    if (!gPBRMaterial.enableLighting)
    {
        output.color = float32_t4(LinearToGamma(baseColor.rgb + emissive), baseColor.a);
        return output;
    }
    
    // PBR計算用の値準備
    float32_t3 albedo = baseColor.rgb;
    float32_t3 V = normalize(gCameraData.worldPosition - input.worldPosition);
    float32_t3 R = reflect(-V, normal);
    
    // F0の計算（金属の場合はalbedo、非金属の場合は0.04）
    float32_t3 F0 = lerp(float32_t3(0.04, 0.04, 0.04), albedo, metallic);
    
    // 最終的な色の初期化
    float32_t3 color = float32_t3(0.0, 0.0, 0.0);

    // ============== アンビエントライト（常に適用） ==============
    // ディレクショナルライトのアンビエント成分を使用
    float32_t3 ambient = gDirectionalLight.ambientColor * gDirectionalLight.ambientIntensity * albedo;
    color += ambient;

    // ============== ディレクショナルライト計算 ==============
    {
        float32_t3 L = normalize(-gDirectionalLight.direction);
        float32_t3 H = normalize(V + L);
        
        // Cook-Torrance BRDF計算
        float32_t NDF = DistributionGGX(normal, H, roughness);
        float32_t G = GeometrySmith(normal, V, L, roughness);
        float32_t3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
        
        float32_t3 numerator = NDF * G * F;
        float32_t denominator = 4.0 * max(dot(normal, V), 0.0) * max(dot(normal, L), 0.0) + 0.0001;
        float32_t3 specular = numerator / denominator;
        
        // エネルギー保存則
        float32_t3 kS = F;
        float32_t3 kD = float32_t3(1.0, 1.0, 1.0) - kS;
        kD *= 1.0 - metallic;
        
        // Lambert拡散
        float32_t3 diffuse = kD * albedo / 3.14159265359;
        
        // ディレクショナルライトの寄与
        float32_t NdotL = max(dot(normal, L), 0.0);
        float32_t3 radiance = gDirectionalLight.color.rgb * gDirectionalLight.intensity;
        color += (diffuse + specular) * radiance * NdotL;
    }
    
    // ============== スポットライト計算 ==============
    {
        float32_t3 L = normalize(gSpotLight.position - input.worldPosition);
        float32_t3 H = normalize(V + L);

        // スポットライトの減衰計算
        float32_t distance = length(gSpotLight.position - input.worldPosition);
        float32_t attenuation = 1.0 / (gSpotLight.attenuation.x + gSpotLight.attenuation.y * distance + gSpotLight.attenuation.z * distance * distance);

        // スポットライトのコーン計算
        float32_t theta = dot(L, normalize(-gSpotLight.direction));

        // 距離に応じた角度の広がり効果
        // 遠くなるほどエッジのぼかしが大きくなり、光が広がって見える
        float32_t distanceSpreadFactor = distance * 0.02; // 距離に応じた広がり係数
        float32_t dynamicEpsilon = (gSpotLight.innerCone - gSpotLight.outerCone) * (1.0 + distanceSpreadFactor);

        float32_t intensity = clamp((theta - gSpotLight.outerCone) / dynamicEpsilon, 0.0, 1.0);
        
        // Cook-Torrance BRDF計算
        float32_t NDF = DistributionGGX(normal, H, roughness);
        float32_t G = GeometrySmith(normal, V, L, roughness);
        float32_t3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
        
        float32_t3 numerator = NDF * G * F;
        float32_t denominator = 4.0 * max(dot(normal, V), 0.0) * max(dot(normal, L), 0.0) + 0.0001;
        float32_t3 specular = numerator / denominator;
        
        // エネルギー保存則
        float32_t3 kS = F;
        float32_t3 kD = float32_t3(1.0, 1.0, 1.0) - kS;
        kD *= 1.0 - metallic;
        
        // Lambert拡散
        float32_t3 diffuse = kD * albedo / 3.14159265359;
        
        // スポットライトの寄与
        float32_t NdotL = max(dot(normal, L), 0.0);
        float32_t3 radiance = gSpotLight.color.rgb * gSpotLight.intensity * attenuation * intensity;
        color += (diffuse + specular) * radiance * NdotL;
    }
    
    // ============== 環境マップ反射 ==============
    if (gPBRMaterial.enableEnvironmentMap != 0)
    {
        // 環境マップからの反射
        float32_t3 envReflection = gEnvironmentMap.Sample(gSampler, R).rgb;

        // フレネル係数を使用して環境反射を計算
        float32_t3 F = FresnelSchlick(max(dot(normal, V), 0.0), F0);

        // 環境反射の寄与（強度パラメータで調整可能）
        float32_t3 envContribution = envReflection * F;
        color += envContribution * gPBRMaterial.environmentMapIntensity * 0.010;
    }

    // ============== 環境光（IBL簡易実装） ==============
    if (gPBRMaterial.enableEnvironmentMap != 0)
    {
        // 環境マップからの簡易環境光
        float32_t3 envAmbient = gEnvironmentMap.Sample(gSampler, normal).rgb;
        float32_t3 ambient = envAmbient * albedo * occlusion * gPBRMaterial.environmentMapIntensity * 0.02;
        color += ambient;
    }
    
    // エミッシブ追加
    color += emissive;
    
    // HDRトーンマッピング（Reinhard）
    color = color / (color + float32_t3(1.0, 1.0, 1.0));
    
    // ガンマ補正
    color = LinearToGamma(color);
    
    output.color = float32_t4(color, baseColor.a);
    return output;
}