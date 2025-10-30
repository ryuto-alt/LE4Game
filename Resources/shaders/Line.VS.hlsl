// ライン描画用頂点シェーダー

cbuffer TransformMatrix : register(b0) {
    float4x4 wvpMatrix;
};

struct VSInput {
    float4 position : POSITION0;
    float4 color : COLOR0;
};

struct VSOutput {
    float4 svPosition : SV_POSITION;
    float4 color : COLOR0;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.svPosition = mul(input.position, wvpMatrix);
    output.color = input.color;
    return output;
}
