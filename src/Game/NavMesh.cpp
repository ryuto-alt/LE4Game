#include "NavMesh.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <queue>
#include <cmath>
#include <limits>

// 簡易JSONパーサー（必要に応じて外部ライブラリに置き換え可能）
#define NOMINMAX  // min/maxマクロを無効化
#include <Windows.h>

NavMesh::NavMesh()
    : agentRadius_(0.5f)
    , agentHeight_(2.0f) {
}

NavMesh::~NavMesh() {
}

bool NavMesh::LoadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        OutputDebugStringA(("NavMesh: Failed to open file: " + filepath + "\n").c_str());
        return false;
    }

    // ファイル全体を文字列として読み込む
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string jsonStr = buffer.str();
    file.close();

    // 簡易JSONパース（手動）
    // 注: 本格的な実装ではnlohmann/jsonなどを使用推奨

    vertices_.clear();
    triangles_.clear();

    // verticesをパース
    size_t verticesPos = jsonStr.find("\"vertices\"");
    if (verticesPos == std::string::npos) return false;

    size_t arrayStart = jsonStr.find("[", verticesPos);
    size_t arrayEnd = jsonStr.find("]", arrayStart);

    std::string verticesStr = jsonStr.substr(arrayStart + 1, arrayEnd - arrayStart - 1);

    // 各頂点を解析
    size_t pos = 0;
    while (pos < verticesStr.length()) {
        size_t objStart = verticesStr.find("{", pos);
        if (objStart == std::string::npos) break;

        size_t objEnd = verticesStr.find("}", objStart);
        std::string obj = verticesStr.substr(objStart, objEnd - objStart + 1);

        float x = 0, y = 0, z = 0;

        // "x": value を抽出
        size_t xPos = obj.find("\"x\"");
        if (xPos != std::string::npos) {
            size_t colonPos = obj.find(":", xPos);
            size_t commaPos = obj.find(",", colonPos);
            if (commaPos == std::string::npos) commaPos = obj.find("}", colonPos);
            std::string xStr = obj.substr(colonPos + 1, commaPos - colonPos - 1);
            x = std::stof(xStr);
        }

        // "y": value を抽出
        size_t yPos = obj.find("\"y\"");
        if (yPos != std::string::npos) {
            size_t colonPos = obj.find(":", yPos);
            size_t commaPos = obj.find(",", colonPos);
            if (commaPos == std::string::npos) commaPos = obj.find("}", colonPos);
            std::string yStr = obj.substr(colonPos + 1, commaPos - colonPos - 1);
            y = std::stof(yStr);
        }

        // "z": value を抽出
        size_t zPos = obj.find("\"z\"");
        if (zPos != std::string::npos) {
            size_t colonPos = obj.find(":", zPos);
            size_t commaPos = obj.find(",", colonPos);
            if (commaPos == std::string::npos) commaPos = obj.find("}", colonPos);
            std::string zStr = obj.substr(colonPos + 1, commaPos - colonPos - 1);
            z = std::stof(zStr);
        }

        vertices_.push_back({x, y, z});

        pos = objEnd + 1;
    }

    // trianglesをパース
    size_t trianglesPos = jsonStr.find("\"triangles\"");
    if (trianglesPos == std::string::npos) return false;

    arrayStart = jsonStr.find("[", trianglesPos);
    arrayEnd = jsonStr.find("]", arrayStart);

    // 三角形データの範囲を見つける（ネストした配列に注意）
    int bracketCount = 0;
    for (size_t i = arrayStart; i < jsonStr.length(); i++) {
        if (jsonStr[i] == '[') bracketCount++;
        if (jsonStr[i] == ']') {
            bracketCount--;
            if (bracketCount == 0) {
                arrayEnd = i;
                break;
            }
        }
    }

    std::string trianglesStr = jsonStr.substr(arrayStart + 1, arrayEnd - arrayStart - 1);

    // 各三角形を解析
    pos = 0;
    while (pos < trianglesStr.length()) {
        size_t objStart = trianglesStr.find("{", pos);
        if (objStart == std::string::npos) break;

        size_t objEnd = trianglesStr.find("}", objStart);

        // ネストしたオブジェクトに対応
        int braceCount = 1;
        for (size_t i = objStart + 1; i < trianglesStr.length(); i++) {
            if (trianglesStr[i] == '{') braceCount++;
            if (trianglesStr[i] == '}') {
                braceCount--;
                if (braceCount == 0) {
                    objEnd = i;
                    break;
                }
            }
        }

        std::string obj = trianglesStr.substr(objStart, objEnd - objStart + 1);

        NavTriangle tri;

        // "indices": [0, 1, 2] を抽出
        size_t indicesPos = obj.find("\"indices\"");
        if (indicesPos != std::string::npos) {
            size_t arrStart = obj.find("[", indicesPos);
            size_t arrEnd = obj.find("]", arrStart);
            std::string indicesStr = obj.substr(arrStart + 1, arrEnd - arrStart - 1);

            // カンマで分割
            size_t comma1 = indicesStr.find(",");
            size_t comma2 = indicesStr.find(",", comma1 + 1);

            tri.indices[0] = std::stoi(indicesStr.substr(0, comma1));
            tri.indices[1] = std::stoi(indicesStr.substr(comma1 + 1, comma2 - comma1 - 1));
            tri.indices[2] = std::stoi(indicesStr.substr(comma2 + 1));
        }

        // "center": {x, y, z} を抽出
        size_t centerPos = obj.find("\"center\"");
        if (centerPos != std::string::npos) {
            size_t centerStart = obj.find("{", centerPos);
            size_t centerEnd = obj.find("}", centerStart);
            std::string centerStr = obj.substr(centerStart, centerEnd - centerStart + 1);

            float cx = 0, cy = 0, cz = 0;

            size_t xPos = centerStr.find("\"x\"");
            if (xPos != std::string::npos) {
                size_t colonPos = centerStr.find(":", xPos);
                size_t commaPos = centerStr.find(",", colonPos);
                if (commaPos == std::string::npos) commaPos = centerStr.find("}", colonPos);
                std::string xStr = centerStr.substr(colonPos + 1, commaPos - colonPos - 1);
                cx = std::stof(xStr);
            }

            size_t yPos = centerStr.find("\"y\"");
            if (yPos != std::string::npos) {
                size_t colonPos = centerStr.find(":", yPos);
                size_t commaPos = centerStr.find(",", colonPos);
                if (commaPos == std::string::npos) commaPos = centerStr.find("}", colonPos);
                std::string yStr = centerStr.substr(colonPos + 1, commaPos - colonPos - 1);
                cy = std::stof(yStr);
            }

            size_t zPos = centerStr.find("\"z\"");
            if (zPos != std::string::npos) {
                size_t colonPos = centerStr.find(":", zPos);
                size_t commaPos = centerStr.find(",", colonPos);
                if (commaPos == std::string::npos) commaPos = centerStr.find("}", colonPos);
                std::string zStr = centerStr.substr(colonPos + 1, commaPos - colonPos - 1);
                cz = std::stof(zStr);
            }

            tri.center = {cx, cy, cz};
        }

        triangles_.push_back(tri);

        pos = objEnd + 1;
    }

    // adjacencyをパース
    size_t adjacencyPos = jsonStr.find("\"adjacency\"");
    if (adjacencyPos != std::string::npos) {
        size_t adjStart = jsonStr.find("{", adjacencyPos);
        size_t adjEnd = jsonStr.find("}", adjStart);

        // ネストしたオブジェクトに対応
        int braceCount = 1;
        for (size_t i = adjStart + 1; i < jsonStr.length(); i++) {
            if (jsonStr[i] == '{') braceCount++;
            if (jsonStr[i] == '}') {
                braceCount--;
                if (braceCount == 0) {
                    adjEnd = i;
                    break;
                }
            }
        }

        std::string adjacencyStr = jsonStr.substr(adjStart + 1, adjEnd - adjStart - 1);

        // 各エントリをパース
        size_t pos = 0;
        while (pos < adjacencyStr.length()) {
            size_t keyStart = adjacencyStr.find("\"", pos);
            if (keyStart == std::string::npos) break;

            size_t keyEnd = adjacencyStr.find("\"", keyStart + 1);
            std::string key = adjacencyStr.substr(keyStart + 1, keyEnd - keyStart - 1);
            int triIndex = std::stoi(key);

            size_t arrStart = adjacencyStr.find("[", keyEnd);
            size_t arrEnd = adjacencyStr.find("]", arrStart);
            std::string neighborsStr = adjacencyStr.substr(arrStart + 1, arrEnd - arrStart - 1);

            // 隣接インデックスをパース
            if (triIndex >= 0 && triIndex < triangles_.size()) {
                std::istringstream iss(neighborsStr);
                std::string token;
                while (std::getline(iss, token, ',')) {
                    // 空白とタブを削除
                    token.erase(std::remove_if(token.begin(), token.end(), ::isspace), token.end());
                    if (!token.empty()) {
                        triangles_[triIndex].neighbors.push_back(std::stoi(token));
                    }
                }
            }

            pos = arrEnd + 1;
        }
    }

    char debugMsg[256];
    sprintf_s(debugMsg, "NavMesh: Loaded %zu vertices, %zu triangles\n",
              vertices_.size(), triangles_.size());
    OutputDebugStringA(debugMsg);

    return true;
}

int NavMesh::FindTriangleAtPosition(const Vector3& position) {
    for (size_t i = 0; i < triangles_.size(); i++) {
        if (IsPointInTriangle(position, static_cast<int>(i))) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool NavMesh::IsPointInTriangle(const Vector3& point, int triangleIndex) {
    if (triangleIndex < 0 || triangleIndex >= triangles_.size()) {
        return false;
    }

    const NavTriangle& tri = triangles_[triangleIndex];
    const Vector3& v0 = vertices_[tri.indices[0]];
    const Vector3& v1 = vertices_[tri.indices[1]];
    const Vector3& v2 = vertices_[tri.indices[2]];

    // 2D平面（XZ平面）で判定
    // 重心座標系を使用
    float denom = (v1.z - v2.z) * (v0.x - v2.x) + (v2.x - v1.x) * (v0.z - v2.z);
    if (std::abs(denom) < 0.0001f) return false;

    float a = ((v1.z - v2.z) * (point.x - v2.x) + (v2.x - v1.x) * (point.z - v2.z)) / denom;
    float b = ((v2.z - v0.z) * (point.x - v2.x) + (v0.x - v2.x) * (point.z - v2.z)) / denom;
    float c = 1.0f - a - b;

    return (a >= 0 && a <= 1 && b >= 0 && b <= 1 && c >= 0 && c <= 1);
}

float NavMesh::GetDistance(int tri1, int tri2) {
    if (tri1 < 0 || tri1 >= triangles_.size() || tri2 < 0 || tri2 >= triangles_.size()) {
        return std::numeric_limits<float>::max();
    }

    Vector3 diff = {
        triangles_[tri2].center.x - triangles_[tri1].center.x,
        triangles_[tri2].center.y - triangles_[tri1].center.y,
        triangles_[tri2].center.z - triangles_[tri1].center.z
    };

    return std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
}

float NavMesh::GetHeuristic(int triangleIndex, const Vector3& goal) {
    if (triangleIndex < 0 || triangleIndex >= triangles_.size()) {
        return std::numeric_limits<float>::max();
    }

    Vector3 diff = {
        goal.x - triangles_[triangleIndex].center.x,
        goal.y - triangles_[triangleIndex].center.y,
        goal.z - triangles_[triangleIndex].center.z
    };

    return std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
}

std::vector<Vector3> NavMesh::FindPath(const Vector3& start, const Vector3& end) {
    std::vector<Vector3> path;

    // スタートとゴールの三角形を検索
    int startTri = FindTriangleAtPosition(start);
    int endTri = FindTriangleAtPosition(end);

    if (startTri == -1 || endTri == -1) {
        OutputDebugStringA("NavMesh: Start or end position not on navmesh\n");
        return path;
    }

    if (startTri == endTri) {
        // 同じ三角形内
        path.push_back(start);
        path.push_back(end);
        return path;
    }

    // A*アルゴリズム
    std::vector<PathNode> nodes(triangles_.size());
    std::vector<bool> closedSet(triangles_.size(), false);

    auto compare = [](const PathNode& a, const PathNode& b) {
        return a.fCost > b.fCost;
    };
    std::priority_queue<PathNode, std::vector<PathNode>, decltype(compare)> openSet(compare);

    // スタートノード
    nodes[startTri].triangleIndex = startTri;
    nodes[startTri].gCost = 0;
    nodes[startTri].hCost = GetHeuristic(startTri, end);
    nodes[startTri].fCost = nodes[startTri].hCost;
    nodes[startTri].parentIndex = -1;

    openSet.push(nodes[startTri]);

    while (!openSet.empty()) {
        PathNode current = openSet.top();
        openSet.pop();

        int currentIdx = current.triangleIndex;

        if (closedSet[currentIdx]) continue;
        closedSet[currentIdx] = true;

        // ゴールに到達
        if (currentIdx == endTri) {
            // パスを再構築
            path.clear();
            path.push_back(end);

            int idx = currentIdx;
            while (idx != -1) {
                if (idx != endTri) {
                    path.push_back(triangles_[idx].center);
                }
                idx = nodes[idx].parentIndex;
            }

            path.push_back(start);
            std::reverse(path.begin(), path.end());

            char debugMsg[128];
            sprintf_s(debugMsg, "NavMesh: Path found with %zu waypoints\n", path.size());
            OutputDebugStringA(debugMsg);

            return path;
        }

        // 隣接ノードを探索
        for (int neighborIdx : triangles_[currentIdx].neighbors) {
            if (closedSet[neighborIdx]) continue;

            float newGCost = current.gCost + GetDistance(currentIdx, neighborIdx);

            if (nodes[neighborIdx].triangleIndex == -1 || newGCost < nodes[neighborIdx].gCost) {
                nodes[neighborIdx].triangleIndex = neighborIdx;
                nodes[neighborIdx].gCost = newGCost;
                nodes[neighborIdx].hCost = GetHeuristic(neighborIdx, end);
                nodes[neighborIdx].fCost = newGCost + nodes[neighborIdx].hCost;
                nodes[neighborIdx].parentIndex = currentIdx;

                openSet.push(nodes[neighborIdx]);
            }
        }
    }

    OutputDebugStringA("NavMesh: No path found\n");
    return path;
}
