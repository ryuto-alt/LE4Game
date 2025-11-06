#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include "../../Engine/Math/Mymath.h"

class JsonLoader {
public:
    struct OrbPosition {
        int index;
        std::string name;
        Vector3 position;
    };

    static bool LoadOrbPositions(const std::string& filepath, std::vector<Vector3>& positions);
    static bool LoadOrbPositionsDetailed(const std::string& filepath, std::vector<OrbPosition>& positions);

private:
    static std::string ReadFile(const std::string& filepath);
    static bool ParseJson(const std::string& json, std::vector<Vector3>& positions);
    static bool ParseJsonDetailed(const std::string& json, std::vector<OrbPosition>& positions);
    static float ParseFloat(const std::string& str);
};