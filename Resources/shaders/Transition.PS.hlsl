// トランジションエフェクト用のPixel Shader
// ホラーな砂嵐エフェクトを実装

struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
};

// トランジションパラメータ
cbuffer TransitionParams : register(b0)
{
    float32_t progress;        // トランジション進行度 (0.0 - 1.0)
    float32_t aspectRatio;     // アスペクト比
    float32_t2 padding;        // パディング
};

// レンダーターゲットのテクスチャ
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// ハッシュ関数（疑似乱数生成用）
float hash(float2 p)
{
    float h = dot(p, float2(127.1, 311.7));
    return frac(sin(h) * 43758.5453123);
}

// ノイズ関数
float noise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);

    // スムーズステップ補間
    float2 u = f * f * (3.0 - 2.0 * f);

    // 4つの角のハッシュ値を取得
    float a = hash(i + float2(0.0, 0.0));
    float b = hash(i + float2(1.0, 0.0));
    float c = hash(i + float2(0.0, 1.0));
    float d = hash(i + float2(1.0, 1.0));

    // バイリニア補間
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

// フラクタルノイズ（複数のオクターブを重ねる）
float fractalNoise(float2 p)
{
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;

    for (int i = 0; i < 5; i++)
    {
        value += amplitude * noise(p * frequency);
        frequency *= 2.0;
        amplitude *= 0.5;
    }

    return value;
}

// グリッチエフェクト用のランダム値
float glitchRandom(float2 p, float seed)
{
    return frac(sin(dot(p + seed, float2(12.9898, 78.233))) * 43758.5453);
}

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // 元のテクスチャカラーを取得
    float32_t4 originalColor = gTexture.Sample(gSampler, input.texcoord);

    // トランジション進行度によって砂嵐の強度を制御
    // progress = 0.0 で完全に砂嵐、progress = 1.0 で完全に元の画像
    float noiseIntensity = 1.0 - progress;

    // 砂嵐エフェクト
    if (noiseIntensity > 0.01)
    {
        float2 uv = input.texcoord;

        // 高周波ノイズ（TVの砂嵐風）
        float2 noiseCoord = uv * 100.0;
        float staticNoise = fractalNoise(noiseCoord);

        // グリッチエフェクト（ランダムな横線）
        float glitchLine = step(0.98, glitchRandom(float2(floor(uv.y * 100.0), 0.0), progress * 10.0));

        // 水平方向のズレ（グリッチ効果）
        float horizontalShift = glitchLine * (glitchRandom(float2(floor(uv.y * 50.0), 1.0), progress * 15.0) - 0.5) * 0.1 * noiseIntensity;
        float2 glitchedUV = float2(uv.x + horizontalShift, uv.y);

        // グリッチ適用後のカラーを取得
        float32_t4 glitchedColor = gTexture.Sample(gSampler, glitchedUV);

        // 砂嵐をグレースケール化
        float3 staticColor = float3(staticNoise, staticNoise, staticNoise);

        // RGB分離（クロマティックアバレーション風）
        float2 redOffset = float2(0.002 * noiseIntensity, 0.0);
        float2 blueOffset = float2(-0.002 * noiseIntensity, 0.0);

        float r = gTexture.Sample(gSampler, glitchedUV + redOffset).r;
        float g = glitchedColor.g;
        float b = gTexture.Sample(gSampler, glitchedUV + blueOffset).b;

        float3 aberrationColor = float3(r, g, b);

        // 砂嵐とオリジナル画像をブレンド
        // noiseIntensityが高いほど砂嵐が強く表示される
        float3 mixedColor = lerp(aberrationColor, staticColor, noiseIntensity * 0.8);

        // ホラー感を出すために暗めに調整
        mixedColor *= 0.7 + 0.3 * (1.0 - noiseIntensity);

        // ビネット効果（周辺を暗く）
        float2 vignetteUV = uv * 2.0 - 1.0;
        float vignette = 1.0 - dot(vignetteUV, vignetteUV) * 0.3 * noiseIntensity;
        mixedColor *= vignette;

        output.color = float32_t4(mixedColor, 1.0);
    }
    else
    {
        // トランジション完了時は元の画像をそのまま表示
        output.color = originalColor;
    }

    return output;
}
