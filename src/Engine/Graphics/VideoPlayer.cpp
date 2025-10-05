#include "VideoPlayer.h"
#include "DirectXCommon.h"
#include "SRVManager.h"
#include "d3dx12.h"
#include <mferror.h>
#include <chrono>
#include <cassert>
#include <fstream>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

bool VideoPlayer::isMFInitialized_ = false;
int VideoPlayer::mfRefCount_ = 0;

VideoPlayer::VideoPlayer() {
}

VideoPlayer::~VideoPlayer() {
    FinalizeMediaFoundation();
}

bool VideoPlayer::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    if (!InitializeMediaFoundation()) {
        return false;
    }

    return true;
}

bool VideoPlayer::InitializeMediaFoundation() {
    if (!isMFInitialized_) {
        HRESULT hr = MFStartup(MF_VERSION);
        if (FAILED(hr)) {
            return false;
        }
        isMFInitialized_ = true;
    }
    mfRefCount_++;
    return true;
}

void VideoPlayer::FinalizeMediaFoundation() {
    mfRefCount_--;
    if (mfRefCount_ <= 0 && isMFInitialized_) {
        MFShutdown();
        isMFInitialized_ = false;
        mfRefCount_ = 0;
    }
}

bool VideoPlayer::LoadVideo(const std::string& filePath) {
    // ファイルの存在確認
    std::ifstream file(filePath);
    if (!file.good()) {
        OutputDebugStringA(("Video file not found: " + filePath + "\n").c_str());
        return false;
    }
    file.close();

    // ファイルパスをワイド文字列に変換
    std::wstring wFilePath(filePath.begin(), filePath.end());

    // Source Readerを作成
    Microsoft::WRL::ComPtr<IMFAttributes> attributes;
    HRESULT hr = MFCreateAttributes(&attributes, 1);
    if (FAILED(hr)) {
        return false;
    }

    hr = attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    if (FAILED(hr)) {
        return false;
    }

    hr = MFCreateSourceReaderFromURL(wFilePath.c_str(), attributes.Get(), &sourceReader_);
    if (FAILED(hr)) {
        return false;
    }

    // 最初のビデオストリームを選択
    hr = sourceReader_->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
    if (FAILED(hr)) {
        return false;
    }

    hr = sourceReader_->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
    if (FAILED(hr)) {
        return false;
    }

    // メディアタイプを設定（RGB32形式に変換）
    Microsoft::WRL::ComPtr<IMFMediaType> mediaType;
    hr = MFCreateMediaType(&mediaType);
    if (FAILED(hr)) {
        return false;
    }

    hr = mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (FAILED(hr)) {
        return false;
    }

    hr = mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    if (FAILED(hr)) {
        return false;
    }

    hr = sourceReader_->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, mediaType.Get());
    if (FAILED(hr)) {
        return false;
    }

    // 動画情報を取得
    Microsoft::WRL::ComPtr<IMFMediaType> currentMediaType;
    hr = sourceReader_->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &currentMediaType);
    if (FAILED(hr)) {
        return false;
    }

    UINT32 width, height;
    hr = MFGetAttributeSize(currentMediaType.Get(), MF_MT_FRAME_SIZE, &width, &height);
    if (FAILED(hr)) {
        return false;
    }

    videoWidth_ = width;
    videoHeight_ = height;
    videoSize_ = { static_cast<float>(width), static_cast<float>(height) };

    // フレームレートを取得
    UINT32 numerator, denominator;
    hr = MFGetAttributeRatio(currentMediaType.Get(), MF_MT_FRAME_RATE, &numerator, &denominator);
    if (SUCCEEDED(hr) && denominator > 0) {
        frameRate_ = static_cast<double>(numerator) / static_cast<double>(denominator);
        frameTime_ = 1.0 / frameRate_;
    }

    // テクスチャリソースを作成
    if (!CreateTextureResource()) {
        return false;
    }

    // 最初のフレームを読み込んで初期化
    DWORD streamFlags = 0;
    LONGLONG timestamp = 0;
    Microsoft::WRL::ComPtr<IMFSample> sample;

    hr = sourceReader_->ReadSample(
        (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        0,
        nullptr,
        &streamFlags,
        &timestamp,
        &sample
    );

    if (SUCCEEDED(hr) && sample) {
        if (UpdateTexture(sample.Get())) {
            OutputDebugStringA("First frame loaded and texture transitioned to PIXEL_SHADER_RESOURCE\n");
        } else {
            OutputDebugStringA("Failed to update first frame\n");
            return false;
        }
    } else {
        OutputDebugStringA("Failed to read first frame\n");
        return false;
    }

    return true;
}

bool VideoPlayer::CreateTextureResource() {
    // アップロード用バッファを作成（書き込み可能）
    UINT rowPitch = (videoWidth_ * 4 + 255) & ~255;
    UINT64 uploadBufferSize = rowPitch * videoHeight_;

    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadBufferDesc = {};
    uploadBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadBufferDesc.Width = uploadBufferSize;
    uploadBufferDesc.Height = 1;
    uploadBufferDesc.DepthOrArraySize = 1;
    uploadBufferDesc.MipLevels = 1;
    uploadBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadBufferDesc.SampleDesc.Count = 1;
    uploadBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer_)
    );

    if (FAILED(hr)) {
        return false;
    }

    // テクスチャリソースの設定
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = videoWidth_;
    textureDesc.Height = videoHeight_;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // ヒープの設定
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    // リソースを作成
    hr = dxCommon_->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&textureResource_)
    );

    if (FAILED(hr)) {
        return false;
    }

    // SRVを作成
    srvIndex_ = srvManager_->Allocate();
    srvHandleCPU_ = srvManager_->GetCPUDescriptorHandle(srvIndex_);
    srvHandleGPU_ = srvManager_->GetGPUDescriptorHandle(srvIndex_);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    dxCommon_->GetDevice()->CreateShaderResourceView(textureResource_.Get(), &srvDesc, srvHandleCPU_);

    return true;
}

void VideoPlayer::Update() {
    if (!isPlaying_ || isPaused_ || !sourceReader_) {
        return;
    }

    auto currentTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> deltaTime = currentTime - lastTime_;

    elapsedTime_ += deltaTime.count();
    lastTime_ = currentTime;

    // フレームレートに基づいて更新
    if (elapsedTime_ >= frameTime_) {
        elapsedTime_ = 0.0;

        DWORD streamFlags = 0;
        LONGLONG timestamp = 0;
        Microsoft::WRL::ComPtr<IMFSample> sample;

        HRESULT hr = sourceReader_->ReadSample(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            0,
            nullptr,
            &streamFlags,
            &timestamp,
            &sample
        );

        if (SUCCEEDED(hr)) {
            if (streamFlags & MF_SOURCE_READERF_ENDOFSTREAM) {
                OutputDebugStringA("Video reached end of stream\n");
                isFinished_ = true;
                if (isLoop_) {
                    Stop();
                    Play();
                } else {
                    isPlaying_ = false;
                }
                return;
            }

            if (sample) {
                char debugInfo[256];
                sprintf_s(debugInfo, "About to update texture - isFirstFrame_=%d\n", isFirstFrame_ ? 1 : 0);
                OutputDebugStringA(debugInfo);

                if (UpdateTexture(sample.Get())) {
                    sprintf_s(debugInfo, "Frame updated - isFirstFrame_ after UpdateTexture=%d\n", isFirstFrame_ ? 1 : 0);
                    OutputDebugStringA(debugInfo);
                }
            }
        } else {
            char buffer[256];
            sprintf_s(buffer, "ReadSample failed with HRESULT: 0x%08X\n", hr);
            OutputDebugStringA(buffer);
        }
    }
}

bool VideoPlayer::UpdateTexture(IMFSample* sample) {
    Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = sample->ConvertToContiguousBuffer(&buffer);
    if (FAILED(hr)) {
        return false;
    }

    BYTE* data = nullptr;
    DWORD dataLength = 0;
    hr = buffer->Lock(&data, nullptr, &dataLength);
    if (FAILED(hr)) {
        return false;
    }

    // データをアップロードバッファにコピー
    void* mappedData = nullptr;
    hr = uploadBuffer_->Map(0, nullptr, &mappedData);
    if (SUCCEEDED(hr)) {
        // 行ごとにコピー（RowPitchを考慮）
        UINT rowPitch = (videoWidth_ * 4 + 255) & ~255;
        BYTE* dest = static_cast<BYTE*>(mappedData);
        for (UINT y = 0; y < videoHeight_; ++y) {
            memcpy(dest + y * rowPitch, data + y * videoWidth_ * 4, videoWidth_ * 4);
        }
        uploadBuffer_->Unmap(0, nullptr);
    }

    buffer->Unlock();

    // コマンドリストを取得してテクスチャにコピー
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    // リソースバリア（COPY_DEST状態に遷移）
    // 注意: 初回はリソース作成時にCOPY_DEST状態なのでバリア不要
    // 2回目以降はPIXEL_SHADER_RESOURCE状態からCOPY_DESTに遷移
    if (!isFirstFrame_) {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = textureResource_.Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        commandList->ResourceBarrier(1, &barrier);
    }

    // テクスチャにコピー
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    footprint.Offset = 0;
    footprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    footprint.Footprint.Width = videoWidth_;
    footprint.Footprint.Height = videoHeight_;
    footprint.Footprint.Depth = 1;
    footprint.Footprint.RowPitch = (videoWidth_ * 4 + 255) & ~255;

    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = textureResource_.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = uploadBuffer_.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint = footprint;

    commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    // リソースバリア（PIXEL_SHADER_RESOURCE状態に遷移）
    D3D12_RESOURCE_BARRIER barrier2 = {};
    barrier2.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier2.Transition.pResource = textureResource_.Get();
    barrier2.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier2.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier2.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier2.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    commandList->ResourceBarrier(1, &barrier2);

    isFirstFrame_ = false;

    char debugMsg[256];
    sprintf_s(debugMsg, "Texture copy command recorded - isFirstFrame BEFORE set to false: %d\n", isFirstFrame_ ? 1 : 0);
    OutputDebugStringA(debugMsg);

    return true;
}

void VideoPlayer::Play() {
    if (!sourceReader_) {
        return;
    }

    isPlaying_ = true;
    isPaused_ = false;
    isFinished_ = false;
    isFirstFrame_ = true;
    lastTime_ = std::chrono::high_resolution_clock::now();
}

void VideoPlayer::Pause() {
    isPaused_ = !isPaused_;
}

void VideoPlayer::Stop() {
    if (!sourceReader_) {
        return;
    }

    isPlaying_ = false;
    isPaused_ = false;
    isFinished_ = false;
    elapsedTime_ = 0.0;

    // 動画を最初から再生するために、Source Readerの位置をリセット
    PROPVARIANT var;
    PropVariantInit(&var);
    var.vt = VT_I8;
    var.hVal.QuadPart = 0;
    sourceReader_->SetCurrentPosition(GUID_NULL, var);
    PropVariantClear(&var);
}
