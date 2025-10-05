#include "Skybox.hlsli"

struct Material {
    float4 color;
    int enableLighting;
};

ConstantBuffer<Material> gMaterial : register(b0);
TextureCube<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    
    // Cubemapから色をサンプリング（3次元UV座標を使用）
    float3 direction = normalize(input.texcoord);
    float4 textureColor = gTexture.Sample(gSampler, direction);
    
    // マテリアルカラーと合成
    output.color = textureColor * gMaterial.color;
    
    return output;
}