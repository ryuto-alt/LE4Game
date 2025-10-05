#include "Object3d.hlsli"

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
    float32_t4 weight : WEIGHT0;
    int4 index : INDEX0;
};

struct Skinned
{
    float32_t4 position;
    float32_t3 normal;
};

struct Well
{
    float32_t4x4 skeletonSpaceMatrix;
    float32_t4x4 skeletonSpaceInverseTransposeMatrix;
};
StructuredBuffer<Well> gMatrixPalette : register(t1);

Skinned Skinning(VertexShaderInput input)
{
    Skinned skinned;
    
    // スキニング計算：各ボーンの影響を加重平均
    skinned.position = mul(input.position, gMatrixPalette[input.index.x].skeletonSpaceMatrix) * input.weight.x;
    skinned.position += mul(input.position, gMatrixPalette[input.index.y].skeletonSpaceMatrix) * input.weight.y;
    skinned.position += mul(input.position, gMatrixPalette[input.index.z].skeletonSpaceMatrix) * input.weight.z;
    skinned.position += mul(input.position, gMatrixPalette[input.index.w].skeletonSpaceMatrix) * input.weight.w;
    skinned.position.w = 1.0f;
    
    // 法線のスキニング
    skinned.normal = mul(input.normal, (float32_t3x3)gMatrixPalette[input.index.x].skeletonSpaceInverseTransposeMatrix) * input.weight.x;
    skinned.normal += mul(input.normal, (float32_t3x3)gMatrixPalette[input.index.y].skeletonSpaceInverseTransposeMatrix) * input.weight.y;
    skinned.normal += mul(input.normal, (float32_t3x3)gMatrixPalette[input.index.z].skeletonSpaceInverseTransposeMatrix) * input.weight.z;
    skinned.normal += mul(input.normal, (float32_t3x3)gMatrixPalette[input.index.w].skeletonSpaceInverseTransposeMatrix) * input.weight.w;
    skinned.normal = normalize(skinned.normal);
    
    return skinned;
}

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    // スキニング処理
    Skinned skinned = Skinning(input);
    
    // スキニング後の頂点でWVP変換
    output.position = mul(skinned.position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    
    // ワールド座標を計算
    float32_t4 worldPos = mul(skinned.position, gTransformationMatrix.World);
    output.worldPos = worldPos.xyz;
    
    // スキニング後の法線をワールド空間に変換
    float32_t3 worldNormal = mul(skinned.normal, (float32_t3x3)gTransformationMatrix.World);
    output.normal = normalize(worldNormal);
    
    return output;
}