// Horror Effect Pixel Shader
// Combines VHS static noise, chromatic aberration, screen distortion, and blood effect

struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
};

// Constant Buffer for animation parameters
cbuffer HorrorParams : register(b0)
{
    float32_t time;            // 経過時間
    float32_t noiseIntensity;  // ノイズの強度 (0.0 - 1.0)
    float32_t distortionAmount;// 歪みの強度 (0.0 - 1.0)
    float32_t bloodAmount;     // 血のエフェクトの強度 (0.0 - 1.0)
    float32_t vignetteIntensity; // ビネットの強度 (0.0 - 1.0)
    float32_t3 padding;        // パディング
};

// RenderTextureをサンプリングするためのテクスチャとサンプラー
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

// VHS風の垂直バー歪み
float verticalBar(float pos, float uvY, float offset)
{
    float range = 0.05;
    float edge0 = (pos - range);
    float edge1 = (pos + range);
    
    float x = smoothstep(edge0, pos, uvY) * offset;
    x -= smoothstep(pos, edge1, uvY) * offset;
    return x;
}

// 色収差効果
float32_t3 chromaticAberration(float32_t2 uv, float amount)
{
    float32_t2 center = float32_t2(0.5, 0.5);
    float32_t2 direction = uv - center;
    
    float32_t r = gTexture.Sample(gSampler, uv + direction * amount * 0.009).r;
    float32_t g = gTexture.Sample(gSampler, uv + direction * amount * 0.006).g;
    float32_t b = gTexture.Sample(gSampler, uv - direction * amount * 0.006).b;
    
    return float32_t3(r, g, b);
}

// ビネット効果（画面の四隅を暗くする - ブラウン管風）
float32_t3 vignetteEffect(float32_t2 uv, float32_t3 color, float intensity)
{
    // 画面の中心からの距離を計算
    float32_t2 center = float32_t2(0.5, 0.5);
    float dist = distance(uv, center);

    // ビネット効果（ブラウン管のような丸みを帯びた暗さ）
    float vignette = 1.0 - smoothstep(0.2, 0.85, dist);
    vignette = pow(vignette, 2.2);

    // ビネットを適用（暗くする）
    float32_t3 result = color * (1.0 - (1.0 - vignette) * intensity);

    return result;
}

// 血のエフェクト（画面の端に赤い染み）
float32_t3 bloodEffect(float32_t2 uv, float32_t3 color, float amount)
{
    // 画面の端からの距離を計算
    float32_t2 center = float32_t2(0.5, 0.5);
    float dist = distance(uv, center);

    // ビネット効果を強化して血のような赤い染みを作る
    float vignette = 1.0 - smoothstep(0.2, 0.8, dist);
    vignette = pow(vignette, 2.0);

    // 血の色（暗い赤）
    float32_t3 bloodColor = float32_t3(0.3, 0.0, 0.0);

    // 血のテクスチャ風のノイズを追加
    float bloodNoise = rand(uv * 10.0 + time * 0.1);
    bloodNoise = smoothstep(0.7, 0.9, bloodNoise);

    // 最終的な血のエフェクトを適用
    float32_t3 result = lerp(color, bloodColor, vignette * amount * (0.7 + bloodNoise * 0.3));

    return result;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float32_t2 uv = input.texcoord;
    
    // VHS風の歪み効果
    for (float i = 0.0; i < 0.71; i += 0.1313)
    {
        float d = fmod(time * i * 0.1, 1.7);
        float o = sin(1.0 - tan(time * 0.024 * i));
        o *= distortionAmount * 0.02;
        uv.x += verticalBar(d, uv.y, o);
    }
    
    // スキャンライン効果
    float scanline = sin(uv.y * 800.0) * 0.04;
    
    // ノイズ効果
    float32_t2 noiseCoord = float32_t2(time * 0.00001, uv.y * 25.0);
    float noise = rand(noiseCoord);
    uv.x += noise * noiseIntensity * 0.02;
    
    // 静的ノイズ
    float staticNoise = rand(uv + time * 0.1);
    staticNoise = (staticNoise - 0.5) * noiseIntensity * 0.5;
    
    // 色収差効果を適用
    float aberrationAmount = 1.0 + distortionAmount * 2.0;
    float32_t3 color = chromaticAberration(uv, aberrationAmount);
    
    // スキャンラインとノイズを追加
    color = color * (1.0 + scanline) + staticNoise;
    
    // 血のエフェクトを適用
    color = bloodEffect(input.texcoord, color, bloodAmount);

    // ビネット効果を適用（緊張感を高める）
    color = vignetteEffect(input.texcoord, color, vignetteIntensity);

    // 全体的に暗くして恐怖感を演出
    color = color * (0.8 - bloodAmount * 0.3);
    
    // コントラストを上げる
    color = saturate((color - 0.5) * (1.0 + distortionAmount * 0.5) + 0.5);
    
    output.color = float32_t4(color, 1.0);
    return output;
}