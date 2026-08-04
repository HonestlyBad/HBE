#include "Game/Player.h"

#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Core/AssetPaths.h"
#include "HBE/Core/Log.h"

#include <algorithm>
#include <cmath>

using namespace HBE::Renderer;

namespace MegaX {
		
	static constexpr int kFrameW = 75;
	static constexpr int kFrameH = 48;
	static constexpr float kPixelScale = 1.0f;

	static constexpr int kIdleRow = 3, kIdleCol0 = 0, kIdleCol1 = 3;
	static constexpr int kWalkRow = 6, kWalkCol0 = 0, kWalkCol1 = 9;
	static constexpr int kCrouchRow = 1, kCrouchCol0 = 0, kCrouchCol1 = 5;
	static constexpr int kJumpRow = 13;
	static constexpr int kRiseCol0 = 0, kRiseCol1 = 2;
	static constexpr int kFallCol0 = 3, kFallCol1 = 3;
	static constexpr int kLandCol0 = 4, kLandCol1 = 5;

	static constexpr int kShootStandRow = 10, kShootStandCol0 = 0, kShootStandCol1 = 1;
	static constexpr int kShootWalkRow  = 5,  kShootWalkCol0  = 0, kShootWalkCol1  = 9;
	static constexpr int kShootAirRow   = 8,  kShootAirCol0   = 0, kShootAirCol1   = 2;

	static constexpr float kIdleFps   = 8.0f;
	static constexpr float kWalkFps   = 14.0f;
	static constexpr float kCrouchFps = 10.0f;
	static constexpr float kJumpFps   = 12.0f;
	static constexpr float kShootFps  = 14.0f;
		
	static constexpr float kBoxW       = 24.0f;
	static constexpr float kBoxStandH  = 40.0f;
	static constexpr float kBoxCrouchH = 24.0f;

	static constexpr float kCrouchSpeedMul = 0.5f;
	static constexpr float kLandTime = 0.16f;

	static constexpr float kCoyote  = 0.08f;
	static constexpr float kJumpBuf = 0.10f;
	static constexpr float kJumpCut = 180.0f;
		
	static constexpr float kFireCooldown = 0.15f;
	static constexpr float kShootHold    = 0.22f;
	static constexpr float kRecoilImpulse = 120.0f;
	static constexpr float kRecoilDamp    = 7.0f;

	static constexpr float kMuzzleFwdStand = 30.0f, kMuzzleUpStand = 1.0f;
	static constexpr float kMuzzleFwdAir   = 30.0f, kMuzzleUpAir   = 2.0f;
	static constexpr float kMuzzleFwdCrouch = 28.0f, kMuzzleUpCrouch = -14.0f;
	static constexpr float kMuzzleFwdWalk  = 30.0f, kMuzzleUpWalk  = kMuzzleUpStand - 4.0f;

	static float feetToCenterOffset(float boxH) {
		return kFrameH * kPixelScale * 0.5f - boxH * 0.5f;
	}

	bool Player::init(ResourceCache& resources, Mesh* quadMesh, GLShader* spriteShader) {
		m_helmSheet = SpriteRenderer2D::DeclareSpriteSheet(
			resources, "player_helm",
			HBE::Core::AssetPaths::Resolve("sprites/Player/player.png"),
			kFrameW, kFrameH);

		m_noHelmSheet = SpriteRenderer2D::DeclareSpriteSheet(
			resources, "player_no_helm",
			HBE::Core::AssetPaths::Resolve("sprites/Player/player-no-helm.png"),
			kFrameW, kFrameH);

		if (!m_helmSheet.isValid() || !m_noHelmSheet.isValid()) {
			HBE::Core::LogError("Player::init: failed to load player sprite sheet(s). "
				"Asset root: " + HBE::Core::AssetPaths::AssetRootString());
			return false;
		}

		m_activeSheet = m_helmet ? m_helmSheet : m_noHelmSheet;
		m_idleAnim   = SpriteAnimation(&m_activeSheet, kIdleCol0,   kIdleCol1,   kIdleRow,   kIdleFps,   true);
		m_walkAnim   = SpriteAnimation(&m_activeSheet, kWalkCol0,   kWalkCol1,   kWalkRow,   kWalkFps,   true);
		m_crouchAnim = SpriteAnimation(&m_activeSheet, kCrouchCol0, kCrouchCol1, kCrouchRow, kCrouchFps, true);
		m_riseAnim   = SpriteAnimation(&m_activeSheet, kRiseCol0,   kRiseCol1,   kJumpRow,   kJumpFps,   false);
		m_fallAnim   = SpriteAnimation(&m_activeSheet, kFallCol0,   kFallCol1,   kJumpRow,   kJumpFps,   false);
		m_landAnim   = SpriteAnimation(&m_activeSheet, kLandCol0,   kLandCol1,   kJumpRow,   kJumpFps,   false);

		m_shootStandAnim = SpriteAnimation(&m_activeSheet, kShootStandCol0, kShootStandCol1, kShootStandRow, kShootFps, true);
		m_shootWalkAnim  = SpriteAnimation(&m_activeSheet, kShootWalkCol0,  kShootWalkCol1,  kShootWalkRow,  kWalkFps,  true);
		m_shootAirAnim   = SpriteAnimation(&m_activeSheet, kShootAirCol0,   kShootAirCol1,   kShootAirRow,   kShootFps, true);

		m_material.shader  = spriteShader;
		m_material.texture = m_activeSheet.texture;

		m_item.mesh = quadMesh;
		m_item.material = &m_material;
		m_item.layer = 100;
		m_item.pass = RenderPass::World;
		m_item.transform.scaleX = kFrameW * kPixelScale;
		m_item.transform.scaleY = kFrameH * kPixelScale;

		m_box.w = kBoxW;
		m_box.h = kBoxStandH;

		m_animState = 0;
		m_idleAnim.play(true);
		SpriteRenderer2D::SetStaticSpriteFrame(m_item, m_activeSheet, kIdleCol0, kIdleRow);
		return true;
	}

	void Player::setHelmet(bool on) {
		m_helmet = on;
		m_activeSheet = on ? m_helmSheet : m_noHelmSheet;
		m_material.texture = m_activeSheet.texture;
	}

	void Player::setPosition(float x, float y) {
		m_hp = startHp;
		m_invulnTimer = 0.0f;
		m_hurtFlashTimer = 0.0f;
		m_x = x; m_y = y;
		m_vx = m_vy = 0.0f;
		m_grounded = false;
		m_crouching = false;
		m_coyote = m_jumpBuf = 0.0f;
		m_landTimer = 0.0f;

		m_box.w = kBoxW;
		m_box.h = kBoxStandH;
		m_box.cx = x;
		m_box.cy = y - feetToCenterOffset(m_box.h);   // inverse of render sync
	}

	void Player::resetForRespawn() {
		m_hp = startHp;
		m_invulnTimer = 0.0f;
		m_hurtFlashTimer = 0.0f;
		m_vx = 0.0f;
		m_vy = 0.0f;
		m_inX = 0.0f;
		m_inY = 0.0f;
		m_grounded = false;
		m_crouching = false;
		m_coyote = 0.0f;
		m_jumpBuf = 0.0f;
		m_landTimer = 0.0f;
		m_landedThisFrame = false;
		m_groundTileId = 0;
		m_jumpPressed = false;
		m_jumpHeld = false;
		m_crouchHeld = false;
		m_firePressed = false;
		m_fireHeld = false;
		m_fireCooldown = 0.0f;
		m_shootTimer = 0.0f;
		m_recoilVx = 0.0f;
		m_shotPending = false;
		m_shotX = 0.0f;
		m_shotY = 0.0f;
		m_shotDir = m_facing;
		m_animState = -1;
	}

	bool Player::boxOverlapsSolid(const AABB& b) const {
		if (!m_map || !m_collLayer) return false;

		const float tw = m_map->worldTileW();
		const float th = m_map->worldTileH();
		const float minX = b.cx - b.w * 0.5f, maxX = b.cx + b.w * 0.5f;
		const float minY = b.cy - b.h * 0.5f, maxY = b.cy + b.h * 0.5f;

		const int minTX = (int)std::floor(minX / tw);
		const int maxTX = (int)std::floor((maxX - 0.001f) / tw);
		const int minTY = (int)std::floor(minY / th);
		const int maxTY = (int)std::floor((maxY - 0.001f) / th);

		const auto& ts = m_map->tilesets[m_collLayer->tilesetIndex];
		for (int ty = minTY; ty <= maxTY; ++ty)
			for (int tx = minTX; tx <= maxTX; ++tx) {
				const int id = m_collLayer->at(tx, ty);
				if (id != 0 && ts.isSolid(id)) return true;
			}
		return false;
	}

	void Player::syncRenderFromBox() {
		m_x = m_box.cx;
		m_y = m_box.cy + feetToCenterOffset(m_box.h);
	}

	SpriteAnimation& Player::animForState(int s) {
		switch (s) {
			case 1: return m_walkAnim;
			case 2: return m_crouchAnim;
			case 3: return m_riseAnim;
			case 4: return m_fallAnim;
			case 5: return m_landAnim;
			case 6: return m_shootStandAnim;
			case 7: return m_shootWalkAnim;
			case 8: return m_shootAirAnim;
			default: return m_idleAnim;
		}
	}

	bool Player::consumeShot(float& x, float& y, int& dir) {
		if (!m_shotPending) return false;
		x = m_shotX;
		y = m_shotY;
		dir = m_shotDir;
		m_shotPending = false;
		return true;
	}

	void Player::setAnimState(int s) {
		if (s == m_animState) return;
		m_animState = s;
		animForState(s).play(true);
	}

	void Player::update(float dt) {
		if (m_invulnTimer > 0.0f) m_invulnTimer = std::max(0.0f, m_invulnTimer - dt);
		if (m_hurtFlashTimer > 0.0f) m_hurtFlashTimer = std::max(0.0f, m_hurtFlashTimer - dt);

		if (m_mode == Mode::Ghost) updateGhost(dt);
		else                       updatePlay(dt);

		m_jumpPressed = false;
		m_firePressed = false;

		m_item.transform.posX = m_x;
		m_item.transform.posY = m_y;
		m_item.transform.scaleX = kFrameW * kPixelScale * static_cast<float>(m_facing);
		m_item.transform.scaleY = kFrameH * kPixelScale;

		if (m_mode == Mode::Ghost) {
			m_item.tint = Color4{ 0.6f, 0.8f, 1.0f, 0.5f };
		}
		else if (m_hurtFlashTimer > 0.0f) {
			const float k = m_hurtFlashTimer / hurtFlashTime;
			m_item.tint = Color4{ 1.0f, 0.35f + (1.0f - k) * 0.65f, 0.35f + (1.0f - k) * 0.65f, 1.0f };
		}
		else if (m_invulnTimer > 0.0f) {
			const int frame = static_cast<int>(m_invulnTimer * 40.0f);
			m_item.tint = (frame & 1) ? Color4{ 1.0f, 1.0f, 1.0f, 0.35f } : Color4{ 1.0f, 1.0f, 1.0f, 1.0f };
		}
		else {
			m_item.tint = Color4{ 1.0f, 1.0f,1.0f,1.0f };
		}

		SpriteAnimation& a = animForState(m_animState < 0 ? 0 : m_animState);
		a.update(dt);
		a.apply(m_item);
	}

	void Player::updateGhost(float dt) {
		float ix = m_inX, iy = m_inY;
		const float mag = std::sqrt(ix * ix + iy * iy);
		if (mag > 1.0f) { ix /= mag; iy /= mag; }

		m_vx = ix * moveSpeed;
		m_vy = iy * moveSpeed;
		m_x += m_vx * dt;
		m_y += m_vy * dt;

		// keep the physics box synced so a switch to Play mode is seamless
		m_box.h = kBoxStandH;
		m_box.cx = m_x;
		m_box.cy = m_y - feetToCenterOffset(m_box.h);

		if (m_inX > 0.0f) m_facing = +1;
		else if (m_inX < 0.0f) m_facing = -1;

		setAnimState((m_inX != 0.0f || m_inY != 0.0f) ? 1 : 0);
	}

	void Player::updatePlay(float dt) {
		// --- timers (coyote uses last frame's grounded state) ---
		m_coyote  = std::max(0.0f, m_coyote - dt);
		m_landedThisFrame = false;
		m_jumpBuf = m_jumpPressed ? kJumpBuf : std::max(0.0f, m_jumpBuf - dt);
		const bool onGroundPrev = m_grounded;
		if (onGroundPrev) m_coyote = kCoyote;

		// --- crouch intent (ground only) ---
		bool wantCrouch = m_crouchHeld && onGroundPrev;

		// --- horizontal (full air control, snappy; half speed while crouched) ---
		// grounded cancels any leftover air recoil so feet grip the floor
		if (onGroundPrev) m_recoilVx = 0.0f;
		const float speed = wantCrouch ? moveSpeed * kCrouchSpeedMul : moveSpeed;
		float ix = m_inX;
		m_vx = ix * speed + m_recoilVx;
		m_recoilVx *= std::exp(-kRecoilDamp * dt);
		if (ix > 0.0f) m_facing = +1;
		else if (ix < 0.0f) m_facing = -1;

		// --- jump (buffered + coyote) ---
		if (m_jumpBuf > 0.0f && m_coyote > 0.0f && !wantCrouch) {
			m_vy = jumpSpeed;
			m_grounded = false;
			m_coyote = 0.0f;
			m_jumpBuf = 0.0f;
		}
		// variable height: releasing jump while rising cuts the climb
		if (!m_jumpHeld && m_vy > kJumpCut) m_vy = kJumpCut;

		// --- gravity ---
		m_vy -= gravity * dt;
		if (m_vy < -maxFall) m_vy = -maxFall;

		// --- shooting (auto-fires while held at a cadence) ---
		m_fireCooldown = std::max(0.0f, m_fireCooldown - dt);
		m_shootTimer   = std::max(0.0f, m_shootTimer - dt);
		const bool airborne = !onGroundPrev;
		if ((m_firePressed || m_fireHeld) && m_fireCooldown <= 0.0f) {
			m_fireCooldown = kFireCooldown;
			m_shootTimer   = kShootHold;

			float mfx, mfy;
			if (airborne)         { mfx = kMuzzleFwdAir;    mfy = kMuzzleUpAir; }
			else if (wantCrouch)  { mfx = kMuzzleFwdCrouch; mfy = kMuzzleUpCrouch; }
			else if (m_inX != 0.0f) { mfx = kMuzzleFwdWalk; mfy = kMuzzleUpWalk; }
			else                  { mfx = kMuzzleFwdStand;  mfy = kMuzzleUpStand; }

			m_shotX = m_x + static_cast<float>(m_facing) * mfx;
			m_shotY = m_y + mfy;
			m_shotDir = m_facing;
			m_shotPending = true;

			if (airborne) m_recoilVx = -static_cast<float>(m_facing) * kRecoilImpulse;  // gun pushes back in air
		}

		// --- crouch / stand box height (keep the box bottom fixed) ---
		const float targetH = wantCrouch ? kBoxCrouchH : kBoxStandH;
		if (targetH != m_box.h) {
			const float bottom = m_box.cy - m_box.h * 0.5f;
			if (targetH > m_box.h) {
				// standing up: only if the taller box is clear of solids
				AABB test = m_box;
				test.h = targetH;
				test.cy = bottom + targetH * 0.5f;
				if (!boxOverlapsSolid(test)) { m_box = test; }
				else { wantCrouch = true; }      // blocked -> stay crouched
			} else {
				m_box.cy = bottom + targetH * 0.5f;
				m_box.h = targetH;
			}
		}
		m_crouching = wantCrouch;

		// --- move + collide (X then Y, engine solver) ---
		const float prevBottom = m_box.cy - m_box.h * 0.5f;
		MoveResult2D res{};
		if (m_map && m_collLayer) {
			res = TileCollision::moveAndCollideEx(
				*m_map, *m_collLayer, m_box, m_vx, m_vy, dt,
				/*maxStepUp*/ 0.0f, /*oneWay*/ true, /*slopes*/ true, prevBottom);
		} else {
			m_box.cx += m_vx * dt;
			m_box.cy += m_vy * dt;
		}
		m_grounded = res.grounded;

		syncRenderFromBox();

		// --- landing timer (start when we touch down after being airborne) ---
		const bool landedThisFrame = (!onGroundPrev && res.grounded);
		m_landTimer = std::max(0.0f, m_landTimer - dt);
		if (landedThisFrame) m_landTimer = kLandTime;

		m_landedThisFrame = landedThisFrame;

		m_groundTileId = 0;
		if (m_grounded && m_map && m_collLayer) {
			const float tw = m_map->worldTileW();
			const float th = m_map->worldTileH();
			if (tw > 0.0f && th > 0.0f) {
				const float bottom = m_box.cy - m_box.h * 0.5f;
				const int tx = static_cast<int>(std::floor(m_box.cx / tw));
				const int ty = static_cast<int>(std::floor((bottom - 0.5f) / th));
				m_groundTileId = m_collLayer->at(tx, ty);
			}
		}

		// --- animation selection ---
		const bool onGround = m_grounded || m_coyote > 0.0f;   // debounce ground jitter
		int st;
		if (!onGround)                              st = (m_vy > 0.0f) ? 3 : 4;  // rise / fall
		else if (m_crouching)                       st = 2;                       // crouch / crawl
		else if (m_landTimer > 0.0f && ix == 0.0f)  st = 5;                       // landing recovery
		else if (ix != 0.0f)                        st = 1;                       // walk
		else                                        st = 0;                       // idle

		// shooting overlay: swap to the matching shoot pose while it's active
		if (m_shootTimer > 0.0f) {
			if (!onGround)        st = 8;   // air shoot
			else if (m_crouching) st = 2;   // stay prone (still fired)
			else if (ix != 0.0f)  st = 7;   // run-and-gun
			else                  st = 6;   // stand shoot
		}
		setAnimState(st);
	}

	void Player::render(Renderer2D& r2d) {
		r2d.draw(m_item);
	}

	bool Player::takeDamage(int amount, int knockbackDir) {
		if (amount <= 0) return false;
		if (m_mode == Mode::Ghost) return false;
		if (m_invulnTimer > 0.0f) return false;

		m_hp = std::max(0, m_hp - amount);
		m_invulnTimer = invulnDuration;
		m_hurtFlashTimer = hurtFlashTime;

		if (knockbackDir != 0) {
			const float dir = (knockbackDir > 0) ? +1.0f : -1.0f;
			m_vx += dir * knockbackImpulse;
		}
		return true;
	}
}
