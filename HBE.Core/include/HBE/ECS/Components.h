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

	// Lightweight rigidbody (2D).
	struct RigidBody2D {
		// Linear velocity
		float velX = 0.0f;
		float velY = 0.0f;

		// Linear acceleration (set by controllers, AI, etc.)
		float accelX = 0.0f;
		float accelY = 0.0f;
		float linearDamping = 0.0f;

		bool isStatic = false;

		// --- Platformer helpers (optional) ---
		bool useGravity = false;
		float gravityScale = 1.0f;

		bool grounded = false;

		float maxStepUp = 0.0f;

		bool enableOneWay = true;
		float oneWayDisableTimer = 0.0f;

		bool enableSlopes = true;

		float maxFallSpeed = 0.0f;
	};

	// script hook
	struct Script {
		std::string name;

		std::function<void(Entity)> onCreate;
		std::function<void(Entity, float)> onUpdate;
	};
}