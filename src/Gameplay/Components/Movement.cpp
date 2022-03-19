#include "Gameplay/Components/Movement.h"
#include <GLFW/glfw3.h>
#include "Gameplay/GameObject.h"
#include "Gameplay/Scene.h"
#include "Utils/ImGuiHelper.h"
#include "Application/Application.h"

Movement::Movement():

	IComponent(),
		_impulse(0.1f)
{ }

Movement::~Movement() = default;

void Movement::Awake()
{
	_body = GetComponent<Gameplay::Physics::RigidBody>(); //Gets the Physics Body that it's attached to
	if (_body == nullptr) {
		IsEnabled = false;
	}
}

void Movement::Update(float deltaTime)
{
	Application& app = Application::Get();
	is_moving = false;

	if (glfwGetKey(app.GetWindow(), GLFW_KEY_A) && GLFW_PRESS) {
		if (_body->GetLinearVelocity().x <= 20.0f) {
			_body->ApplyImpulse(glm::vec3(-_impulse, 0.0f, 0.0f));
			is_moving = true;
			GetGameObject()->SetRotation(glm::vec3(90.f, 0.f, 0.f));
			
		}
	}

	if (glfwGetKey(app.GetWindow(),GLFW_KEY_D) && GLFW_PRESS) {
		if (_body->GetLinearVelocity().x >= -20.0f) {
			_body->ApplyImpulse(glm::vec3(_impulse, 0.0f, 0.0f));
			is_moving = true;
			GetGameObject()->SetRotation(glm::vec3(90.f, 0.f, 0.f));
		
		}
	}
}

void Movement::RenderImGui()
{
	return;
}

nlohmann::json Movement::ToJson() const
{
	return {
	 {"moving", is_moving}
	};
}

Movement::Sptr Movement::FromJson(const nlohmann::json& blob)
{
	Movement::Sptr result = std::make_shared<Movement>();
	result->is_moving = blob["moving"];
	return result;
}
