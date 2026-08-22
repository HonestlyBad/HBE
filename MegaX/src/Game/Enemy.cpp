#include "Game/Enemy.h"
#include "Game/EnemyManager.h"   // DifficultyProfile (Item 10)
#include "Game/Player.h"

#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Core/AssetPaths.h"
#include "HBE/Core/Log.h"

#include <algorithm>
#include <cmath>

using namespace HBE::Renderer;

namespace MegaX {

    static constexpr int   kFrameW = 64;
    static constexpr int   kFrameH = 64;
    static constexpr float kPixelScale = 1.0f;

    static constexpr int   kIdleRow = 0;
    static constexpr int   kIdleCol0 = 0;
    static constexpr int   kIdleCol1 = 3;
    static constexpr float kIdleFps = 6.0f;

    static constexpr int   kWalkRow = 0;
    static constexpr int   kWalkCol0 = 0;
    static constexpr int   kWalkCol1 = 3;
    static constexpr float kWalkFps = 10.0f;

    static float posYForFeet(float feetY) {
        return feetY + static_cast<float>(kFrameH) * kPixelScale * 0.5f;
    }

    bool Enemy::isSolidTileAt(float wx, float wy) const {
        if (!m_map || !m_solid) return false;
        const float tw = m_map->worldTileW();
        const float th = m_map->worldTileH();
        if (tw <= 0.0f || th <= 0.0f) return false;
        const int tx = static_cast<int>(std::floor(wx / tw));
        const int ty = static_cast<int>(std::floor(wy / th));
        return TileCollision::isSolidTile(*m_map, *m_solid, tx, ty);
    }

    bool Enemy::init(ResourceCache& resources, Mesh* quadMesh, GLShader* spriteShader) {
        if (!quadMesh || !spriteShader) return false;

        m_idleSheet = SpriteRenderer2D::DeclareSpriteSheet(
            resources, "robot_soldier_idle",
            HBE::Core::AssetPaths::Resolve("sprites/Enemies/RobotSoldier/SoldierIdle_Spritesheet.png"),
            kFrameW, kFrameH);
        m_walkSheet = SpriteRenderer2D::DeclareSpriteSheet(
            resources, "robot_soldier_walk",
            HBE::Core::AssetPaths::Resolve("sprites/Enemies/RobotSoldier/SoldierFordward_Spritesheet.png"),
            kFrameW, kFrameH);

        if (!m_idleSheet.isValid() || !m_walkSheet.isValid()) {
            HBE::Core::LogError("Enemy::init: failed to load Robot Soldier sheets.");
            return false;
        }

        m_idleAnim = SpriteAnimation(&m_idleSheet, kIdleCol0, kIdleCol1, kIdleRow, kIdleFps, true);
        m_walkAnim = SpriteAnimation(&m_walkSheet, kWalkCol0, kWalkCol1, kWalkRow, kWalkFps, true);
        m_idleAnim.play(true);
        m_walkAnim.play(true);

        m_material.shader = spriteShader;
        m_material.texture = m_idleSheet.texture;

        m_item.mesh = quadMesh;
        m_item.material = &m_material;
        m_item.layer = 99;
        m_item.pass = RenderPass::World;
        m_item.transform.scaleX = kFrameW * kPixelScale;
        m_item.transform.scaleY = kFrameH * kPixelScale;
        m_item.tint = Color4{ 1.0f, 1.0f, 1.0f, 1.0f };

        m_maxHp = startHp;
        m_hp = m_maxHp;

        setAnimState(AnimState::Idle);
        return true;
    }

    void Enemy::spawn(float x, float groundY, int facing) {
        m_x = x;
        m_feetY = groundY;
        m_y = posYForFeet(groundY);
        m_facing = (facing >= 0) ? +1 : -1;
        m_prevX = m_x;
        m_prevY = m_y;

        // Physics body anchored to feet: box bottom == groundY, box top ==
        // groundY + boxHalfH*2. Cy sits halfH above feet.
        m_box.cx = m_x;
        m_box.cy = m_feetY + boxHalfH;
        m_box.w = boxHalfW * 2.0f;
        m_box.h = boxHalfH * 2.0f;
        m_vx = m_vy = 0.0f;
        m_grounded = true;
        m_prevBottom = m_box.cy - m_box.h * 0.5f;

        m_maxHp = startHp;
        m_hp = m_maxHp;
        m_invulnTimer = m_flashTimer = m_deathTimer = 0.0f;
        m_dead = false;

        m_landedThisFrame = false;
        m_wasGroundedLast = true;
        m_groundTileId = 0;
        m_justDied = false;
        m_walkDustAccum = 0.0f;

        m_state = AIState::Patrol;
        m_stateTimer = m_alertLatch = m_hiddenTimer = m_jumpCooldown = 0.0f;
        m_patrolWaitTimer = 0.0f;
        m_patrolTargetSign = m_facing;   // head in current facing dir first
        m_lastKnownX = m_x;
        m_lastKnownY = m_y;
        m_fireCooldown = 0.0f;

        setAnimState(AnimState::Idle);
    }

    void Enemy::setPatrolPath(float leftX, float rightX, float waitSec) {
        if (leftX > rightX) std::swap(leftX, rightX);
        m_patrolLeftX = leftX;
        m_patrolRightX = rightX;
        patrolWaitAtEnd = waitSec;
    }

    bool Enemy::takeDamage(int amount) {
        if (m_dead || m_invulnTimer > 0.0f || amount <= 0) return false;
        m_hp -= amount;
        m_invulnTimer = invulnAfterHit;
        m_flashTimer = hitFlashTime;
        if (m_hp <= 0) {
            m_hp = 0;
            m_dead = true;
            m_justDied = true;
            m_hitboxActive = false;
            m_deathTimer = deathFadeTime;
            m_vx = 0.0f;
        }
        return true;
    }

    AABB Enemy::hurtbox() const {
        AABB b{};
        const float fx = static_cast<float>(m_facing);
        b.cx = m_x + hurtOffsetX * fx;
        b.cy = m_feetY + hurtHalfH + hurtOffsetY;
        b.w = hurtHalfW * 2.0f;
        b.h = hurtHalfH * 2.0f;
        return b;
    }
    AABB Enemy::hitbox() const {
        AABB b{};
        const float fx = static_cast<float>(m_facing);
        b.cx = m_x + hitOffsetX * fx;
        b.cy = m_feetY + hitHalfH + hitOffsetY;
        b.w = hitHalfW * 2.0f;
        b.h = hitHalfH * 2.0f;
        return b;
    }

    void Enemy::fixedTick(float h, const Player& player) {
        m_prevX = m_x;
        m_prevY = m_y;

        m_landedThisFrame = false;

        if (m_invulnTimer > 0.0f) m_invulnTimer = std::max(0.0f, m_invulnTimer - h);
        if (m_flashTimer > 0.0f) m_flashTimer = std::max(0.0f, m_flashTimer - h);
        if (m_jumpCooldown > 0.0f) m_jumpCooldown = std::max(0.0f, m_jumpCooldown - h);

        if (m_dead) {
            if (m_deathTimer > 0.0f) m_deathTimer = std::max(0.0f, m_deathTimer -h);
            return;
        }

        if (player.mode() == Player::Mode::Ghost) {
            m_lastHeard = false;
            m_lastSeen = false;
        }
        else {
            m_lastHeard = hearsPlayer(player);
            m_lastSeen = seesPlayer(player);
            if (m_lastSeen) {
                m_lastKnownX = player.x();
                m_lastKnownY = player.y();
            }
        }

        switch (m_state) {
        case AIState::Patrol:     tickPatrol(h, player);     break;
        case AIState::Suspicious: tickSuspicious(h, player); break;
        case AIState::Alert:      tickAlert(h, player);      break;
        case AIState::Chase:      tickChase(h, player);      break;
        case AIState::Search:     tickSearch(h, player);     break;
        case AIState::Return:     tickReturn(h, player);     break;
        }

        applyPhysics(h);

        setAnimState((m_grounded && std::fabs(m_vx) > 5.0f) ? AnimState::Walk : AnimState::Idle);
    }

    void Enemy::updateVisual(float dt)
    {
        currentAnim().update(dt);
    }

    bool Enemy::hearsPlayer(const Player& p) const {
        if (std::fabs(p.velX()) < minPlayerVxToHear) return false;
        const AABB pb = p.hurtbox();
        if (pb.h < 34.0f) return false;

        const float dx = p.x() - m_x;
        const float dy = p.y() - m_y;
        return (dx * dx + dy * dy) <= (hearingRadius * hearingRadius);
    }

    bool Enemy::seesPlayer(const Player& p) const {
        const float ex = eyeX();
        const float ey = eyeY();
        const AABB pb = p.hurtbox();
        const float dx = pb.cx - ex;
        const float dy = pb.cy - ey;
        const float dist2 = dx * dx + dy * dy;
        if (dist2 > sightRange * sightRange) return false;

        const float dist = std::sqrt(dist2);
        if (dist > 0.0f) {
            const float dot = (dx * static_cast<float>(m_facing)) / dist;
            const float cosHalf = std::cos(sightHalfAngleDeg * 3.14159265f / 180.0f);
            if (dot < cosHalf) return false;
        }

        return losClear(ex, ey, pb.cx, pb.cy);
    }

    bool Enemy::losClear(float ax, float ay, float bx, float by) const {
        const float dx = bx - ax;
        const float dy = by - ay;
        const float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < 1.0f) return true;
        const int steps = std::max(1, static_cast<int>(std::ceil(dist / losStepPx)));
        for (int k = 1; k < steps; ++k) {
            const float t = static_cast<float>(k) / static_cast<float>(steps);
            if (isSolidTileAt(ax + dx * t, ay + dy * t)) return false;
        }
        return true;
    }

    bool Enemy::wallInFront() const {
        const float px = m_x + static_cast<float>(m_facing) * (boxHalfW + 4.0f);
        const float py = m_feetY + boxHalfH;
        return isSolidTileAt(px, py);
    }
    bool Enemy::ledgeInFront() const {
        const float px = m_x + static_cast<float>(m_facing) * (boxHalfW + 4.0f);
        const float py = m_feetY - 4.0f;

        if (!m_grounded) return false;
        return !isSolidTileAt(px, py);
    }
    void Enemy::faceX(float targetX) {
        if (std::fabs(targetX - m_x) < 0.5f) return;
        m_facing = (targetX >= m_x) ? +1 : -1;
    }

    void Enemy::enter(AIState s) {
        m_state = s;
        m_stateTimer = 0.0f;
        m_alertLatch = 0.0f;
        m_hiddenTimer = 0.0f;
        if (s == AIState::Suspicious) { faceX(m_lastKnownX); m_vx = 0.0f; }
        if (s == AIState::Alert) { faceX(m_lastKnownX); m_vx = 0.0f; }
        if (s == AIState::Patrol) { m_patrolWaitTimer = 0.0f; }
    }

    void Enemy::tickPatrol(float dt, const Player&) {
        if (m_patrolRightX - m_patrolLeftX < 1.0f) {
            m_vx = 0.0f;
            if (m_lastSeen) { enter(AIState::Alert); return; }
            if (m_lastHeard) { enter(AIState::Suspicious); return; }
            return;
        }

        if (m_patrolWaitTimer > 0.0f) {
            m_patrolWaitTimer = std::max(0.0f, m_patrolWaitTimer - dt);
            m_vx = 0.0f;
            if (m_patrolWaitTimer <= 0.0f) {
                m_facing = -m_facing;
                m_patrolTargetSign = m_facing;
            }
            if (m_lastSeen) { enter(AIState::Alert); return; }
            if (m_lastHeard) { enter(AIState::Suspicious); return; }
            return;
        }

        m_vx = static_cast<float>(m_facing) * moveSpeed;

        const bool reachedRight = (m_facing > 0 && m_x >= m_patrolRightX);
        const bool reachedLeft = (m_facing < 0 && m_x <= m_patrolLeftX);
        const bool blocked = wallInFront() || ledgeInFront();
        if (reachedRight || reachedLeft || blocked) {
            m_vx = 0.0f;
            m_patrolWaitTimer = patrolWaitAtEnd;
        }

        if (m_lastSeen) { enter(AIState::Alert); return; }
        if (m_lastHeard) { enter(AIState::Suspicious); return; }
    }

    void Enemy::tickSuspicious(float dt, const Player& player) {
        m_vx = 0.0f;
        faceX(m_lastKnownX);
        m_stateTimer += dt;
        if (m_lastSeen) { enter(AIState::Alert);  return; }
        if (m_stateTimer >= suspicionDuration) { enter(AIState::Return); return; }
        (void)player;
    }

    void Enemy::tickAlert(float dt, const Player& player) {
        m_vx = 0.0f;
        faceX(player.x());
        m_stateTimer += dt;

        if (m_stateTimer >= alertLatchTime) {
            enter(AIState::Chase);
        }
    }

    void Enemy::tickChase(float dt, const Player& player) {
        if (player.mode() == Player::Mode::Ghost) {
            enter(AIState::Search);
            return;
        }

        faceX(player.x());

        const float dx = player.x() - m_x;
        const float adx = std::fabs(dx);
        const float standoff = shootingRange * standoffFrac;
        int moveDir = 0;
        if (adx > standoff + standoffDeadzone)      moveDir = m_facing;   // advance
        else if (adx < standoff - standoffDeadzone) moveDir = -m_facing;  // back off
        // else: within the deadzone -> hold ground and (later) shoot
        m_vx = static_cast<float>(moveDir) * chaseSpeed;

        if (m_grounded && m_jumpCooldown <= 0.0f) {
            const float dyToPlayer = player.y() - m_y;
            const bool advancing = (moveDir == m_facing) && moveDir != 0;
            const bool wantJump = (dyToPlayer > 24.0f) ||
                (advancing && (wallInFront() || ledgeInFront()));
            if (wantJump) {
                m_vy = jumpSpeed;
                m_grounded = false;
                m_jumpCooldown = chaseJumpCooldown;
            }
        }

        tickShooting(dt, player);

        const AABB pb = player.hurtbox();
        const bool crouching = pb.h < 34.0f;
        if (!m_lastSeen && crouching) {
            m_hiddenTimer += dt;
            if (m_hiddenTimer >= loseAggroDelay) { enter(AIState::Search); return; }
        }
        else {
            m_hiddenTimer = 0.0f;
        }
    }

    void Enemy::tickSearch(float dt, const Player& player) {
        if (player.mode() != Player::Mode::Ghost && m_lastSeen) {
            enter(AIState::Alert);
            return;
        }

        const float dx = m_lastKnownX - m_x;
        if (std::fabs(dx) > 8.0f) {
            faceX(m_lastKnownX);
            m_vx = static_cast<float>(m_facing) * moveSpeed;
            m_stateTimer = 0.0f;
        }
        else {
            m_vx = 0.0f;
            m_stateTimer += dt;
            if (m_stateTimer >= searchDwellTime) enter(AIState::Return);
        }
    }

    void Enemy::tickReturn(float dt, const Player& player) {
        if (player.mode() != Player::Mode::Ghost) {
            if (m_lastSeen) { enter(AIState::Alert);      return; }
            if (m_lastHeard) { enter(AIState::Suspicious); return; }
        }

        const float target = (std::fabs(m_x - m_patrolLeftX) < std::fabs(m_x - m_patrolRightX))
            ? m_patrolLeftX : m_patrolRightX;
        if (std::fabs(m_x - target) <= 4.0f) {
            enter(AIState::Patrol);
            m_facing = (target <= m_patrolLeftX + 0.5f) ? -1 : +1;
            m_patrolTargetSign = m_facing;
            return;
        }
        faceX(target);
        m_vx = static_cast<float>(m_facing) * moveSpeed;
        if (wallInFront()) enter(AIState::Patrol);
        (void)dt;
    }

    void Enemy::tickShooting(float dt, const Player& player) {
        if (m_fireCooldown > 0.0f) m_fireCooldown -= dt;

        if (!m_fireFn) return;
        if (!m_lastSeen) return;

        const float dx = player.x() - m_x;
        const float dy = player.y() - m_y;
        if (dx * dx + dy * dy > shootingRange * shootingRange) return;

        if (m_fireCooldown > 0.0f) return;

        float mx, my;
        muzzleWorldPos(mx, my);

        float aimX = player.x();
        float aimY = player.y();
        if (leadFactor > 0.0001f && bulletSpeed > 0.0f) {
            const float toPlayerX = aimX - mx;
            const float toPlayerY = aimY - my;
            const float dist = std::sqrt(toPlayerX * toPlayerX + toPlayerY * toPlayerY);
            const float flight = dist / bulletSpeed;
            aimX = player.x() + player.velX() * flight * leadFactor;
        }

        m_fireFn(m_fireCtx, mx, my, aimX, aimY, bulletSpeed, bulletDamage);
        m_fireCooldown = fireCooldownSec;
    }

    void Enemy::applyPhysics(float dt) {
        m_vy -= gravity * dt;
        if (m_vy < -maxFall) m_vy = -maxFall;

        m_prevBottom = m_box.cy - m_box.h * 0.5f;

        bool prevGrounded = m_wasGroundedLast;

        if (m_map && m_solid) {
            MoveResult2D res = TileCollision::moveAndCollideEx(
                *m_map, *m_solid, m_box, m_vx, m_vy, dt,
                0.0f, true, true, m_prevBottom);
            m_grounded = res.grounded;
        }
        else {
            m_box.cx += m_vx * dt;
            m_box.cy += m_vy * dt;
        }
        syncFromBox();

        if (!prevGrounded && m_grounded) {
            m_landedThisFrame = true;
        }
        m_wasGroundedLast = m_grounded;

        if (m_grounded && m_map && m_solid) {
            const float tw = m_map->worldTileW();
            const float th = m_map->worldTileH();
            if (tw > 0.0f && th > 0.0f) {
                const float probeY = m_feetY - 1.0f;
                const int tx = static_cast<int>(std::floor(m_x / tw));
                const int ty = static_cast<int>(std::floor(probeY / th));
                m_groundTileId = m_solid->at(tx, ty);
            }
        }
        else {
            m_groundTileId = 0;
        }
    }

    void Enemy::syncFromBox() {
        m_x = m_box.cx;
        m_feetY = m_box.cy - m_box.h * 0.5f + spriteFeetOffsetY;
        m_y = posYForFeet(m_feetY);
    }

    void Enemy::setAnimState(AnimState s) {
        if (s == m_animState) return;
        m_animState = s;
        if (s == AnimState::Walk) {
            m_material.texture = m_walkSheet.texture;
        }
        else {
            m_material.texture = m_idleSheet.texture;
        }
        currentAnim().play(true);
    }
    SpriteAnimation& Enemy::currentAnim() {
        return (m_animState == AnimState::Walk) ? m_walkAnim : m_idleAnim;
    }

    void Enemy::render(Renderer2D& r2d, float alpha) {
        if (isFinished()) return;

        const float t = (alpha < 0.0f) ? 0.0f : ((alpha > 1.0f) ? 1.0f : alpha);

        m_item.transform.posX = m_prevX + (m_x - m_prevX) * t;
        m_item.transform.posY = m_prevY + (m_y - m_prevY) * t;
        m_item.transform.scaleX = kFrameW * kPixelScale * static_cast<float>(m_facing);
        m_item.transform.scaleY = kFrameH * kPixelScale;

        if (m_dead) {
            const float t = (deathFadeTime > 0.0f) ? (m_deathTimer / deathFadeTime) : 0.0f;
            m_item.tint = Color4{ 1.0f, 1.0f, 1.0f, std::clamp(t, 0.0f, 1.0f) };
        }
        else if (m_flashTimer > 0.0f) {
            m_item.tint = Color4{ 1.0f, 0.4f, 0.4f, 1.0f };
        }
        else {
            m_item.tint = Color4{ 1.0f, 1.0f, 1.0f, 1.0f };
        }

        currentAnim().apply(m_item);
        r2d.draw(m_item);
    }

    Enemy::BubbleIcon Enemy::bubbleIcon() const {
        if (m_dead) return BubbleIcon::None;
        switch (m_state) {
        case AIState::Suspicious: return BubbleIcon::Question;
        case AIState::Alert:
        case AIState::Chase:
        case AIState::Search:     return BubbleIcon::Exclaim;
        default:                  return BubbleIcon::None;
        }
    }

    void Enemy::onHeardGunshot(float sx, float sy) {
        if (m_dead) return;
        m_lastKnownX = sx;
        m_lastKnownY = sy;
        if (m_state == AIState::Patrol || m_state == AIState::Return) {
            enter(AIState::Suspicious);
        }
    }

    void Enemy::snapshotBaseStats() {
        m_baseChaseSpeed     = chaseSpeed;
        m_baseSightRange     = sightRange;
        m_baseHearingRadius  = hearingRadius;
        m_baseLoseAggroDelay = loseAggroDelay;
        m_baseStartHp        = startHp;
    }

    void Enemy::applyDifficulty(const DifficultyProfile& p) {
        chaseSpeed     = m_baseChaseSpeed     * p.chaseSpeedMul;
        sightRange     = m_baseSightRange     * p.sightRangeMul;
        hearingRadius  = m_baseHearingRadius  * p.hearingRadiusMul;
        loseAggroDelay = m_baseLoseAggroDelay * p.loseAggroDelayMul;

        const int newStart = std::max(1, static_cast<int>(m_baseStartHp * p.startHpMul));
        startHp = newStart;
        m_maxHp = newStart;

        bulletDamage    = p.bulletDamage;
        fireCooldownSec = p.fireCooldownSec;
        bulletSpeed     = p.bulletSpeed;
        leadFactor      = p.leadFactor;
    }

    void Enemy::muzzleWorldPos(float& mx, float& my) const {
        mx = m_x + static_cast<float>(m_facing) * muzzleForwardX;
        my = m_feetY + muzzleAboveFeet;
    }

    bool Enemy::consumeWalkDustPuff(float dt, float period) {
        const bool moving = std::fabs(m_vx) > 5.0f;
        const bool grounded = m_grounded;
        if (!moving || !grounded) {
            m_walkDustAccum = 0.0f;
            return false;
        }
        m_walkDustAccum += dt;
        if (m_walkDustAccum < period) return false;
        m_walkDustAccum -= period;
        return true;
    }
}