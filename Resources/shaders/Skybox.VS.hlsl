#include "Skybox.hlsli"

struct VertexShaderInput {
    float4 position : POSITION0;
};

struct TransformationMatrix {
    float4x4 WVP;
    float4x4 World;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;
    
    // WVP行列で変換
    output.position = mul(input.position, gTransformationMatrix.WVP);
    
    // Skybox特有の処理：zとwを同じ値にして最遠方に配置
    output.position = output.position.xyww;
    
    // texcoordは頂点位置をそのまま使用（3次元ベクトル）
    output.texcoord = input.position.xyz;
    
    return output;
}