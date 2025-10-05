// PBRスキニング専用頂点シェーダー
// PBRObject3d.PS.hlslと互換性のある出力を提供

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

struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t3 worldPosition : POSITION0;
    float32_t3 tangent : TANGENT0;
    float32_t3 bitangent : BITANGENT0;
};

struct Skinned
{
    float32_t4 position;
    float32_t3 normal;
    float32_t3 tangent;
    float32_t3 bitangent;
};

struct Well
{
    float32_t4x4 skeletonSpaceMatrix;
    float32_t4x4 skeletonSpaceInverseTransposeMatrix;
};
StructuredBuffer<Well> gMatrixPalette : register(t1);

// スキニング処理（TANGENTとBITANGENT計算を含む）
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
    
    // Tangent と Bitangent を Normal から計算（PBRで必要）
    // 簡単な手法：Normalから垂直なベクトルを生成
    float3 worldNormal = skinned.normal;
    
    // 法線ベクトルに垂直なタンジェントベクトルを計算
    float3 arbitraryVector = abs(worldNormal.x) < 0.9 ? float3(1, 0, 0) : float3(0, 1, 0);
    skinned.tangent = normalize(cross(arbitraryVector, worldNormal));
    
    // バイタンジェントはタンジェントと法線の外積
    skinned.bitangent = normalize(cross(worldNormal, skinned.tangent));
    
    return skinned;
}

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    // スキニング処理
    Skinned skinned = Skinning(input);
    
    // 座標変換
    output.position = mul(skinned.position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    
    // ワールド座標での法線・タンジェント・バイタンジェント
    output.normal = normalize(mul(skinned.normal, (float32_t3x3)gTransformationMatrix.World));
    output.tangent = normalize(mul(skinned.tangent, (float32_t3x3)gTransformationMatrix.World));
    output.bitangent = normalize(mul(skinned.bitangent, (float32_t3x3)gTransformationMatrix.World));
    
    // ワールド座標での位置
    output.worldPosition = mul(skinned.position, gTransformationMatrix.World).xyz;
    
    return output;
}