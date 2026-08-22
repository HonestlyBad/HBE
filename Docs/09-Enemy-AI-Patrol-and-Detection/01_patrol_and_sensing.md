# 01 — Patrol, sensing, and the extended `Enemy.h`

This doc covers:

1. The **design** of every sensor (patrol movement, hearing,
   sight cone, LOS raycast), including exact formulas so you
   can eyeball what's happening from the code.
2. The **extended `Enemy.h`** with every new field/method
   Item 09 needs.

Implementation (`Enemy.cpp` and the state machine) is in
`02_enemy_state_machine.md`.

---

## 1. Design

### 1a. Patrol

The enemy walks along a horizontal patrol segment defined by two
world-space X coordinates: `m_patrolLeftX <= m_patrolRightX`. It
walks toward its current target end at `moveSpeed`, and when it
reaches it (or gets bumped short by a wall / ledge), it:

1. Stops (`m_vx = 0`).
2. Waits for `patrolWaitAtEnd` seconds.
3. Flips `m_facing`.
4. Sets the target to the other end.
5. Resumes walking.

Two conditions cause an *early* turnaround on either side (long
before reaching the far X):

* **Wall in front**: sample a probe point one tile in front of
  the enemy at chest height. If solid, turn around.
* **Ledge in front**: sample a probe point one tile in front of
  the enemy, one tile *below* the feet. If NOT solid, turn
  around. This is the reason a patrolling enemy never walks off
  a platform. Chase disables this check, which is what lets
  chase feel dangerous.

Patrol movement is grounded, feet-anchored, and uses
`TileCollision::moveAndCollideEx` just like the player — so
slopes, one-way platforms and step-ups all "just work".

### 1b. Hearing

Hearing is an omnidirectional radius check with two gates:

```
hears = distance(enemyPos, playerPos) <= hearingRadius
     && !player.isCrouched()
     && player.mode() != Ghost
     && |player.velX()| >= minPlayerVxToHear
```

Rationale:

* Distance is measured foot-to-foot in world pixels; the y
  component matters (a player two floors up will not be heard
  through 2 tiles of floor).
* Crouching kills the sound entirely — that's the whole point
  of crouch as stealth in a Mega-Man-X-style game.
* Ghost mode is invisible AND silent (no audio, no collision —
  matches the ghost contract from Item 05).
* The velocity gate is important. Without it, a stationary
  player standing 5 pixels from a patrolling enemy would ping
  hearing every single frame. Requiring `|vx|` above a small
  threshold means "you have to actually be walking".

Hearing is also the transport for the **gunshot ping**: when the
player fires, `EnemyManager::notifyGunshot(x, y)` is called by
`GameLayer`, and every enemy within `gunshotHearRadius` (bigger
than normal `hearingRadius`) is bumped into Suspicious
regardless of crouch state. Details in doc 03.

### 1c. Sight (vision cone)

Sight is a forward cone check with LOS:

```
sees = withinRange && withinConeAngle && losClear
```

Range check:

```
dx = playerCX - eyeX
dy = playerCY - eyeY
dist2 = dx*dx + dy*dy
withinRange = dist2 <= sightRange^2
```

Cone check — dot product against the enemy facing vector
(`facing = (m_facing, 0)`):

```
if dist == 0: withinCone = true       # standing on top of you
dot  = (dx * m_facing) / dist         # cosine of angle to target
withinCone = dot >= cos(sightHalfAngleDeg)
```

The enemy's *eye* point sits slightly above the head:
`eyeX = m_x`, `eyeY = m_feetY + eyeHeightAboveFeet`. Using the
eye (not the feet) makes the cone graze the top of the crouched
player instead of the ground.

Sight is skipped entirely while the player is in Ghost.

### 1d. Line-of-sight raycast

LOS walks the segment from the enemy's eye to the player's
hurtbox center and samples the solid layer at each step. Any
solid tile => blocked.

```
steps = ceil(dist / stepPx)   # stepPx defaults to 12
for k in 1 .. steps-1:
    t = k / steps
    sx = eyeX + dx * t
    sy = eyeY + dy * t
    if TileCollision::isSolidTile(map, layer, tx(sx), ty(sy)):
        return blocked
return clear
```

We skip the endpoints (`k = 0` and `k = steps`) because those
points sit on top of the enemy and the player respectively, and
if either occupies a solid tile we shouldn't blame LOS for it.

Step size trade-off: 12 px is ~⅜ of a tile. Small enough that a
single 32-px pillar between two floors reliably blocks sight;
large enough that we're not doing hundreds of samples per frame
per enemy. Bump to 8 for very thin solids, 16 for looser walls.

### 1e. Ghost bypass — one guard, top of tick

To keep the Ghost carve-out from leaking, `sense()` reads the
player's mode once at entry:

```cpp
void Enemy::sense(const Player& player, ...) {
    if (player.mode() == Player::Mode::Ghost) {
        // hearing and sight both dark; searching / patrol still tick.
        m_lastHeard = false;
        m_lastSeen  = false;
        return;
    }
    m_lastHeard = hearsPlayer(player);
    m_lastSeen  = seesPlayer(player, map, layer);
}
```

Every state transition reads `m_lastHeard` / `m_lastSeen`, so a
single early-out at the top of `sense()` is enough to guarantee
"Ghost never gets seen or heard". The state machine itself keeps
running — patrols continue, an in-progress Return-to-post
finishes — but no new aggro is gained.

### 1f. Physics: what changes vs Item 08

Item 08 spawned the enemy at a fixed `(x, groundY)` and never
moved it. Item 09 gives the enemy a full-fat physics body:

* An AABB `m_box` sized ~24 × 40 px (roughly matches the visible
  sprite silhouette).
* `m_vx`, `m_vy` velocities.
* Gravity applied each tick.
* Ground-detection via `MoveResult2D::grounded`.
* `syncFromBox()` copies `m_box.cy - h/2` back into `m_feetY`
  each tick so the hurtbox (which is feet-anchored) tracks the
  new position.

The physics body is fed through `TileCollision::moveAndCollideEx`
with the same knobs as the player: step-up 0 (no stair climbing
for enemies in Item 09), one-way ON, slopes ON.

---

## 2. `include/Game/Enemy.h` — the full extended header

Replace the entire file with the following. Item 08's tunables
(`startHp`, `damagePerHit`, hurtbox/hitbox halves) stay; all
additions are grouped after them.

```cpp
#pragma once

#include "HBE/Renderer/Sprite2D.h"
#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/TileCollision.h"   // AABB, TileMap, TileMapLayer, MoveResult2D

// Do NOT include ParticleSystem.h, Scene2D.h or CombatSystem.h here;
// see 07-Particle-Effects/01_effects_configs_and_class.md for why.

namespace HBE::Renderer {
    class ResourceCache;
    class Mesh;
    class GLShader;
    class Renderer2D;
}

namespace MegaX {

    class Player;   // forward-decl: Enemy needs a const Player& for sensing

    class Enemy {
    public:
        // -----------------------------------------------------------------
        // AI states
        // -----------------------------------------------------------------
        enum class AIState {
            Patrol,        // walking between patrolLeftX and patrolRightX
            Suspicious,    // heard something: turned to face, "?" over head
            Alert,         // saw the player: "!" over head, latching
            Chase,         // running/jumping toward the player
            Search,        // player lost: walk to last-known position, dwell
            Return         // walk back to nearest patrol endpoint
        };

        enum class BubbleIcon { None, Question, Exclaim };

        // -----------------------------------------------------------------
        // Setup / lifecycle
        // -----------------------------------------------------------------
        bool init(HBE::Renderer::ResourceCache& resources,
                  HBE::Renderer::Mesh* quadMesh,
                  HBE::Renderer::GLShader* spriteShader);

        void spawn(float x, float groundY, int facing);

        // Configure the horizontal patrol range in world pixels. `leftX <=
        // rightX`. `waitSec` is how long the enemy stands still at each end
        // before turning around. If leftX == rightX the enemy stands still
        // forever (useful for guard posts) but keeps sensing.
        void setPatrolPath(float leftX, float rightX, float waitSec = 1.0f);

        // Tile world for collision + LOS. The layer is your solid Ground
        // layer, same one the player uses.
        void setCollision(const HBE::Renderer::TileMap* map,
                          const HBE::Renderer::TileMapLayer* solidLayer) {
            m_map = map;
            m_solid = solidLayer;
        }

        // Called by EnemyManager::update once per frame. Owns all physics,
        // sensing and state-machine transitions.
        void tick(float dt, const Player& player);

        void render(HBE::Renderer::Renderer2D& r2d);

        // Called by EnemyManager when a gunshot goes off within range. The
        // enemy immediately upgrades to Suspicious (or Alert if it already
        // has LOS) and faces the sound.
        void onHeardGunshot(float sourceX, float sourceY);

        // -----------------------------------------------------------------
        // Damage / life-cycle (unchanged from Item 08)
        // -----------------------------------------------------------------
        bool takeDamage(int amount);

        HBE::Renderer::AABB hurtbox() const;
        HBE::Renderer::AABB hitbox() const;

        bool hurtboxActive() const { return !m_dead; }
        bool hitboxActive()  const { return m_hitboxActive && !m_dead; }

        bool isAlive()    const { return !m_dead; }
        bool isDying()    const { return m_dead && m_deathTimer > 0.0f; }
        bool isFinished() const { return m_dead && m_deathTimer <= 0.0f; }

        int  hp()    const { return m_hp; }
        int  maxHp() const { return m_maxHp; }

        // -----------------------------------------------------------------
        // Introspection (for debug overlay / bubbles)
        // -----------------------------------------------------------------
        AIState    aiState()   const { return m_state; }
        BubbleIcon bubbleIcon() const;
        int   facing() const { return m_facing; }
        float x() const { return m_x; }
        float y() const { return m_y; }
        float feetY() const { return m_feetY; }

        // Eye position — top of the vision cone. Used by debug draw
        // for the cone origin and by the sight raycast.
        float eyeX() const { return m_x; }
        float eyeY() const { return m_feetY + eyeHeightAboveFeet; }

        // Tunables for the debug overlay to visualize.
        float visionRange()      const { return sightRange; }
        float visionHalfAngle()  const { return sightHalfAngleDeg; }
        float hearingRingRadius() const { return hearingRadius; }

        // -----------------------------------------------------------------
        // Tunables (public — safe to edit at runtime / from GameLayer)
        // -----------------------------------------------------------------

        // -- combat (unchanged from Item 08) --
        int   startHp        = 3;
        int   damagePerHit   = 1;
        float invulnAfterHit = 0.08f;
        float hitFlashTime   = 0.10f;
        float deathFadeTime  = 0.60f;

        // Hurtbox / hitbox (unchanged from Item 08)
        float hurtHalfW      = 11.0f;
        float hurtHalfH      = 18.0f;
        float hurtOffsetX    =  0.0f;
        float hurtOffsetY    =  0.0f;

        float hitHalfW       = 20.0f;
        float hitHalfH       = 14.0f;
        float hitOffsetX     = 20.0f;
        float hitOffsetY     =  0.0f;
        int   hitDamage      = 1;

        // -- movement / physics --
        float moveSpeed      = 60.0f;    // patrol walk speed (px/s)
        float chaseSpeed     = 120.0f;   // sprint while chasing
        float gravity        = 2100.0f;
        float jumpSpeed      = 520.0f;
        float maxFall        = 900.0f;
        float chaseJumpCooldown = 0.5f;  // min seconds between chase jumps

        // Feet-anchored collision box. Enemy is roughly 24 x 40 px.
        float boxHalfW       = 12.0f;
        float boxHalfH       = 20.0f;
        // Where the visible sprite bottom sits relative to the box bottom.
        // 0 means "sprite frame bottom == box bottom" (matches player).
        float spriteFeetOffsetY = 0.0f;

        // -- patrol --
        float patrolWaitAtEnd = 1.0f;    // seconds paused at each endpoint

        // -- hearing --
        float hearingRadius        = 140.0f;   // pixels
        float minPlayerVxToHear    =  40.0f;   // px/s: below this we don't hear
        float gunshotHearRadius    = 380.0f;   // radius when a gunshot pings us

        // -- sight --
        float sightRange           = 260.0f;   // pixels
        float sightHalfAngleDeg    =  35.0f;   // half-angle of vision cone
        float eyeHeightAboveFeet   =  32.0f;   // eye Y relative to feet

        // -- state timers / transition tunables --
        float suspicionDuration    = 1.20f;    // "?" bubble time before giving up
        float alertLatchTime       = 0.20f;    // saw player must persist this long
        float loseAggroDelay       = 3.00f;    // hidden + crouched seconds to lose
        float searchDwellTime      = 2.50f;    // walk-to-last-known + look around

        // -- LOS sample step --
        float losStepPx            = 12.0f;    // smaller = tighter walls block LOS

    private:
        // --- animation helpers ---
        enum class AnimState { Idle, Walk };
        void setAnimState(AnimState s);
        HBE::Renderer::SpriteAnimation& currentAnim();

        // --- physics ---
        void applyPhysics(float dt);
        void syncFromBox();              // m_box -> m_x/m_feetY

        // --- sensing ---
        bool hearsPlayer(const Player& p) const;
        bool seesPlayer(const Player& p) const;
        bool losClear(float ax, float ay, float bx, float by) const;
        bool isSolidTileAt(float wx, float wy) const;

        // --- patrol helpers ---
        bool wallInFront() const;
        bool ledgeInFront() const;
        int  desiredPatrolFacing() const;   // which way to walk to reach target end

        // --- state helpers ---
        void enter(AIState s);
        void faceX(float targetX);          // sets m_facing to point at targetX

        // ---- position ----
        float m_x = 0.0f;
        float m_y = 0.0f;
        float m_feetY = 0.0f;

        int   m_facing = -1;

        // ---- physics body ----
        HBE::Renderer::AABB m_box{};
        float m_vx = 0.0f;
        float m_vy = 0.0f;
        bool  m_grounded = false;
        float m_prevBottom = 0.0f;   // for one-way tile landing

        // ---- combat ----
        int   m_hp    = 3;
        int   m_maxHp = 3;
        float m_invulnTimer = 0.0f;
        float m_flashTimer  = 0.0f;
        float m_deathTimer  = 0.0f;
        bool  m_dead        = false;
        bool  m_hitboxActive = false;

        // ---- ai state machine ----
        AIState m_state = AIState::Patrol;
        float m_stateTimer     = 0.0f;   // generic per-state timer
        float m_alertLatch     = 0.0f;   // time we've been seeing the player
        float m_hiddenTimer    = 0.0f;   // time player has been hidden+crouched
        float m_jumpCooldown   = 0.0f;
        bool  m_lastHeard = false;
        bool  m_lastSeen  = false;
        float m_lastKnownX = 0.0f;
        float m_lastKnownY = 0.0f;

        // ---- patrol ----
        float m_patrolLeftX  = 0.0f;
        float m_patrolRightX = 0.0f;
        int   m_patrolTargetSign = -1;   // -1 = heading to left end, +1 = right
        float m_patrolWaitTimer  = 0.0f;

        // ---- world refs (not owned) ----
        const HBE::Renderer::TileMap*      m_map   = nullptr;
        const HBE::Renderer::TileMapLayer* m_solid = nullptr;

        // ---- animation ----
        HBE::Renderer::SpriteSheet     m_idleSheet{};
        HBE::Renderer::SpriteSheet     m_walkSheet{};
        HBE::Renderer::SpriteAnimation m_idleAnim;
        HBE::Renderer::SpriteAnimation m_walkAnim;
        AnimState m_animState = AnimState::Idle;

        HBE::Renderer::Material   m_material{};
        HBE::Renderer::RenderItem m_item{};
    };
}
```

Notes on the header shape:

* **`Player` is only a forward declaration.** `Enemy.cpp`
  includes `Game/Player.h`, but `Enemy.h` mustn't — Player.h
  transitively drags in things we don't want in this header.
  Forward declaration is enough because `sense/tick` take
  `const Player&`.
* **`BubbleIcon` is derived, not stored.** The state machine
  changes state; `bubbleIcon()` maps `Suspicious → Question`
  and `Alert / Chase / Search → Exclaim`. Everything else is
  `None`.
* **Every AI knob is a public field, not a #define.** That way
  you can tune from `GameLayer` at spawn (`e->sightRange =
  180.0f;`) without recompiling `Enemy.cpp`.
* **`onHeardGunshot` is separate from `sense/tick`.** The
  manager calls it directly on each in-range enemy the frame
  the player fires. Keeping it out of the normal sensor path
  avoids a per-enemy per-frame "did the player fire" check.

---

Next: `02_enemy_state_machine.md` — the full `Enemy.cpp`
implementing patrol, senses, LOS, chase, search, and return.
