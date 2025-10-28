// TV Noise Effect for GameOver Screen
// Inspired by https://www.youtube.com/watch?v=zXsWftRdsvU

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

struct TimeBuffer {
    float32_t time;
    float32_t noiseIntensity;
    float32_t2 padding;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<TimeBuffer> gTime : register(b1);
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

// Value Noise - GLSL fract() to HLSL frac()
float Noise21(float32_t2 p, float ta, float tb) {
    return frac(sin(p.x * ta + p.y * tb) * 5678.0);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float32_t2 uv = input.texcoord;

    // Get original texture color
    float32_t4 transformedUV = mul(float32_t4(uv, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    // TV Noise calculation
    float t = gTime.time + 123.0; // tweak the start moment
    float ta = t * 0.654321;
    float tb = t * (ta * 0.123456);

    float noiseValue = Noise21(uv, ta, tb);
    float32_t3 noiseColor = float32_t3(noiseValue, noiseValue, noiseValue);

    // Mix texture with noise based on intensity
    float intensity = gTime.noiseIntensity;
    float32_t3 finalColor = lerp(textureColor.rgb, noiseColor, intensity);

    output.color = float32_t4(finalColor, textureColor.a) * gMaterial.color;

    return output;
}
