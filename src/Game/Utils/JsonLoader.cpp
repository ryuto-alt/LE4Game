#include "JsonLoader.h"
#include <iostream>
#include <algorithm>

std::string JsonLoader::ReadFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filepath << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

float JsonLoader::ParseFloat(const std::string& str) {
    try {
        return std::stof(str);
    }
    catch (...) {
        return 0.0f;
    }
}

bool JsonLoader::LoadOrbPositions(const std::string& filepath, std::vector<Vector3>& positions) {
    std::string jsonContent = ReadFile(filepath);
    if (jsonContent.empty()) {
        return false;
    }

    return ParseJson(jsonContent, positions);
}

bool JsonLoader::LoadOrbPositionsDetailed(const std::string& filepath, std::vector<OrbPosition>& positions) {
    std::string jsonContent = ReadFile(filepath);
    if (jsonContent.empty()) {
        return false;
    }

    return ParseJsonDetailed(jsonContent, positions);
}

bool JsonLoader::ParseJson(const std::string& json, std::vector<Vector3>& positions) {
    positions.clear();

    // Find "positions" array
    size_t positionsStart = json.find("\"positions\"");
    if (positionsStart == std::string::npos) {
        std::cerr << "Could not find 'positions' in JSON" << std::endl;
        return false;
    }

    // Find the start of the array
    size_t arrayStart = json.find('[', positionsStart);
    if (arrayStart == std::string::npos) {
        return false;
    }

    std::cout << "Starting to parse positions..." << std::endl;

    // Find each position object
    size_t currentPos = arrayStart;
    int count = 0;
    while (true) {
        // Find next "position" field
        size_t posFieldStart = json.find("\"position\"", currentPos);
        if (posFieldStart == std::string::npos) {
            std::cout << "No more position fields found after " << count << " positions" << std::endl;
            break;
        }

        // Find the array for this position
        size_t posArrayStart = json.find('[', posFieldStart);
        if (posArrayStart == std::string::npos) {
            break;
        }

        size_t posArrayEnd = json.find(']', posArrayStart);
        if (posArrayEnd == std::string::npos) {
            break;
        }

        // Extract the position array content
        std::string posArray = json.substr(posArrayStart + 1, posArrayEnd - posArrayStart - 1);

        // Parse the three float values
        std::vector<float> values;
        std::string value;
        std::stringstream ss(posArray);

        while (std::getline(ss, value, ',')) {
            // Remove whitespace and quotes
            value.erase(std::remove_if(value.begin(), value.end(),
                [](char c) { return std::isspace(c) || c == '\"'; }), value.end());

            if (!value.empty()) {
                values.push_back(ParseFloat(value));
            }
        }

        if (values.size() >= 3) {
            // Convert from Blender coordinates to DirectX coordinates
            // Blender: X=right, Y=forward, Z=up
            // DirectX: X=right, Y=up, Z=forward
            // Conversion: Game.X = -Blender.X (flip), Game.Y = Blender.Z, Game.Z = -Blender.Y (180deg rotation on Y axis)
            float blender_x = values[0];
            float blender_y = values[1];
            float blender_z = values[2];

            float game_x = -blender_x;  // Flip X
            float game_y = blender_z;   // Z becomes Y
            float game_z = -blender_y;  // Y becomes -Z (180deg rotation)

            positions.push_back(Vector3(game_x, game_y, game_z));
            count++;
            std::cout << "Loaded position " << count << ": Blender(" << blender_x << ", " << blender_y << ", " << blender_z
                      << ") -> Game(" << game_x << ", " << game_y << ", " << game_z << ")" << std::endl;
        }

        // Move to next position
        currentPos = posArrayEnd + 1;

        // Simply continue to find more positions
        // The loop will naturally end when no more "position" fields are found
    }

    std::cout << "Loaded " << positions.size() << " orb positions from JSON" << std::endl;
    return !positions.empty();
}

bool JsonLoader::ParseJsonDetailed(const std::string& json, std::vector<OrbPosition>& positions) {
    positions.clear();

    // Find "positions" array
    size_t positionsStart = json.find("\"positions\"");
    if (positionsStart == std::string::npos) {
        return false;
    }

    // Find the start of the array
    size_t arrayStart = json.find('[', positionsStart);
    if (arrayStart == std::string::npos) {
        return false;
    }

    // Find each position object
    size_t currentPos = arrayStart;
    while (true) {
        // Find next object
        size_t objectStart = json.find('{', currentPos);
        if (objectStart == std::string::npos) {
            break;
        }

        size_t objectEnd = json.find('}', objectStart);
        if (objectEnd == std::string::npos) {
            break;
        }

        std::string object = json.substr(objectStart, objectEnd - objectStart + 1);

        OrbPosition orbPos;

        // Parse index
        size_t indexPos = object.find("\"index\"");
        if (indexPos != std::string::npos) {
            size_t colonPos = object.find(':', indexPos);
            size_t commaPos = object.find(',', colonPos);
            if (colonPos != std::string::npos) {
                std::string indexStr = object.substr(colonPos + 1,
                    (commaPos != std::string::npos ? commaPos : object.length()) - colonPos - 1);
                orbPos.index = std::stoi(indexStr);
            }
        }

        // Parse name
        size_t namePos = object.find("\"node_name\"");
        if (namePos == std::string::npos) {
            namePos = object.find("\"name\"");
        }
        if (namePos != std::string::npos) {
            size_t colonPos = object.find(':', namePos);
            size_t quotStart = object.find('\"', colonPos + 1);
            size_t quotEnd = object.find('\"', quotStart + 1);
            if (quotStart != std::string::npos && quotEnd != std::string::npos) {
                orbPos.name = object.substr(quotStart + 1, quotEnd - quotStart - 1);
            }
        }

        // Parse position array
        size_t posFieldStart = object.find("\"position\"");
        if (posFieldStart != std::string::npos) {
            size_t posArrayStart = object.find('[', posFieldStart);
            size_t posArrayEnd = object.find(']', posArrayStart);

            if (posArrayStart != std::string::npos && posArrayEnd != std::string::npos) {
                std::string posArray = object.substr(posArrayStart + 1, posArrayEnd - posArrayStart - 1);

                std::vector<float> values;
                std::string value;
                std::stringstream ss(posArray);

                while (std::getline(ss, value, ',')) {
                    value.erase(std::remove_if(value.begin(), value.end(),
                        [](char c) { return std::isspace(c) || c == '\"'; }), value.end());

                    if (!value.empty()) {
                        values.push_back(ParseFloat(value));
                    }
                }

                if (values.size() >= 3) {
                    // Convert from Blender coordinates to DirectX coordinates
                    float blender_x = values[0];
                    float blender_y = values[1];
                    float blender_z = values[2];

                    float game_x = -blender_x;  // Flip X
                    float game_y = blender_z;   // Z becomes Y
                    float game_z = -blender_y;  // Y becomes -Z (180deg rotation)

                    orbPos.position = Vector3(game_x, game_y, game_z);
                    positions.push_back(orbPos);
                }
            }
        }

        // Move to next object
        currentPos = objectEnd + 1;
    }

    return !positions.empty();
}