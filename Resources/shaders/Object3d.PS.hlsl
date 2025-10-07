#include "object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    int32_t enableEnvironmentMap;
    float32_t4x4 uvTransform;
};

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};

struct SpotLight
{
    float32_t4 color;       // ライトの色
    float32_t3 position;    // スポットライトの位置
    float intensity;        // ライトの強度
    float32_t3 direction;   // スポットライトの方向
    float innerCone;        // 内側コーン角度（cos値）
    float32_t3 attenuation; // 減衰パラメータ（定数、線形、二次）
    float outerCone;        // 外側コーン角度（cos値）
};

struct CameraData
{
    float32_t3 worldPosition;
    float32_t fisheyeStrength;
};

ConstantBuffer<Material> gMaterial : register(b0);
Texture2D<float32_t4> gTexture : register(t0);
TextureCube<float32_t4> gEnvironmentTexture : register(t2);
SamplerState gSample : register(s0);

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<SpotLight> gSpotLight : register(b2);
ConstantBuffer<CameraData> gCamera : register(b3);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSample, transformedUV.xy);
    
    // アルファ値が閾値未満の場合、そのピクセルを描画しない
    if (textureColor.a < 0.1f)
    {
        discard; // このピクセルの処理を中止
    }
    
    PixelShaderOutput output;
    
    // 正規化された法線を使用（念のため再度正規化）
    float3 normal = normalize(input.normal);
    
    if (gMaterial.enableLighting != 0)
    {
        // 半球ライティングを採用
        float3 lightDir = normalize(-gDirectionalLight.direction);
        float NdotL = dot(normal, lightDir);
        
        // 滑らかなシェーディングのための改良されたライティング計算
        float diffuse = saturate(NdotL * 0.5f + 0.5f); // 半球ライティング
        
        // リムライトを追加（輪郭を強調）
        float3 viewDir = float3(0.0f, 0.0f, -1.0f); // カメラ方向（適宜調整）
        float rim = 1.0f - saturate(dot(normal, viewDir));
        rim = pow(rim, 3.0f) * 0.3f; // リム効果を調整
        
        // ディレクショナルライトの計算
        float3 directionalLighting = gDirectionalLight.color.rgb * diffuse * gDirectionalLight.intensity;
        directionalLighting += rim * gDirectionalLight.color.rgb; // リムライトを追加
        
        // スポットライトの計算
        float3 spotLighting = float3(0.0f, 0.0f, 0.0f);
        
        // ピクセルからスポットライトへのベクトル
        float3 lightToPixel = input.worldPos - gSpotLight.position;
        float distance = length(lightToPixel);
        lightToPixel = normalize(lightToPixel);
        
        // 減衰の計算
        float attenuation = 1.0f / (
            gSpotLight.attenuation.x +                    // 定数減衰
            gSpotLight.attenuation.y * distance +         // 線形減衰
            gSpotLight.attenuation.z * distance * distance // 二次減衰
        );
        
        // スポットライトの方向との角度をチェック
        float cosAngle = dot(lightToPixel, normalize(gSpotLight.direction));
        
        // コーン内にいるかチェック
        if (cosAngle > gSpotLight.outerCone)
        {
            // 内側と外側のコーンの間でフェードアウト
            float spotFactor = 1.0f;
            if (cosAngle < gSpotLight.innerCone)
            {
                spotFactor = smoothstep(gSpotLight.outerCone, gSpotLight.innerCone, cosAngle);
            }
            
            // スポットライトの拡散反射
            float spotDiffuse = saturate(dot(normal, -lightToPixel));
            
            // スポットライトの最終的な寄与
            spotLighting = gSpotLight.color.rgb * spotDiffuse * gSpotLight.intensity * attenuation * spotFactor;
        }
        
        // アンビエントライトを追加（暗すぎる問題を解決）
        float3 ambient = float3(0.15f, 0.15f, 0.15f); // 環境光を追加
        
        // 環境マップの計算（有効な場合のみ）
        float3 environmentLighting = float3(0.0f, 0.0f, 0.0f);
        if (gMaterial.enableEnvironmentMap != 0)
        {
            float3 cameraToPosition = normalize(input.worldPos - gCamera.worldPosition);
            float3 reflectedVector = reflect(cameraToPosition, normalize(input.normal));
            float4 environmentColor = gEnvironmentTexture.Sample(gSample, reflectedVector);
            environmentLighting = environmentColor.rgb * 0.3f; // 環境反射の強度を調整
        }
        
        // すべてのライティングを合成
        float3 totalLighting = directionalLighting + spotLighting + ambient + environmentLighting;
        
        output.color.rgb = gMaterial.color.rgb * textureColor.rgb * totalLighting;
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }
    
    return output;
}