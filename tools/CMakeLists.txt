cmake_minimum_required(VERSION 3.15)
project(NavMeshTools)

set(CMAKE_CXX_STANDARD 17)

# Recast/Detourのパス
set(RECAST_DIR "${CMAKE_SOURCE_DIR}/../externals/recastnavigation")

# tinygltfのパス
set(TINYGLTF_DIR "${CMAKE_SOURCE_DIR}/../externals/tinygltf")

# インクルードディレクトリ
include_directories(
    "${RECAST_DIR}/Recast/Include"
    "${RECAST_DIR}/Detour/Include"
    "${TINYGLTF_DIR}"
)

# ライブラリディレクトリ
link_directories(
    "${RECAST_DIR}/build/Recast/Release"
    "${RECAST_DIR}/build/Detour/Release"
)

# NavMeshGenerator実行ファイル
add_executable(NavMeshGenerator NavMeshGenerator.cpp)

# リンク
target_link_libraries(NavMeshGenerator
    Recast
    Detour
)

# 出力ディレクトリ
set_target_properties(NavMeshGenerator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../externals"
)
