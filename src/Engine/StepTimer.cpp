#include "StepTimer.h"
#include <Windows.h>

// コンストラクタ
StepTimer::StepTimer() noexcept(false)
    : elapsedTicks_(0)
    , totalTicks_(0)
    , leftOverTicks_(0)
    , frameCount_(0)
    , framesPerSecond_(0)
    , framesThisSecond_(0)
    , qpcSecondCounter_(0)
    , isFixedTimeStep_(false)
    , targetElapsedTicks_(TicksPerSecond / 60) // デフォルトは60FPS
    , deltaHistoryIndex_(0)
    , useDeltaSmoothing_(true) // デフォルトでスムージング有効
{
    // QueryPerformanceFrequencyで周波数を取得
    LARGE_INTEGER frequency;
    if (!QueryPerformanceFrequency(&frequency)) {
        throw std::exception("QueryPerformanceFrequency failed");
    }

    qpcFrequency_ = static_cast<uint64_t>(frequency.QuadPart);

    // 最大デルタタイムを1/10秒（100ms）に設定
    qpcMaxDelta_ = qpcFrequency_ / 10;

    // 初期時刻を取得
    LARGE_INTEGER currentTime;
    if (!QueryPerformanceCounter(&currentTime)) {
        throw std::exception("QueryPerformanceCounter failed");
    }

    qpcLastTime_ = static_cast<uint64_t>(currentTime.QuadPart);

    // デルタタイム履歴を初期化（60FPSを仮定）
    uint64_t initialDelta = TicksPerSecond / 60;
    for (int i = 0; i < DELTA_HISTORY_SIZE; ++i) {
        deltaHistory_[i] = initialDelta;
    }
}

// リセット後の経過時間クリア
void StepTimer::ResetElapsedTime() {
    LARGE_INTEGER currentTime;
    if (!QueryPerformanceCounter(&currentTime)) {
        throw std::exception("QueryPerformanceCounter failed");
    }

    qpcLastTime_ = static_cast<uint64_t>(currentTime.QuadPart);
    qpcSecondCounter_ = 0;
    framesThisSecond_ = 0;
}
