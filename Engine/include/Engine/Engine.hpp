#pragma once
#include <memory>
#include "Engine/Graphics/IGraphicBackend.hpp"

namespace OneGame::Engine
{

	class Engine
	{
	public:
		void Initialize(Graphics::WindowHandle*);
		void Shutdown();
	private:
		std::unique_ptr<IGraphicsBackend> backend;
		AsyncDispatcher asyncDispatcher;
		JobSystem jobSystem;
	};

}
