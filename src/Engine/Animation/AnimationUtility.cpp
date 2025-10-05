#include "AnimationUtility.h"
#include <cassert>
#include <cmath>
#include <algorithm>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <Windows.h>
#include <string>
#include "Mymath.h"

Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time) {
    assert(!keyframes.empty());
    assert(!std::isnan(time));
    
    if (keyframes.size() == 1 || time <= keyframes[0].time) {
        const Vector3& value = keyframes[0].value;
        assert(!std::isnan(value.x) && !std::isnan(value.y) && !std::isnan(value.z));
        return value;
    }
    
    for (size_t index = 0; index < keyframes.size() - 1; ++index) {
        size_t nextIndex = index + 1;
        if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
            float timeDiff = keyframes[nextIndex].time - keyframes[index].time;
            assert(timeDiff > 0.0f);
            
            float t = (time - keyframes[index].time) / timeDiff;
            assert(!std::isnan(t));
            assert(t >= 0.0f && t <= 1.0f);
            
            Vector3 result = Lerp(keyframes[index].value, keyframes[nextIndex].value, t);
            assert(!std::isnan(result.x) && !std::isnan(result.y) && !std::isnan(result.z));
            return result;
        }
    }
    
    const Vector3& value = keyframes.back().value;
    assert(!std::isnan(value.x) && !std::isnan(value.y) && !std::isnan(value.z));
    return value;
}

Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time) {
    assert(!keyframes.empty());
    assert(!std::isnan(time));
    
    if (keyframes.size() == 1 || time <= keyframes[0].time) {
        const Quaternion& value = keyframes[0].value;
        assert(!std::isnan(value.x) && !std::isnan(value.y) && !std::isnan(value.z) && !std::isnan(value.w));
        return value;
    }
    
    for (size_t index = 0; index < keyframes.size() - 1; ++index) {
        size_t nextIndex = index + 1;
        if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
            float timeDiff = keyframes[nextIndex].time - keyframes[index].time;
            assert(timeDiff > 0.0f);
            
            float t = (time - keyframes[index].time) / timeDiff;
            assert(!std::isnan(t));
            assert(t >= 0.0f && t <= 1.0f);
            
            const Quaternion& q1 = keyframes[index].value;
            const Quaternion& q2 = keyframes[nextIndex].value;
            assert(!std::isnan(q1.x) && !std::isnan(q1.y) && !std::isnan(q1.z) && !std::isnan(q1.w));
            assert(!std::isnan(q2.x) && !std::isnan(q2.y) && !std::isnan(q2.z) && !std::isnan(q2.w));
            
            Quaternion result = Slerp(q1, q2, t);
            assert(!std::isnan(result.x) && !std::isnan(result.y) && !std::isnan(result.z) && !std::isnan(result.w));
            return result;
        }
    }
    
    const Quaternion& value = keyframes.back().value;
    assert(!std::isnan(value.x) && !std::isnan(value.y) && !std::isnan(value.z) && !std::isnan(value.w));
    return value;
}

// アニメーション読み込み関数
Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename) {
    Animation animation;
    
    // assimpでファイルを読み込み
    Assimp::Importer importer;
    std::string fullPath = directoryPath + "/" + filename;
    
    const aiScene* scene = importer.ReadFile(fullPath,
        aiProcess_Triangulate |
        aiProcess_FlipUVs
    );
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        OutputDebugStringA(("LoadAnimationFile: Error loading file: " + std::string(importer.GetErrorString()) + "\n").c_str());
        
        // エラーの場合はダミーアニメーションを返す
        animation.duration = 2.0f;
        NodeAnimation dummyNodeAnimation;
        KeyframeVector3 scaleKey;
        scaleKey.time = 0.0f;
        scaleKey.value = { 1.0f, 1.0f, 1.0f };
        dummyNodeAnimation.scale.push_back(scaleKey);
        animation.nodeAnimations["root"] = dummyNodeAnimation;
        return animation;
    }
    
    // アニメーションが存在しない場合
    if (scene->mNumAnimations == 0) {
        OutputDebugStringA("LoadAnimationFile: No animations found in file\n");
        animation.duration = 0.0f;
        return animation;
    }
    
    // 最初のアニメーションを処理
    const aiAnimation* assimpAnimation = scene->mAnimations[0];
    
    OutputDebugStringA(("LoadAnimationFile: Processing animation with " + std::to_string(assimpAnimation->mNumChannels) + " channels\n").c_str());
    
    // アニメーション時間を設定
    animation.duration = static_cast<float>(assimpAnimation->mDuration / assimpAnimation->mTicksPerSecond);
    animation.nodeAnimations.clear();
    
    // 各ノードアニメーションチャンネルを処理
    for (unsigned int i = 0; i < assimpAnimation->mNumChannels; i++) {
        const aiNodeAnim* nodeAnim = assimpAnimation->mChannels[i];
        std::string nodeName = nodeAnim->mNodeName.C_Str();
        
        NodeAnimation& nodeAnimation = animation.nodeAnimations[nodeName];
        
        // 位置キーフレーム（右手座標系→左手座標系：X座標を反転）
        for (unsigned int j = 0; j < nodeAnim->mNumPositionKeys; j++) {
            const aiVectorKey& key = nodeAnim->mPositionKeys[j];
            KeyframeVector3 keyframe;
            keyframe.time = static_cast<float>(key.mTime / assimpAnimation->mTicksPerSecond);
            keyframe.value = {-key.mValue.x, key.mValue.y, key.mValue.z};  
            nodeAnimation.translate.push_back(keyframe);
        }
        
        // 回転キーフレーム（右手座標系→左手座標系）
        for (unsigned int j = 0; j < nodeAnim->mNumRotationKeys; j++) {
            const aiQuatKey& key = nodeAnim->mRotationKeys[j];
            KeyframeQuaternion keyframe;
            keyframe.time = static_cast<float>(key.mTime / assimpAnimation->mTicksPerSecond);
            

            // Y,Z成分を反転
            keyframe.value = {key.mValue.x, -key.mValue.y, -key.mValue.z, key.mValue.w};
            nodeAnimation.rotate.push_back(keyframe);
        }
        
        // スケールキーフレーム（スケールは座標系に依存しない）
        for (unsigned int j = 0; j < nodeAnim->mNumScalingKeys; j++) {
            const aiVectorKey& key = nodeAnim->mScalingKeys[j];
            KeyframeVector3 keyframe;
            keyframe.time = static_cast<float>(key.mTime / assimpAnimation->mTicksPerSecond);
            keyframe.value = {key.mValue.x, key.mValue.y, key.mValue.z};
            nodeAnimation.scale.push_back(keyframe);
        }
        
       ///OutputDebugStringA(("LoadAnimationFile: Node " + nodeName + " - Position keys: " + std::to_string(nodeAnimation.translate.size()) + 
       ///                  ", Rotation keys: " + std::to_string(nodeAnimation.rotate.size()) + 
       ///                  ", Scale keys: " + std::to_string(nodeAnimation.scale.size()) + "\n").c_str());
    }
    
    ///OutputDebugStringA(("LoadAnimationFile: Animation duration: " + std::to_string(animation.duration) + " seconds\n").c_str());
    
    return animation;
}