#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/Math.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"


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

	void SubsystemCamera::Update(SubsystemContext& ctx, float dt)
	{
		static float sens = 1.f;
		static float speed = 10.f;
		auto cameraEntity = m_gameWorld.view<ComponentCamera>().front();
		auto& camera = m_gameWorld.get<ComponentCamera>(cameraEntity);
		float dx = ctx.input.GetMouseDX() * sens * dt;
		float dy = -ctx.input.GetMouseDY() * sens * dt;

		float dwx = 0;
		float dwz = 0;
		if (ctx.input.IsKeyDown(KeyCode::KY_A))
			dwx += dt * speed;
		if (ctx.input.IsKeyDown(KeyCode::KY_D))
			dwx -= dt * speed;
		if (ctx.input.IsKeyDown(KeyCode::KY_W))
			dwz += dt * speed;
		if (ctx.input.IsKeyDown(KeyCode::KY_S))
			dwz -= dt * speed;
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
