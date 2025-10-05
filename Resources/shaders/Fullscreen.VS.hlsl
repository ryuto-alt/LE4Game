// Fullscreen処理用のVertex Shader
// 資料に基づく実装：頂点データを入力しない、3頂点を想定

struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
};

VertexShaderOutput main(uint32_t vertexId : SV_VertexID)
{
    VertexShaderOutput output;
    
    // 3頂点でフルスクリーンを覆う三角形を生成
    // vertexId = 0: 左下 (-1, -1)
    // vertexId = 1: 左上 (-1, 3)
    // vertexId = 2: 右下 (3, -1)
    // この方法により、画面全体を覆う大きな三角形を作成
    
    if (vertexId == 0)
    {
        // 左下
        output.position = float32_t4(-1.0f, -1.0f, 0.0f, 1.0f);
        output.texcoord = float32_t2(0.0f, 1.0f);
    }
    else if (vertexId == 1)
    {
        // 左上（画面外まで伸ばすが、テクスチャ座標は適切に設定）
        output.position = float32_t4(-1.0f, 3.0f, 0.0f, 1.0f);
        output.texcoord = float32_t2(0.0f, -1.0f);
    }
    else
    {
        // 右下（画面外まで伸ばすが、テクスチャ座標は適切に設定）
        output.position = float32_t4(3.0f, -1.0f, 0.0f, 1.0f);
        output.texcoord = float32_t2(2.0f, 1.0f);
    }
    
    return output;
}