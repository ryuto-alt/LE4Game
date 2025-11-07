#pragma once
#include <cstdint>
#include <exception>
#include <Windows.h>

//--------------------------------------------------------------------------------------
// StepTimer - 高精度ゲームループタイマー
//
// DirectX Tool Kitの実装を参考にした、フレームレート非依存の正確なデルタタイム計算を提供します。
// QueryPerformanceCounterを使用してWindows上で最高精度のタイミングを実現します。
//
// 主な機能:
// - 可変フレームレート対応（デフォルト）
// - 固定フレームレート対応（オプション）
// - デルタタイムの上限設定（デバッグ時の一時停止対策）
// - 64bit整数でのTick管理とdoubleでの秒管理による高精度
//--------------------------------------------------------------------------------------
class StepTimer {
public:
    // コンストラクタ
    StepTimer() noexcept(false);

    // 経過時間を取得（秒単位）
    uint64_t GetElapsedTicks() const noexcept { return elapsedTicks_; }
    double GetElapsedSeconds() const noexcept { return TicksToSeconds(elapsedTicks_); }

    // 合計時間を取得（秒単位）
    uint64_t GetTotalTicks() const noexcept { return totalTicks_; }
    double GetTotalSeconds() const noexcept { return TicksToSeconds(totalTicks_); }

    // フレームカウントを取得
    uint32_t GetFrameCount() const noexcept { return frameCount_; }

    // フレームレートを取得
    uint32_t GetFramesPerSecond() const noexcept { return framesPerSecond_; }

    // 固定タイムステップモードの設定
    void SetFixedTimeStep(bool isFixedTimestep) noexcept { isFixedTimeStep_ = isFixedTimestep; }

    // ターゲットの経過時間を設定（固定タイムステップモード用）
    void SetTargetElapsedTicks(uint64_t targetElapsed) noexcept { targetElapsedTicks_ = targetElapsed; }
    void SetTargetElapsedSeconds(double targetElapsed) noexcept { targetElapsedTicks_ = SecondsToTicks(targetElapsed); }

    // 整数と秒の変換定数（10,000,000 ticks = 1秒）
    static constexpr uint64_t TicksPerSecond = 10000000;

    // 秒 -> Ticks変換
    static constexpr uint64_t SecondsToTicks(double seconds) noexcept { return static_cast<uint64_t>(seconds * TicksPerSecond); }

    // Ticks -> 秒変換
    static constexpr double TicksToSeconds(uint64_t ticks) noexcept { return static_cast<double>(ticks) / TicksPerSecond; }

    // リセット後の経過時間クリア（サスペンド復帰時などに使用）
    void ResetElapsedTime();

    // デルタタイムスムージングを有効化/無効化
    void SetDeltaTimeSmoothing(bool enabled) noexcept { useDeltaSmoothing_ = enabled; }

    // 更新処理
    template<typename TUpdate>
    void Tick(const TUpdate& update) {
        // 現在のタイムスタンプを取得（生のQPC値）
        LARGE_INTEGER currentTime;
        QueryPerformanceCounter(&currentTime);
        uint64_t currentQPC = static_cast<uint64_t>(currentTime.QuadPart);

        // 前回からの経過時間を計算（QPC単位）
        uint64_t qpcDelta = currentQPC - qpcLastTime_;
        qpcLastTime_ = currentQPC;

        // QPCデルタをTicksに変換
        uint64_t timeDelta = (qpcDelta * TicksPerSecond) / qpcFrequency_;

        // デルタタイムの上限をクランプ（1/10秒 = 100ms）
        uint64_t maxDelta = TicksPerSecond / 10;
        if (timeDelta > maxDelta) {
            timeDelta = maxDelta;
        }

        // Unreal Engine風のデルタタイムスムージング
        if (useDeltaSmoothing_) {
            // 過去数フレームのデルタタイムの平均を取る
            deltaHistory_[deltaHistoryIndex_] = timeDelta;
            deltaHistoryIndex_ = (deltaHistoryIndex_ + 1) % DELTA_HISTORY_SIZE;

            // 平均を計算
            uint64_t sum = 0;
            for (int i = 0; i < DELTA_HISTORY_SIZE; ++i) {
                sum += deltaHistory_[i];
            }
            timeDelta = sum / DELTA_HISTORY_SIZE;
        }

        qpcSecondCounter_ += timeDelta;

        // フレームカウント記録
        uint32_t lastFrameCount = frameCount_;

        if (isFixedTimeStep_) {
            // 固定タイムステップモード
            if (targetElapsedTicks_ == 0) {
                throw std::exception("Fixed timestep enabled but target elapsed time is zero");
            }

            leftOverTicks_ += timeDelta;

            while (leftOverTicks_ >= targetElapsedTicks_) {
                elapsedTicks_ = targetElapsedTicks_;
                totalTicks_ += targetElapsedTicks_;
                leftOverTicks_ -= targetElapsedTicks_;
                frameCount_++;

                update();
            }
        } else {
            // 可変タイムステップモード（デフォルト）
            elapsedTicks_ = timeDelta;
            totalTicks_ += timeDelta;
            leftOverTicks_ = 0;
            frameCount_++;

            update();
        }

        // FPSカウンタの更新（1秒ごと）
        if (frameCount_ != lastFrameCount) {
            framesThisSecond_++;
        }

        if (qpcSecondCounter_ >= TicksPerSecond) {
            framesPerSecond_ = framesThisSecond_;
            framesThisSecond_ = 0;
            qpcSecondCounter_ %= TicksPerSecond;
        }
    }

private:
    // 内部状態
    uint64_t qpcFrequency_;         // QueryPerformanceCounterの周波数
    uint64_t qpcLastTime_;          // 前回の時刻
    uint64_t qpcMaxDelta_;          // 最大デルタタイム（1/10秒）
    uint64_t qpcSecondCounter_;     // 1秒カウンタ

    // 経過時間データ
    uint64_t elapsedTicks_;         // 前フレームからの経過Ticks
    uint64_t totalTicks_;           // 合計経過Ticks
    uint64_t leftOverTicks_;        // 固定タイムステップの余剰Ticks

    // フレームカウント
    uint32_t frameCount_;           // 総フレーム数
    uint32_t framesPerSecond_;      // FPS
    uint32_t framesThisSecond_;     // 今秒のフレーム数

    // 固定タイムステップ設定
    uint64_t targetElapsedTicks_;   // ターゲット経過時間
    bool isFixedTimeStep_;          // 固定タイムステップモードフラグ

    // デルタタイムスムージング（Unreal Engine風）
    static constexpr int DELTA_HISTORY_SIZE = 4;  // 過去4フレームの平均を取る
    uint64_t deltaHistory_[DELTA_HISTORY_SIZE];
    int deltaHistoryIndex_;
    bool useDeltaSmoothing_;
};
