#pragma once
#include <cinttypes>

namespace OneGame::Engine::Graphics
{
	class ICommandList;

	class IGpuUploader
	{
	public:
		virtual ~IGpuUploader() = default;

		virtual uint32_t UploadGpuObjects(uint32_t frameIndex, uint32_t byteBudget, ICommandList& cmd) noexcept = 0;
		virtual void GarbageCollect(uint32_t frameIndex) noexcept = 0;
	};
}
