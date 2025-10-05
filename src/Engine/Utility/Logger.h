#pragma once
#include <string>

// デバッグログを無効化するマクロ
#define OutputDebugStringA(str) ((void)0)

namespace Logger
{
	void Log(const std::string& message);
};

