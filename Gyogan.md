# 魚眼レンズ実装

## 頂点シェーダー

### HLSL
```hlsl
struct CameraData {
    float fisheyeStrength;
};

output.position = mul(input.position, gTransformationMatrix.WVP);

if (gCameraData.fisheyeStrength > 0.0)
{
    float2 ndc = output.position.xy / output.position.w;
    float r = length(ndc);
    float distortion = tanh(r * gCameraData.fisheyeStrength * 0.5) / (r + 0.0001);
    output.position.xy = ndc * distortion * output.position.w;
}
```

### C++
```cpp
// Camera.h
class Camera {
private:
    float fisheyeStrength_ = 0.0f;

public:
    void SetFisheyeStrength(float strength) { fisheyeStrength_ = strength; }
    float GetFisheyeStrength() const { return fisheyeStrength_; }
};

// Object3d.h
struct CameraData {
    Vector3 worldPosition;
    float fisheyeStrength;
};

// Object3d.cpp 初期化
cameraData_->fisheyeStrength = 0.0f;

// Object3d.cpp 更新
cameraData_->fisheyeStrength = camera_->GetFisheyeStrength();
```

---

## ピクセルシェーダー

### HLSL
```hlsl
cbuffer HorrorParams : register(b0)
{
    float32_t time;
    float32_t noiseIntensity;
    float32_t distortionAmount;
    float32_t bloodAmount;
    float32_t vignetteIntensity;
    float32_t fisheyeStrength;
    float32_t fisheyeRadius;
    float32_t padding;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float32_t2 uv = input.texcoord;

    if (fisheyeStrength > 0.0)
    {
        float32_t2 xy = (uv - 0.5) * 2.0;
        float d = length(xy);

        float aperture = 150.0 + fisheyeStrength * 28.0;
        aperture = min(aperture, 179.9);
        float apertureHalf = 0.5 * aperture * (3.14159265 / 180.0);
        float maxFactor = sin(apertureHalf);

        if (d < fisheyeRadius)
        {
            float scaledD = d * maxFactor / fisheyeRadius;
            scaledD = min(scaledD, 0.99);
            float z = sqrt(1.0 - scaledD * scaledD);
            float r = atan2(scaledD, z) / 3.14159265;
            float phi = atan2(xy.y, xy.x);

            uv.x = r * cos(phi) * (fisheyeRadius / maxFactor) + 0.5;
            uv.y = r * sin(phi) * (fisheyeRadius / maxFactor) + 0.5;
        }
    }

    float aberrationAmount = 1.0 + distortionAmount * 2.0;
    float32_t3 color = chromaticAberration(uv, aberrationAmount);

    output.color = float32_t4(color, 1.0);
    return output;
}
```

### C++
```cpp
// PostProcess.h
struct HorrorParams {
    float time;
    float noiseIntensity;
    float distortionAmount;
    float bloodAmount;
    float vignetteIntensity;
    float fisheyeStrength;
    float fisheyeRadius;
    float padding;
};

class PostProcess {
private:
    HorrorParams currentParams_{};
    HorrorParams* horrorParamsData_ = nullptr;

public:
    void SetFisheyeStrength(float strength) {
        currentParams_.fisheyeStrength = strength;
        if (horrorParamsData_) {
            *horrorParamsData_ = currentParams_;
        }
    }

    void SetFisheyeRadius(float radius) {
        currentParams_.fisheyeRadius = radius;
        if (horrorParamsData_) {
            *horrorParamsData_ = currentParams_;
        }
    }
};

// PostProcess.cpp 初期化
currentParams_.fisheyeStrength = 0.0f;
currentParams_.fisheyeRadius = 1.5f;
```

---

## 使い方

```cpp
// GamePlayScene.h
float fisheyeStrength_ = 2.58f;
float fisheyeRadius_ = 1.5f;

// GamePlayScene.cpp
#ifdef _DEBUG
    ImGui::SliderFloat("Fisheye Strength", &fisheyeStrength_, 0.0f, 100.0f);
    ImGui::SliderFloat("Fisheye Radius", &fisheyeRadius_, 0.1f, 3.0f);
#endif

postProcess_->SetFisheyeStrength(fisheyeStrength_);
postProcess_->SetFisheyeRadius(fisheyeRadius_);
```
