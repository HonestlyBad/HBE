#pragma once
#include "HBE/ECS/Entity.h"

#include <functional>
#include <string>

namespace HBE::ECS {

	// Simple AABB collider (2D)
	struct Collider2D {
		float halfW = 0.5f;
		float halfH = 0.5f;
		float offsetX = 0.0f;
		float offsetY = 0.0f;

		bool isTrigger = false;
	};

	// Simple rigidbody (2D)
	struct RigidBody2D {
		float velX = 0.0f;
		float velY = 0.0f;
		
		float accelX = 0.0f;
		float accelY = 0.0f;

		float linearDamping = 0.0f; // 0 = none
		bool isStatic = false;
	};

	// script hook
	struct Script {
		std::string name;

		// Called once after attaching script to an entity (optional)
		std::function<void(Entity)> onCreate;

		// Called every frame (optional)
		std::function<void(Entity, float)> onUpdate;
	};
}