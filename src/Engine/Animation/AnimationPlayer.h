#pragma once
#include "Animation.h"
#include <string>

// アニメーション再生クラス
class AnimationPlayer {
public:
    // コンストラクタ
    AnimationPlayer();
    
    // デストラクタ
    ~AnimationPlayer();
    
    // アニメーションを設定
    void SetAnimation(const Animation& animation);
    
    // アニメーション時刻を更新
    void Update(float deltaTime);
    
    // 指定したノードのローカル変換行列を取得
    Matrix4x4 GetLocalMatrix(const std::string& nodeName);
    
    // アニメーション時刻を設定
    void SetTime(float time);
    
    // 現在のアニメーション時刻を取得
    float GetTime() const { return animationTime_; }
    
    // アニメーションの長さを取得
    float GetDuration() const;
    
    // アニメーションを取得
    Animation& GetAnimation() { return animation_; }
    const Animation& GetAnimation() const { return animation_; }
    
    // アニメーションがループするかを設定
    void SetLoop(bool loop) { isLoop_ = loop; }
    
    // アニメーションが再生中かを取得
    bool IsPlaying() const { return isPlaying_; }
    
    // アニメーション再生を開始
    void Play();
    
    // アニメーション再生を停止
    void Stop();
    
    // アニメーション再生を一時停止
    void Pause();

private:
    Animation animation_;          // 再生するアニメーション
    float animationTime_;          // 現在の時刻（秒）
    bool isPlaying_;              // 再生中フラグ
    bool isLoop_;                 // ループフラグ
};