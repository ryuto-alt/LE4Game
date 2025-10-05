// Horror Overlay Effect for Sprite
// シンプルなビネット+グレイスケール+ノイズエフェクト

struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

struct Material {
    float32_t4 color;
    int32_t enableLighting;
    float32_t3 padding;
    float32_t4x4 uvTransform;
};

ConstantBuffer<Material> gMaterial : register(b0);
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

// 擬似乱数生成
float rand(float32_t2 co)
{
    return frac(sin(dot(co.xy, float32_t2(12.9898, 78.233))) * 43758.5453);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    // グレイスケール変換
    float gray = dot(textureColor.rgb, float32_t3(0.299, 0.587, 0.114));
    float32_t3 grayColor = float32_t3(gray, gray, gray);

    // ビネット効果
    float32_t2 center = float32_t2(0.5, 0.5);
    float dist = distance(input.texcoord, center);
    float vignette = 1.0 - smoothstep(0.3, 1.2, dist);
    vignette = pow(vignette, 1.5);

    // ノイズ効果
    float noise = rand(input.texcoord * 100.0);
    noise = (noise - 0.5) * 0.1;

    // 最終カラー
    float32_t3 finalColor = grayColor * vignette + noise;
    finalColor = finalColor * 0.7; // 全体を暗くする

    output.color = float32_t4(finalColor, textureColor.a) * gMaterial.color;

    return output;
}
