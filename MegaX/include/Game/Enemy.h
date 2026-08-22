#pragma once

#include "HBE/Renderer/Sprite2D.h"
#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/TileCollision.h"

namespace HBE::Renderer {
	class ResourceCache;
	class Mesh;
	class GLShader;
	class Renderer2D;
}

namespace MegaX {

	class Player;
	struct DifficultyProfile;

	class Enemy {
	public:
		enum class AIState {
			Patrol,
			Suspicious,
			Alert,
			Chase,
			Search,
			Return
		};

		enum class BubbleIcon {None, Question, Exclaim };

		bool init(HBE::Renderer::ResourceCache& resources, HBE::Renderer::Mesh* quadMesh, HBE::Renderer::GLShader* spriteShader);

		void spawn(float x, float groundY, int facing);
		void setPatrolPath(float leftX, float rightX, float waitSec = 1.0f);
		void setCollision(const HBE::Renderer::TileMap* map, const HBE::Renderer::TileMapLayer* solidLayer) {
			m_map = map;
			m_solid = solidLayer;
		}

		void fixedTick(float h, const Player& player);
		void updateVisual(float dt);

		void render(HBE::Renderer::Renderer2D& r2d, float alpha);

		void onHeardGunshot(float sourceX, float sourceY);

		void applyDifficulty(const DifficultyProfile& p);

		void snapshotBaseStats();

		void muzzleWorldPos(float& mx, float& my) const;

		using FireFn = void(*)(void* ctx, float sx, float sy,
		                       float aimX, float aimY,
		                       float speed, int damage);
		void setFireCallback(FireFn fn, void* ctx) { m_fireFn = fn; m_fireCtx = ctx; }

		bool takeDamage(int amount);

		HBE::Renderer::AABB hurtbox() const;
		HBE::Renderer::AABB hitbox() const;

		bool hurtboxActive() const { return !m_dead; }
		bool hitboxActive() const { return m_hitboxActive && !m_dead; }

		bool isAlive() const { return !m_dead; }
		bool isDying() const { return m_dead && m_deathTimer > 0.0f; }
		bool isFinished() const { return m_dead && m_deathTimer <= 0.0f; }

		bool landedThisFrame() const { return m_landedThisFrame; }
		
		int groundTileId() const { return m_groundTileId; }

		bool justDied() const { return m_justDied; }
		void consumeJustDied() { m_justDied = false; }

		bool consumeWalkDustPuff(float dt, float period);

		int hp() const { return m_hp; }
		int maxHp() const{ return m_maxHp; }

		AIState aiState() const { return m_state; }
		BubbleIcon bubbleIcon() const;
		int facing() const { return m_facing; }
		float x() const { return m_x; }
		float y() const { return m_y; }
		float feetY() const { return m_feetY; }

		float eyeX() const { return m_x; }
		float eyeY() const { return m_feetY + eyeHeightAboveFeet; }

		float visionRange() const { return sightRange; }
		float visionHalfAngle() const { return sightHalfAngleDeg; }
		float hearingRingRadius() const { return hearingRadius; }

		int startHp = 3;
		int damagePerHit = 1;
		float invulnAfterHit = 0.08f;
		float hitFlashTime = 0.10f;
		float deathFadeTime = 0.60f;

		float hurtHalfW = 11.0f;
		float hurtHalfH = 18.0f;
		float hurtOffsetX = 0.0f;
		float hurtOffsetY = 0.0f;

		float hitHalfW = 20.0f;
		float hitHalfH = 14.0f;
		float hitOffsetX = 20.0f;
		float hitOffsetY = 0.0f;
		int hitDamage = 1;

		float moveSpeed = 60.0f;
		float chaseSpeed = 120.0f;
		float gravity = 2100.0f;
		float jumpSpeed = 520.0f;
		float maxFall = 900.0f;
		float chaseJumpCooldown = 0.5f;

		float boxHalfW = 12.0f;
		float boxHalfH = 20.0f;

		float spriteFeetOffsetY = 0.0f;

		float patrolWaitAtEnd = 1.0f;

		float hearingRadius = 70.0f;
		float minPlayerVxToHear = 40.0f;
		float gunshotHearRadius = 140.0f;

		float sightRange = 260.0f;
		float sightHalfAngleDeg = 35.0f;
		float eyeHeightAboveFeet = 32.0f;
		float shootingRange = 360.0f;
		float standoffFrac = 0.5f;
		float standoffDeadzone = 16.0f;

		int   bulletDamage    = 1;
		float fireCooldownSec = 0.90f;
		float bulletSpeed     = 480.0f;
		float leadFactor      = 0.0f;
		float muzzleForwardX  = 22.0f;
		float muzzleAboveFeet = 26.0f;
		float shootingLosStep = 12.0f;

		float suspicionDuration = 1.20f;
		float alertLatchTime = 0.20f;
		float loseAggroDelay = 3.00f;
		float searchDwellTime = 2.50f;

		float losStepPx = 12.0f;

		private:
			enum class AnimState {Idle, Walk};
			void setAnimState(AnimState s);
			HBE::Renderer::SpriteAnimation& currentAnim();

			void applyPhysics(float dt);
			void syncFromBox();

			bool hearsPlayer(const Player& p) const;
			bool seesPlayer(const Player& p) const;
			bool losClear(float ax, float ay, float bx, float by) const;
			bool isSolidTileAt(float wx, float wy) const;

			bool wallInFront() const;
			bool ledgeInFront() const;
			int desiredPatrolFacing() const;

			void enter(AIState s);
			void faceX(float targetX);

			void tickPatrol(float dt, const Player& player);
			void tickSuspicious(float dt, const Player& player);
			void tickAlert(float dt, const Player& player);
			void tickChase(float dt, const Player& player);
			void tickSearch(float dt, const Player& player);
			void tickReturn(float dt, const Player& player);
			void tickShooting(float dt, const Player& player);

			float m_x = 0.0f;
			float m_y = 0.0f;
			float m_prevX = 0.0f;
			float m_prevY = 0.0f;
			float m_feetY = 0.0f;

			int m_facing = -1;

			HBE::Renderer::AABB m_box{};
			float m_vx = 0.0f;
			float m_vy = 0.0f;
			bool m_grounded = false;
			float m_prevBottom = 0.0f;

			int m_hp = 3;
			int m_maxHp = 3;
			float m_invulnTimer = 0.0f;
			float m_flashTimer = 0.0f;
			float m_deathTimer = 0.0f;
			bool m_dead = false;
			bool m_hitboxActive = false;

			AIState m_state = AIState::Patrol;
			float m_stateTimer = 0.0f;
			float m_alertLatch = 0.0f;
			float m_hiddenTimer = 0.0f;
			float m_jumpCooldown = 0.0f;
			bool m_lastHeard = false;
			bool m_lastSeen = false;
			float m_lastKnownX = 0.0f;
			float m_lastKnownY = 0.0f;

			float m_patrolLeftX = 0.0f;
			float m_patrolRightX = 0.0f;
			int m_patrolTargetSign = -1;
			float m_patrolWaitTimer = 0.0f;

			const HBE::Renderer::TileMap* m_map = nullptr;
			const HBE::Renderer::TileMapLayer* m_solid = nullptr;

			HBE::Renderer::SpriteSheet m_idleSheet{};
			HBE::Renderer::SpriteSheet m_walkSheet{};
			HBE::Renderer::SpriteAnimation m_idleAnim;
			HBE::Renderer::SpriteAnimation m_walkAnim;
			AnimState m_animState = AnimState::Idle;

			HBE::Renderer::Material m_material{};
			HBE::Renderer::RenderItem m_item{};

			float  m_fireCooldown = 0.0f;
			FireFn m_fireFn  = nullptr;
			void*  m_fireCtx = nullptr;

			float m_baseChaseSpeed     = 0.0f;
			float m_baseSightRange     = 0.0f;
			float m_baseHearingRadius  = 0.0f;
			float m_baseLoseAggroDelay = 0.0f;
			int   m_baseStartHp        = 0;

			bool m_landedThisFrame = false;
			bool m_wasGroundedLast = true;
			int m_groundTileId = 0;
			bool m_justDied = false;
			float m_walkDustAccum = 0.0f;
	};
}