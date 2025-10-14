// White Static Noise Effect Pixel Shader
// TV砂嵐風の白黒ノイズエフェクト

struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
};

// Constant Buffer
cbuffer StaticParams : register(b0)
{
    float32_t time;         // 経過時間
    float32_t intensity;    // ノイズの強度 (0.0 - 1.0)
    float32_t2 padding;
};

// テクスチャとサンプラー
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

// 高速な擬似乱数生成（TV静的ノイズ用）
float rand(float2 co)
{
    return frac(sin(dot(co, float2(12.9898, 78.233))) * 43758.5453);
}

// より細かいランダム値生成
float rand2(float2 co, float seed)
{
    return frac(sin(dot(co + seed, float2(12.9898, 78.233))) * 43758.5453123);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float2 uv = input.texcoord;

    // 時間でシード値を変化させて、フレームごとにノイズパターンを変える
    float timeSeed = floor(time * 60.0); // 60FPSでノイズパターンを変更

    // ピクセル座標を大きくしてノイズを細かくする
    float2 pixelCoord = uv * float2(1280.0, 720.0); // 画面解像度に合わせて調整

    // 非常に細かいノイズを生成（ピクセル単位）
    float noise = rand2(floor(pixelCoord), timeSeed);

    // グリッチ効果：ランダムな横線
    float lineNoise = rand(float2(floor(uv.y * 200.0), timeSeed));
    float glitchLine = step(0.97, lineNoise); // 3%の確率で横線

    // 横線が出た場合、ノイズを強化
    noise = lerp(noise, 1.0, glitchLine * 0.7);

    // ノイズの閾値処理（白黒のはっきりしたノイズ）
    float staticNoise = step(0.5, noise);

    // 白黒ノイズ（TV砂嵐風）
    float3 whiteNoise = float3(staticNoise, staticNoise, staticNoise);

    // より自然な砂嵐のために、グレーの中間調も追加
    float grayScale = noise * 0.3 + staticNoise * 0.7;
    float3 finalNoise = float3(grayScale, grayScale, grayScale);

    // グリッチのちらつき（ランダムな明滅）
    float flicker = rand(float2(timeSeed * 0.1, 0.5));
    finalNoise *= (0.85 + flicker * 0.3);

    // 強度で最終的なカラーを調整
    output.color = float32_t4(finalNoise * intensity, intensity);

    return output;
}
