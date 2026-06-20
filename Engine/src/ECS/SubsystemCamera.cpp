#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/Math.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

namespace OneGame::Engine::ECS
{
	struct ComponentCamera
	{
		float yaw;
		float pitch;
		math::vec3 position;
		glm::vec3 forward;

		math::mat4 view() const
		{
			return math::lookAt(position, position + forward, glm::vec3(0, 1, 0));;
		}

		void ApplyDelta(float dsx, float dsy, float dwx, float dwz)
		{
			yaw += dsx;
			pitch += dsy;
			pitch = math::clamp(pitch, -math::radians(89.0f), math::radians(89.0f));

			forward.x = cos(pitch) * sin(yaw);
			forward.y = sin(pitch);
			forward.z = cos(pitch) * cos(yaw);
			forward = math::normalize(forward);

			glm::vec3 worldUp = { 0, 1, 0 };

			// Horizontal movement direction
			//glm::vec3 flatForward = forward;
			//flatForward.y = 0.0f;
			//flatForward = glm::normalize(flatForward);

			//glm::vec3 right = glm::normalize(glm::cross(flatForward, worldUp));

			//position += dwx * right + dwz * flatForward;

			glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));

			position += dwx * math::cross(forward, glm::vec3(0, 1, 0)) + dwz * forward;
		}
	};

	void SubsystemCamera::Initialize(SubsystemInitContext& ctx)
	{
		auto camera = m_gameWorld.create();
		ComponentCamera& cam = m_gameWorld.emplace<ComponentCamera>(camera);

		cam.position = { 20.f, 20.f, 20.f };

		glm::vec3 target = { 0.f, 0.f, 0.f };
		cam.forward = glm::normalize(target - cam.position);

		cam.yaw = std::atan2(cam.forward.x, cam.forward.z);
		cam.pitch = std::asin(cam.forward.y);
	}

	void SubsystemCamera::Update(SubsystemContext& ctx)
	{
		float dt = ctx.dt;
		auto cameraEntity = m_gameWorld.view<ComponentCamera>().front();
		auto& camera = m_gameWorld.get<ComponentCamera>(cameraEntity);
		float dx = 0;
		float dy = 0;
		float width = ctx.backend.SwapchainExtend().x;
#ifdef PLATFORM_ANDROID
		static float speed = 20.f;
		static float sens = 1.f;
		if (panFingerIndex != -1)
		{
			dx = ctx.input.GetTouchDX(panFingerIndex) * sens;
			dy = -ctx.input.GetTouchDY(panFingerIndex) * sens;
			if ((ctx.input.GetReleasedTouchIdMask() & (1 << panFingerIndex)) != 0)
			{
				LOG_DEBUG("pan up {}", panFingerIndex);
				panFingerIndex = -1;
			}
		}
		if (ctx.input.GetPressedTouchIdMask() != 0)
		{
			auto fingerIdx = __builtin_ctz(ctx.input.GetPressedTouchIdMask());
			auto fingerX = ctx.input.GetTouchX(fingerIdx);
			auto fingerY = ctx.input.GetTouchY(fingerIdx);
			if (fingerX < 0.5)
			{
				if (moveFingerIndex == -1)
				{
					moveFingerIndex = fingerIdx;
					moveFingerStartX = fingerX;
					moveFingerStartY = fingerY;
					//LOG_DEBUG("move down {} {},{}", moveFingerIndex, fingerX, fingerY);
				}
			}
			else
			{
				if (panFingerIndex == -1)
				{
					panFingerIndex = fingerIdx;
					//LOG_DEBUG("pan down {} {},{}", panFingerIndex, fingerX, fingerY);
				}
			}
		}
#else
		static float speed = 10.f;
		static float sens = 0.001f;
		dx = ctx.input.GetMouseDX() * sens;
		dy = -ctx.input.GetMouseDY() * sens;
#endif

		float dwx = 0;
		float dwz = 0;
#ifdef PLATFORM_ANDROID
		if (moveFingerIndex != -1)
		{
			dwx += -(ctx.input.GetTouchX(moveFingerIndex) - moveFingerStartX) * dt * speed;
			dwz += -(ctx.input.GetTouchY(moveFingerIndex) - moveFingerStartY) * dt * speed;
			if ((ctx.input.GetReleasedTouchIdMask() & (1 << moveFingerIndex)) != 0)
			{
				//LOG_DEBUG("move up {}", moveFingerIndex);
				moveFingerIndex = -1;
			}
		}
#else
		if (ctx.input.IsKeyDown(KeyCode::KY_A))
			dwx += dt * speed;
		if (ctx.input.IsKeyDown(KeyCode::KY_D))
			dwx -= dt * speed;
		if (ctx.input.IsKeyDown(KeyCode::KY_W))
			dwz += dt * speed;
		if (ctx.input.IsKeyDown(KeyCode::KY_S))
			dwz -= dt * speed;
#endif
		camera.ApplyDelta(dx, dy, dwx, dwz);
	}

	void SubsystemCamera::Present(entt::registry& presentationWorld)
	{
		auto e = presentationWorld.create();
		auto& view = presentationWorld.emplace<Graphics::PViewTransform>(e);

		auto cameraEntity = m_gameWorld.view<ComponentCamera>().front();
		auto& camera = m_gameWorld.get<ComponentCamera>(cameraEntity);

		view.view = camera.view();
	}
}
