#pragma once
#include "Gameplay/Components/IComponent.h"
#include "Gameplay/Physics/RigidBody.h"
class Movement : public Gameplay::IComponent
{
public:
	typedef std::shared_ptr<Movement> Sptr;

	Movement();
	virtual ~Movement();

	virtual void Awake() override;
	virtual void Update(float deltaTime) override;

public:
	virtual void RenderImGui() override;
	MAKE_TYPENAME(Movement);
	virtual nlohmann::json ToJson() const override;
	static Movement::Sptr FromJson(const nlohmann::json& blob);

	bool is_moving = false;


protected:

	float _impulse;
	Gameplay::Physics::RigidBody::Sptr _body;
	float speed = 5.0f;
};

