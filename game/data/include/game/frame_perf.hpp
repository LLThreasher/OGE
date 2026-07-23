#pragma once

namespace game
{
struct FramePerfStatus
{
    float inputProcessingTime = 0.f;
    float logicTime = 0.f;
    float assetUploadTime = 0.f;
    float renderSubmitTime = 0.f;

    float actualFrameTime() const { return inputProcessingTime + logicTime + assetUploadTime + renderSubmitTime; }

    FramePerfStatus operator+(const FramePerfStatus& other) const
    {
        return FramePerfStatus{
            inputProcessingTime + other.inputProcessingTime,
            logicTime + other.logicTime,
            assetUploadTime + other.assetUploadTime,
            renderSubmitTime + other.renderSubmitTime,
        };
    }

    FramePerfStatus operator/(float divisor) const
    {
        return FramePerfStatus{
            inputProcessingTime / divisor,
            logicTime / divisor,
            assetUploadTime / divisor,
            renderSubmitTime / divisor,
        };
    }
};
}