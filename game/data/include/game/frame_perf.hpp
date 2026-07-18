#pragma once

namespace game
{
struct FramePerfStatus
{
    float inputProcessingTime;
    float logicTime;
    float assetUploadTime;
    float renderSubmitTime;
    float cpuUsage;

    float actualFrameTime() const { return inputProcessingTime + logicTime + assetUploadTime + renderSubmitTime; }

    FramePerfStatus operator+(const FramePerfStatus& other) const
    {
        return FramePerfStatus{
            inputProcessingTime + other.inputProcessingTime,
            logicTime + other.logicTime,
            assetUploadTime + other.assetUploadTime,
            renderSubmitTime + other.renderSubmitTime,
            cpuUsage + other.cpuUsage,
        };
    }

    FramePerfStatus operator/(float divisor) const
    {
        return FramePerfStatus{
            inputProcessingTime / divisor,
            logicTime / divisor,
            assetUploadTime / divisor,
            renderSubmitTime / divisor,
            cpuUsage / divisor,
        };
    }
};
}