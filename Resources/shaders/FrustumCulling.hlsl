// GPU駆動フラスタムカリング用コンピュートシェーダー
// 5860万頂点から可視オブジェクトのみを40-60%削減

// 定数バッファ
cbuffer CullingConstants : register(b0) {
    float4x4 ViewProjection;
    float4 FrustumPlanes[6];
    float3 CameraPosition;
    float NearPlane;
    float FarPlane;
    uint TotalDrawCalls;
    uint2 Padding;
};

// オブジェクト情報
struct ObjectData {
    float3 BoundsMin;
    float Padding1;
    float3 BoundsMax;
    float Padding2;
    float4x4 WorldMatrix;
};

// ドローコマンド
struct DrawCommand {
    uint VertexCountPerInstance;
    uint InstanceCount;
    uint StartVertexLocation;
    uint StartInstanceLocation;
    uint Visible; // 0 or 1
};

// 入力バッファ
StructuredBuffer<ObjectData> Objects : register(t0);

// 出力バッファ
RWStructuredBuffer<DrawCommand> DrawCommands : register(u0);
RWStructuredBuffer<uint> VisibleCount : register(u1);

// AABBがフラスタム内にあるかチェック
bool IsAABBVisible(float3 boundsMin, float3 boundsMax) {
    // 8つの頂点すべてをチェック
    float3 corners[8];
    corners[0] = float3(boundsMin.x, boundsMin.y, boundsMin.z);
    corners[1] = float3(boundsMax.x, boundsMin.y, boundsMin.z);
    corners[2] = float3(boundsMin.x, boundsMax.y, boundsMin.z);
    corners[3] = float3(boundsMax.x, boundsMax.y, boundsMin.z);
    corners[4] = float3(boundsMin.x, boundsMin.y, boundsMax.z);
    corners[5] = float3(boundsMax.x, boundsMin.y, boundsMax.z);
    corners[6] = float3(boundsMin.x, boundsMax.y, boundsMax.z);
    corners[7] = float3(boundsMax.x, boundsMax.y, boundsMax.z);

    // 各フラスタム平面に対してチェック
    for (uint i = 0; i < 6; ++i) {
        float4 plane = FrustumPlanes[i];

        bool allOutside = true;
        for (uint j = 0; j < 8; ++j) {
            float distance = dot(plane.xyz, corners[j]) + plane.w;
            if (distance >= 0.0f) {
                allOutside = false;
                break;
            }
        }

        // すべての頂点が平面の外側にある場合、このオブジェクトは不可視
        if (allOutside) {
            return false;
        }
    }

    return true;
}

// 距離ベースのカリング
bool IsDistanceVisible(float3 boundsCenter, float maxDistance) {
    float distance = length(CameraPosition - boundsCenter);
    return distance <= maxDistance;
}

// メインコンピュートシェーダー
[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint objectIndex = dispatchThreadID.x;

    // 範囲チェック
    if (objectIndex >= TotalDrawCalls) {
        return;
    }

    // オブジェクト情報を取得
    ObjectData obj = Objects[objectIndex];

    // ワールド座標に変換
    float3 worldMin = mul(float4(obj.BoundsMin, 1.0f), obj.WorldMatrix).xyz;
    float3 worldMax = mul(float4(obj.BoundsMax, 1.0f), obj.WorldMatrix).xyz;
    float3 boundsCenter = (worldMin + worldMax) * 0.5f;

    // フラスタムカリング
    bool visible = IsAABBVisible(worldMin, worldMax);

    // 距離カリング(オプション - 10000単位以内)
    if (visible) {
        visible = IsDistanceVisible(boundsCenter, 10000.0f);
    }

    // 結果を書き込み
    DrawCommand cmd;
    cmd.VertexCountPerInstance = DrawCommands[objectIndex].VertexCountPerInstance;
    cmd.InstanceCount = visible ? 1u : 0u; // 不可視の場合はInstanceCountを0に
    cmd.StartVertexLocation = DrawCommands[objectIndex].StartVertexLocation;
    cmd.StartInstanceLocation = DrawCommands[objectIndex].StartInstanceLocation;
    cmd.Visible = visible ? 1u : 0u;

    DrawCommands[objectIndex] = cmd;

    // 可視オブジェクト数をカウント
    if (visible) {
        InterlockedAdd(VisibleCount[0], 1u);
    }
}
