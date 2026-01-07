// pch.h - Precompiled Header
// このファイルには、頻繁に使用されるが変更頻度の低いヘッダーを含めます
#pragma once

// ========================================
// Windows SDK
// ========================================
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

// Windows.hのマクロを無効化（メンバー関数名との衝突を防ぐ）
#ifdef GetObject
#undef GetObject
#endif

// ========================================
// DirectX 12
// ========================================
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <wrl/client.h>

// DirectX Math
#include <DirectXMath.h>
#include <DirectXColors.h>

// D3D12 Helper
#include "d3dx12.h"

// ========================================
// C++ Standard Library
// ========================================
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// ========================================
// Link Libraries
// ========================================
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")
