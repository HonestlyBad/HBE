# 02 — `Enemy.cpp` state machine

Full replacement source for `src/Game/Enemy.cpp`. Every AI
transition, patrol / chase / search movement rule, sensor
helper, and the animation swap live here.

Sections:

1. Constants
2. Utility helpers
3. `init` / `spawn`
4. `takeDamage`, hurtbox/hitbox (unchanged from Item 08 aside
   from feet-anchoring — kept in one place for reference)
5. `tick` — top-level orchestrator
6. Sensing helpers (`hearsPlayer`, `seesPlayer`, `losClear`)
7. Patrol helpers (`wallInFront`, `ledgeInFront`, `faceX`)
8. State-machine per-state transitions
9. Physics (`applyPhysics`, `syncFromBox`)
10. Animation swap + `render`
11. `bubbleIcon` + `onHeardGunshot`

---

## Full replacement — `src/Game/Enemy.cpp`

```cpp
#include "Game/Enemy.h"
#include "Game/Player.h"

#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Core/AssetPaths.h"
#include "HBE/Core/Log.h"

#include <algorithm>
#include <cmath>

using namespace HBE::Renderer;

namespace MegaX {

    // ---------------------------------------------------------------- 1. constants
    // Sprite grid: idle & walk are both 4-col x 64x64 cell sheets.
    // Idle sheet: 4c x 2r (Item 08). Walk sheet: 4c x 4r (all 16 filled) --
    // we use row 0 only; SpriteAnimation is single-row. If the cycle
    // reads oddly, swap kWalkRow to 1/2/3.
    static constexpr int   kFrameW      = 64;
    static constexpr int   kFrameH      = 64;
    static constexpr float kPixelScale  = 1.0f;

    static constexpr int   kIdleRow     = 0;
    static constexpr int   kIdleCol0    = 0;
    static constexpr int   kIdleCol1    = 3;
    static constexpr float kIdleFps     = 6.0f;

    static constexpr int   kWalkRow     = 0;
    static constexpr int   kWalkCol0    = 0;
    static constexpr int   kWalkCol1    = 3;
    static constexpr float kWalkFps     = 10.0f;

    // Feet-to-sprite-center offset. Sprite pixels sit in the LOWER half of
    // each 64-tall cell (alpha-scan verified), so drawing the 64x64 quad
    // centered kFrameH/2 above the feet places the visible feet on the
    // ground line -- matches how Item 08 anchors the enemy.
    static float posYForFeet(float feetY) {
        return feetY + static_cast<float>(kFrameH) * kPixelScale * 0.5f;
    }

    // ---------------------------------------------------------------- 2. utility
    // Convert world coord -> tile coord and query the solid layer.
    bool Enemy::isSolidTileAt(float wx, float wy) const {
        if (!m_map || !m_solid) return false;
        const float tw = m_map->worldTileW();
        const float th = m_map->worldTileH();
        if (tw <= 0.0f || th <= 0.0f) return false;
        const int tx = static_cast<int>(std::floor(wx / tw));
        const int ty = static_cast<int>(std::floor(wy / th));
        return TileCollision::isSolidTile(*m_map, *m_solid, tx, ty);
    }

    // ---------------------------------------------------------------- 3. init / spawn
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

        m_material.shader  = spriteShader;
        m_material.texture = m_idleSheet.texture;

        m_item.mesh     = quadMesh;
        m_item.material = &m_material;
        m_item.layer    = 99;
        m_item.pass     = RenderPass::World;
        m_item.transform.scaleX = kFrameW * kPixelScale;
        m_item.transform.scaleY = kFrameH * kPixelScale;
        m_item.tint     = Color4{ 1.0f, 1.0f, 1.0f, 1.0f };

        m_maxHp = startHp;
        m_hp    = m_maxHp;

        setAnimState(AnimState::Idle);
        return true;
    }

    void Enemy::spawn(float x, float groundY, int facing) {
        m_x     = x;
        m_feetY = groundY;
        m_y     = posYForFeet(groundY);
        m_facing = (facing >= 0) ? +1 : -1;

        // Physics body anchored to feet: box bottom == groundY, box top ==
        // groundY + boxHalfH*2. Cy sits halfH above feet.
        m_box.cx = m_x;
        m_box.cy = m_feetY + boxHalfH;
        m_box.w  = boxHalfW * 2.0f;
        m_box.h  = boxHalfH * 2.0f;
        m_vx = m_vy = 0.0f;
        m_grounded = true;
        m_prevBottom = m_box.cy - m_box.h * 0.5f;

        m_maxHp = startHp;
        m_hp    = m_maxHp;
        m_invulnTimer = m_flashTimer = m_deathTimer = 0.0f;
        m_dead = false;

        m_state = AIState::Patrol;
        m_stateTimer = m_alertLatch = m_hiddenTimer = m_jumpCooldown = 0.0f;
        m_patrolWaitTimer = 0.0f;
        m_patrolTargetSign = m_facing;   // head in current facing dir first
        m_lastKnownX = m_x;
        m_lastKnownY = m_y;

        setAnimState(AnimState::Idle);
    }

    // Call this once, before or after spawn(), to define the patrol range.
    // If leftX > rightX we swap. If leftX == rightX the enemy stands still
    // forever (guard post) but still senses / turns / chases.
    void Enemy::setPatrolPath(float leftX, float rightX, float waitSec) {
        if (leftX > rightX) std::swap(leftX, rightX);
        m_patrolLeftX  = leftX;
        m_patrolRightX = rightX;
        patrolWaitAtEnd = waitSec;
    }

    // ---------------------------------------------------------------- 4. damage / boxes
    bool Enemy::takeDamage(int amount) {
        if (m_dead || m_invulnTimer > 0.0f || amount <= 0) return false;
        m_hp -= amount;
        m_invulnTimer = invulnAfterHit;
        m_flashTimer  = hitFlashTime;
        if (m_hp <= 0) {
            m_hp = 0;
            m_dead = true;
            m_hitboxActive = false;
            m_deathTimer = deathFadeTime;
            m_vx = 0.0f;
        }
        return true;
    }

    AABB Enemy::hurtbox() const {
        AABB b{};
        const float fx = static_cast<float>(m_facing);
        b.cx = m_x    + hurtOffsetX * fx;
        b.cy = m_feetY + hurtHalfH  + hurtOffsetY;
        b.w  = hurtHalfW * 2.0f;
        b.h  = hurtHalfH * 2.0f;
        return b;
    }
    AABB Enemy::hitbox() const {
        AABB b{};
        const float fx = static_cast<float>(m_facing);
        b.cx = m_x    + hitOffsetX * fx;
        b.cy = m_feetY + hitHalfH  + hitOffsetY;
        b.w  = hitHalfW * 2.0f;
        b.h  = hitHalfH * 2.0f;
        return b;
    }

    // ---------------------------------------------------------------- 5. tick (orchestrator)
    void Enemy::tick(float dt, const Player& player) {
        // Timers first (so a hit / cooldown expiry counts THIS frame).
        if (m_invulnTimer > 0.0f) m_invulnTimer = std::max(0.0f, m_invulnTimer - dt);
        if (m_flashTimer  > 0.0f) m_flashTimer  = std::max(0.0f, m_flashTimer  - dt);
        if (m_jumpCooldown > 0.0f) m_jumpCooldown = std::max(0.0f, m_jumpCooldown - dt);

        if (m_dead) {
            if (m_deathTimer > 0.0f) m_deathTimer = std::max(0.0f, m_deathTimer - dt);
            // No physics / no AI once dead -- freeze in place, fade out.
            currentAnim().update(dt);
            return;
        }

        // Ghost bypass at the top of sense-tick: leave m_lastHeard/m_lastSeen
        // false and the state machine effectively "un-hears" you.
        if (player.mode() == Player::Mode::Ghost) {
            m_lastHeard = false;
            m_lastSeen  = false;
        } else {
            m_lastHeard = hearsPlayer(player);
            m_lastSeen  = seesPlayer(player);
            if (m_lastSeen) {
                m_lastKnownX = player.x();
                m_lastKnownY = player.y();
            }
        }

        // Per-state tick: sets m_vx (intent) and possibly triggers a jump.
        switch (m_state) {
            case AIState::Patrol:     tickPatrol(dt, player);     break;
            case AIState::Suspicious: tickSuspicious(dt, player); break;
            case AIState::Alert:      tickAlert(dt, player);      break;
            case AIState::Chase:      tickChase(dt, player);      break;
            case AIState::Search:     tickSearch(dt, player);     break;
            case AIState::Return:     tickReturn(dt, player);     break;
        }

        applyPhysics(dt);

        // Anim swap: walking whenever we're grounded AND intending to move.
        setAnimState((m_grounded && std::fabs(m_vx) > 5.0f) ? AnimState::Walk : AnimState::Idle);
        currentAnim().update(dt);
    }

    // ---------------------------------------------------------------- 6. sensing
    bool Enemy::hearsPlayer(const Player& p) const {
        // Silent player? no ping.
        if (std::fabs(p.velX()) < minPlayerVxToHear) return false;
        // Crouching kills all sound.
        // Player has no isCrouched() accessor yet -- see note below on how
        // we detect it via hurtbox height.
        // (See doc 03 for the recommended one-line accessor.)
        // For now, treat crouch as "hurtbox height <= X" -- Player Item 04
        // shrinks the box height when crouching.
        const AABB pb = p.hurtbox();
        // Any playing sprite whose collision box height is less than ~34 px
        // is currently crouched (Player standing box height is ~48 px).
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

        // Facing cone (dot product with facing vector).
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

    // ---------------------------------------------------------------- 7. patrol probes
    bool Enemy::wallInFront() const {
        const float px = m_x + static_cast<float>(m_facing) * (boxHalfW + 4.0f);
        const float py = m_feetY + boxHalfH;   // chest height
        return isSolidTileAt(px, py);
    }
    bool Enemy::ledgeInFront() const {
        const float px = m_x + static_cast<float>(m_facing) * (boxHalfW + 4.0f);
        const float py = m_feetY - 4.0f;       // one step below feet
        // We only trust the "no floor" answer while grounded; airborne calls
        // give false negatives.
        if (!m_grounded) return false;
        return !isSolidTileAt(px, py);
    }
    void Enemy::faceX(float targetX) {
        if (std::fabs(targetX - m_x) < 0.5f) return;
        m_facing = (targetX >= m_x) ? +1 : -1;
    }

    // ---------------------------------------------------------------- 8. state machine
    void Enemy::enter(AIState s) {
        m_state = s;
        m_stateTimer = 0.0f;
        m_alertLatch = 0.0f;
        m_hiddenTimer = 0.0f;
        if (s == AIState::Suspicious) { faceX(m_lastKnownX); m_vx = 0.0f; }
        if (s == AIState::Alert)      { faceX(m_lastKnownX); m_vx = 0.0f; }
        if (s == AIState::Patrol)     { m_patrolWaitTimer = 0.0f; }
    }

    void Enemy::tickPatrol(float dt, const Player& /*player*/) {
        // Standing-post variant: no range => never move, but still sense.
        if (m_patrolRightX - m_patrolLeftX < 1.0f) {
            m_vx = 0.0f;
            if (m_lastSeen)  { enter(AIState::Alert); return; }
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
            if (m_lastSeen)  { enter(AIState::Alert); return; }
            if (m_lastHeard) { enter(AIState::Suspicious); return; }
            return;
        }

        m_vx = static_cast<float>(m_facing) * moveSpeed;

        const bool reachedRight = (m_facing > 0 && m_x >= m_patrolRightX);
        const bool reachedLeft  = (m_facing < 0 && m_x <= m_patrolLeftX);
        const bool blocked      = wallInFront() || ledgeInFront();
        if (reachedRight || reachedLeft || blocked) {
            m_vx = 0.0f;
            m_patrolWaitTimer = patrolWaitAtEnd;
        }

        if (m_lastSeen)  { enter(AIState::Alert); return; }
        if (m_lastHeard) { enter(AIState::Suspicious); return; }
    }

    void Enemy::tickSuspicious(float dt, const Player& player) {
        m_vx = 0.0f;
        faceX(m_lastKnownX);
        m_stateTimer += dt;
        if (m_lastSeen)                        { enter(AIState::Alert);  return; }
        if (m_stateTimer >= suspicionDuration) { enter(AIState::Return); return; }
        (void)player;
    }

    void Enemy::tickAlert(float dt, const Player& player) {
        m_vx = 0.0f;
        faceX(player.x());
        m_stateTimer += dt;
        // Short startle -- always upgrade to Chase after alertLatchTime,
        // whether or not still visible. That keeps the "!" beat feeling
        // committed even if the player ducks around a corner.
        if (m_stateTimer >= alertLatchTime) {
            enter(AIState::Chase);
        }
    }

    void Enemy::tickChase(float dt, const Player& player) {
        // Ghost mid-chase: immediately drop to Search of last-known.
        if (player.mode() == Player::Mode::Ghost) {
            enter(AIState::Search);
            return;
        }

        faceX(player.x());
        m_vx = static_cast<float>(m_facing) * chaseSpeed;

        // Jump intent: player is above OR wall/ledge in front. Cooldown
        // stops jump-spam on flat terrain.
        if (m_grounded && m_jumpCooldown <= 0.0f) {
            const float dyToPlayer = player.y() - m_y;   // +Y up: >0 means above
            const bool wantJump = (dyToPlayer > 24.0f) || wallInFront() || ledgeInFront();
            if (wantJump) {
                m_vy = jumpSpeed;
                m_grounded = false;
                m_jumpCooldown = chaseJumpCooldown;
            }
        }

        // Hidden tracking: crouched behind cover for long enough => Search.
        const AABB pb = player.hurtbox();
        const bool crouching = pb.h < 34.0f;
        if (!m_lastSeen && crouching) {
            m_hiddenTimer += dt;
            if (m_hiddenTimer >= loseAggroDelay) { enter(AIState::Search); return; }
        } else {
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
            m_stateTimer = 0.0f;   // reset dwell whenever we're still moving
        } else {
            m_vx = 0.0f;
            m_stateTimer += dt;
            if (m_stateTimer >= searchDwellTime) enter(AIState::Return);
        }
    }

    void Enemy::tickReturn(float dt, const Player& player) {
        if (player.mode() != Player::Mode::Ghost) {
            if (m_lastSeen)  { enter(AIState::Alert);      return; }
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
        // If we're wedged into a wall on the return trip, just declare done.
        if (wallInFront()) enter(AIState::Patrol);
        (void)dt;
    }

    // ---------------------------------------------------------------- 9. physics
    void Enemy::applyPhysics(float dt) {
        // Gravity (engine is +Y up; matches Player.cpp).
        m_vy -= gravity * dt;
        if (m_vy < -maxFall) m_vy = -maxFall;

        m_prevBottom = m_box.cy - m_box.h * 0.5f;

        if (m_map && m_solid) {
            MoveResult2D res = TileCollision::moveAndCollideEx(
                *m_map, *m_solid, m_box, m_vx, m_vy, dt,
                /*maxStepUp*/ 0.0f, /*oneWay*/ true, /*slopes*/ true, m_prevBottom);
            m_grounded = res.grounded;
        } else {
            m_box.cx += m_vx * dt;
            m_box.cy += m_vy * dt;
        }
        syncFromBox();
    }

    void Enemy::syncFromBox() {
        m_x     = m_box.cx;
        m_feetY = m_box.cy - m_box.h * 0.5f + spriteFeetOffsetY;
        m_y     = posYForFeet(m_feetY);
    }

    // --------------------------------------------------------------- 10. anim + render
    void Enemy::setAnimState(AnimState s) {
        if (s == m_animState) return;
        m_animState = s;
        if (s == AnimState::Walk) {
            m_material.texture = m_walkSheet.texture;
        } else {
            m_material.texture = m_idleSheet.texture;
        }
        // Rewind the incoming anim so state changes read cleanly.
        currentAnim().play(true);
    }
    SpriteAnimation& Enemy::currentAnim() {
        return (m_animState == AnimState::Walk) ? m_walkAnim : m_idleAnim;
    }

    void Enemy::render(Renderer2D& r2d) {
        if (isFinished()) return;

        m_item.transform.posX   = m_x;
        m_item.transform.posY   = m_y;
        m_item.transform.scaleX = kFrameW * kPixelScale * static_cast<float>(m_facing);
        m_item.transform.scaleY = kFrameH * kPixelScale;

        if (m_dead) {
            const float t = (deathFadeTime > 0.0f) ? (m_deathTimer / deathFadeTime) : 0.0f;
            m_item.tint = Color4{ 1.0f, 1.0f, 1.0f, std::clamp(t, 0.0f, 1.0f) };
        } else if (m_flashTimer > 0.0f) {
            m_item.tint = Color4{ 1.0f, 0.4f, 0.4f, 1.0f };
        } else {
            m_item.tint = Color4{ 1.0f, 1.0f, 1.0f, 1.0f };
        }

        currentAnim().apply(m_item);
        r2d.draw(m_item);
    }

    // ---------------------------------------------------------------- 11. bubble + gunshot
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
        // Only bump us up if we're currently unaware -- don't yank an
        // in-progress Chase back to Suspicious.
        if (m_state == AIState::Patrol || m_state == AIState::Return) {
            enter(AIState::Suspicious);
        }
    }
}
```

---

## Header additions Enemy.cpp assumes

Two per-state tick functions are called from `tick()`. Add these
private forward declarations to `include/Game/Enemy.h` — inside
the class, at the bottom of the `private:` block, just above the
data members:

```cpp
        void tickPatrol(float dt, const Player& player);
        void tickSuspicious(float dt, const Player& player);
        void tickAlert(float dt, const Player& player);
        void tickChase(float dt, const Player& player);
        void tickSearch(float dt, const Player& player);
        void tickReturn(float dt, const Player& player);
```

---

## Design notes worth remembering

* **Crouch detection reads the Player hurtbox height.** Player
  shrinks its collision box while crouching (Item 04); using the
  hurtbox height (< 34 px = crouched) means the enemy respects
  crouch without Player needing a new `isCrouched()` accessor.
  If you ever add one — great, replace the two `pb.h < 34.0f`
  checks with `p.isCrouched()`.
* **Ghost bypass is checked twice.** Once at the top of
  `tick()` (zeroes `m_lastHeard/m_lastSeen`) and once inside
  `tickChase` (immediate un-latch). Both are necessary: the top
  guard covers Patrol/Suspicious/Alert; the Chase guard is what
  makes the aggro drop feel instant instead of waiting three
  hidden-seconds.
* **Wall-in-front + ledge-in-front are combined in patrol.**
  Either one triggers a wait+flip. This is why placing a single
  solid tile at the middle of a wide patrol range naturally cuts
  the patrol into two halves.
* **`m_jumpCooldown` prevents jump-spam on flat terrain.** Without
  it, an enemy running into a wall on flat ground would jump
  every physics frame and skip along the ceiling. With 0.5s
  cooldown you get one clean "trying to jump the wall" hop, then
  a pause.
* **`m_alertLatch` is currently unused.** It's declared in
  Item 09 for Item 10's shooting cadence — leaving it here so
  the next diff is smaller. Ignore any "unused private field"
  warning from the analyzer.

Next: `03_bubbles_and_gamelayer.md` — `EnemyManager` gets the
player+collision setters, `DebugDraw2D`-based bubble rendering,
and `GameLayer` gets wired to feed both plus the gunshot ping.
