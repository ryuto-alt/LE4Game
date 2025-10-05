#include"object3d.hlsli"

struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);


struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    
    // ワールド座標を計算
    float32_t4 worldPos = mul(input.position, gTransformationMatrix.World);
    output.worldPos = worldPos.xyz;
    
    // 法線の計算を精密に行い、正規化を確実に行う
    float32_t3 worldNormal = mul(input.normal, (float32_t3x3) gTransformationMatrix.World);
    output.normal = normalize(worldNormal);
    
    return output;
}