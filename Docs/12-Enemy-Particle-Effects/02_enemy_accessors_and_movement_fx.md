# 02 — `Enemy` self-observation + `EnemyManager` FX dispatch

Doc 01 gave us the raw `Effects::spawn...` calls. Doc 02
teaches the `Enemy` how to tell the `EnemyManager` *when*
each of those calls should fire, and wires the manager to
call them at the right moment.

The pattern is intentionally symmetrical with how Item 07
handled the player: the *state owner* (Enemy) exposes
one-shot latches and per-frame observations, and the
*orchestrator* (EnemyManager) reads them and dispatches to
the `Effects*` it's holding. Same shape, different domain.

Files touched by doc 02:

* `G:\Dev\HBE\MegaX\include\Game\Enemy.h`
* `G:\Dev\HBE\MegaX\src\Game\Enemy.cpp`
* `G:\Dev\HBE\MegaX\include\Game\EnemyManager.h`
* `G:\Dev\HBE\MegaX\src\Game\EnemyManager.cpp`

---

## 1. `include/Game/Enemy.h` — four new accessors + private fields

Open `G:\Dev\HBE\MegaX\include\Game\Enemy.h`.

### 1a. New public accessors

Find the existing accessor block (right around
`bool isFinished() const { return m_dead && m_deathTimer <= 0.0f; }`).
Add the four Item 12 accessors **immediately below**
`isFinished()`:

```cpp
        bool isFinished() const { return m_dead && m_deathTimer <= 0.0f; }

        // -------- Item 12: self-observation for Effects dispatch --------

        // True on the SINGLE tick m_grounded transitioned false -> true
        // (a landing). Cleared at the top of the next tick().
        bool landedThisFrame() const { return m_landedThisFrame; }

        // Tile id currently beneath the enemy's feet (0 if not grounded
        // or if the layer has no tile there). Sampled inside applyPhysics
        // when m_grounded becomes true, and updated once per frame while
        // grounded so consumers see the current terrain.
        int groundTileId() const { return m_groundTileId; }

        // True on the SINGLE tick HP dropped to 0 inside takeDamage. Set
        // by takeDamage; cleared by EnemyManager::update via
        // consumeJustDied() after the explosion has been spawned.
        bool justDied() const { return m_justDied; }
        void consumeJustDied() { m_justDied = false; }

        // Per-enemy walk-dust cadence. Feed it dt every frame; the
        // helper returns true at most once every `period` seconds, only
        // when grounded AND horizontally moving fast enough. Resets the
        // accumulator whenever the enemy is airborne or stationary.
        bool consumeWalkDustPuff(float dt, float period);
```

### 1b. New private fields

Find the existing private-field block (down near `int m_hp`,
`float m_deathTimer`, etc.). Add these fields **at the bottom
of the same block**, before the closing `};`:

```cpp
            // -------- Item 12: FX dispatch state --------
            bool  m_landedThisFrame  = false;   // one-shot per landing
            bool  m_wasGroundedLast  = true;    // last frame's m_grounded snapshot
            int   m_groundTileId     = 0;       // tile currently under feet
            bool  m_justDied         = false;   // one-shot per death
            float m_walkDustAccum    = 0.0f;    // per-enemy cadence
```

That's every `Enemy.h` change in Item 12.

---

## 2. `src/Game/Enemy.cpp` — clear latches, sample ground, detect landing

Open `G:\Dev\HBE\MegaX\src\Game\Enemy.cpp`.

### 2a. Clear one-shot latches at top of `tick`

Find the current top of `Enemy::tick(...)`:

```cpp
    void Enemy::tick(float dt, const Player& player) {
        if (m_invulnTimer > 0.0f) m_invulnTimer = std::max(0.0f, m_invulnTimer - dt);
        if (m_flashTimer > 0.0f)  m_flashTimer  = std::max(0.0f, m_flashTimer  - dt);
        if (m_jumpCooldown > 0.0f) m_jumpCooldown = std::max(0.0f, m_jumpCooldown - dt);
```

Add ONE line **immediately above** the first `if (m_invulnTimer ...)`:

```cpp
    void Enemy::tick(float dt, const Player& player) {
        m_landedThisFrame = false;   // Item 12: latch is one frame only

        if (m_invulnTimer > 0.0f) m_invulnTimer = std::max(0.0f, m_invulnTimer - dt);
```

We do NOT clear `m_justDied` here — the manager clears it via
`consumeJustDied()` after dispatching the explosion. If we
cleared it here on the frame after death, the manager might
miss it entirely on that first post-death tick (order of
operations in `EnemyManager::update` runs `tick` first, then
inspects `justDied()`). Setting it in `takeDamage` and
clearing on consume is the safe pattern.

### 2b. Set `m_justDied` inside `takeDamage`

Find the existing:

```cpp
    bool Enemy::takeDamage(int amount) {
        if (m_dead || m_invulnTimer > 0.0f || amount <= 0) return false;
        m_hp -= amount;
        m_invulnTimer = invulnAfterHit;
        m_flashTimer = hitFlashTime;
        if (m_hp <= 0) {
            m_hp = 0;
            m_dead = true;
            m_hitboxActive = false;
            m_deathTimer = deathFadeTime;
            m_vx = 0.0f;
        }
        return true;
    }
```

Add ONE line inside the `m_hp <= 0` block, **immediately
below** `m_dead = true;`:

```cpp
        if (m_hp <= 0) {
            m_hp = 0;
            m_dead = true;
            m_justDied = true;   // Item 12: latch cleared by manager
            m_hitboxActive = false;
            m_deathTimer = deathFadeTime;
            m_vx = 0.0f;
        }
```

### 2c. Landing detection + ground sample in `applyPhysics`

Find the bottom of `Enemy::applyPhysics(...)`:

```cpp
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
    }
```

Replace the whole `if (m_map && m_solid) { ... }` block plus
the trailing `syncFromBox()` with this expanded version:

```cpp
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

        // Item 12: landing detection + tile-under-feet sample.
        // A "landing" is a transition from airborne to grounded, with a
        // downward-ish velocity on the frame before (so a sideways bump
        // into the ground while already y-grounded doesn't count).
        // m_vy is the physics velocity AFTER moveAndCollideEx clamps it;
        // a landing frame typically resolves to m_vy ~ 0, so check the
        // transition itself, not the sign.
        if (!prevGrounded && m_grounded) {
            m_landedThisFrame = true;
        }
        m_wasGroundedLast = m_grounded;

        // Ground-tile sample: probe one pixel below feet (m_feetY - 1),
        // read the tile id at that world position. 0 if airborne.
        if (m_grounded && m_map && m_solid) {
            const float tw = m_map->worldTileW();
            const float th = m_map->worldTileH();
            if (tw > 0.0f && th > 0.0f) {
                const float probeY = m_feetY - 1.0f;
                const int tx = static_cast<int>(std::floor(m_x / tw));
                const int ty = static_cast<int>(std::floor(probeY / th));
                m_groundTileId = m_solid->at(tx, ty);
            }
        } else {
            m_groundTileId = 0;
        }
    }
```

The `prevGrounded = m_wasGroundedLast;` snapshot is taken
before the move-and-collide call. The snapshot member itself
is updated *after* the resolve so the next frame sees this
frame's post-resolve grounded state.

The tile probe uses `m_feetY - 1.0f` (one pixel below feet)
because `m_feetY` sits exactly at the top of the tile the
enemy is standing on — sampling AT that pixel might hit the
tile above (empty). The `-1` puts the probe firmly inside the
solid tile. This mirrors the player's ground-sample logic
from Item 07.

`m_wasGroundedLast` defaults to `true` in the header (matches
`Enemy::spawn` which sets `m_grounded = true`) so the very
first tick does NOT falsely trigger a landing.

### 2d. Reset FX latches inside `Enemy::spawn`

Find the existing `Enemy::spawn(...)`:

```cpp
        m_hp = m_maxHp;
        m_invulnTimer = m_flashTimer = m_deathTimer = 0.0f;
        m_dead = false;
```

Add the Item 12 resets right below the `m_dead = false;` line
(or wherever the death/spawn state cluster ends):

```cpp
        m_dead = false;

        // Item 12: FX latches / accumulator start clean at (re)spawn.
        m_landedThisFrame = false;
        m_wasGroundedLast = true;   // matches m_grounded = true above
        m_groundTileId    = 0;
        m_justDied        = false;
        m_walkDustAccum   = 0.0f;
```

### 2e. Implement `consumeWalkDustPuff`

Add this function near the bottom of `Enemy.cpp` (a good spot
is right after `muzzleWorldPos(...)` from Item 10):

```cpp
    bool Enemy::consumeWalkDustPuff(float dt, float period) {
        // Match the player's tickWalkDust semantics: reset the
        // accumulator whenever the enemy isn't cleanly grounded and
        // moving. This prevents a "burst on landing" bug where a
        // moving-in-air enemy suddenly puffs the moment it grounds.
        const bool moving   = std::fabs(m_vx) > 5.0f;
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
```

Same 5px-per-frame threshold on `|m_vx|` the anim state
already uses (grep `std::fabs(m_vx)` in the file to confirm)
— keeps the FX cadence and the anim state in sync.

---

## 3. `include/Game/EnemyManager.h` — hold an `Effects*`, expose setter

Open `G:\Dev\HBE\MegaX\include\Game\EnemyManager.h`.

### 3a. Add the setter declaration + tunable

Find the existing public method block that contains
`void setPlayerRef(const Player* p) { m_player = p; }` (from
Item 09). Add the new setter **immediately below** it:

```cpp
        void setPlayerRef(const Player* p) { m_player = p; }
        void setCollision(const HBE::Renderer::TileMap* map,
            const HBE::Renderer::TileMapLayer* solidLayer);

        // Item 12: attach a game-side Effects instance. The manager does
        // not own it; GameLayer does. Set to nullptr to disable all
        // enemy-side FX dispatch (safe -- every dispatch site null-checks).
        void setEffects(class Effects* fx) { m_effects = fx; }
        Effects* effects() const { return m_effects; }
```

`class Effects;` is already forward-declared in the header
(Item 10 forward-declared it for `checkBulletHits`); no new
forward decl needed.

Also add a tunable for the walk-dust cadence right next to
the other public tunables (there aren't many at this scope —
put it above `int aliveCount() const;`):

```cpp
        // Item 12: per-frame walk dust cadence for grounded enemies.
        float walkDustPeriod = 0.18f;   // matches Effects::walkDustPeriod
```

### 3b. Make `m_effects` accessible from the fire callback

Because the `Enemy::FireFn` signature is a raw
`void(*)(void*, float, ...)` function pointer, the wrapping
lambda in `spawn(...)` is non-capturing. It reaches the
manager via `static_cast<EnemyManager*>(ctx)` and dereferences
`ctx->m_effects`. That means `m_effects` needs to be
accessible from that lambda.

Two clean options:

* Move `m_effects` into the `public:` section. It's a raw
  non-owning pointer, so exposing it doesn't leak ownership.
* Add `friend struct EnemyFireCallback;` and put the lambda
  behind that struct.

Item 12 uses the first option — simpler, and every raw
non-owning pointer already exposed on this class is public in
practice (e.g., the manager currently reaches
`m_enemyBullets` via `static_cast<EnemyManager*>(ctx)->m_enemyBullets->spawn(...)`
which requires `m_enemyBullets` to be at least accessible to
that TU; making both public is consistent).

Locate the private block:

```cpp
    private:
        HBE::Renderer::ResourceCache* m_resources = nullptr;
        HBE::Renderer::Mesh* m_quadMesh = nullptr;
        HBE::Renderer::GLShader* m_spriteShader = nullptr;

        // Stored refs (not owned)
        const Player* m_player = nullptr;
        const HBE::Renderer::TileMap* m_map = nullptr;
        const HBE::Renderer::TileMapLayer* m_solid = nullptr;
```

Add the `Effects*` field to that same "not owned" cluster:

```cpp
        // Stored refs (not owned)
        const Player* m_player = nullptr;
        const HBE::Renderer::TileMap* m_map = nullptr;
        const HBE::Renderer::TileMapLayer* m_solid = nullptr;
        Effects* m_effects = nullptr;                          // Item 12
```

Because the lambda in `spawn(...)` is a friend-free
non-capturing lambda, it needs `m_effects` and `m_enemyBullets`
to be reachable via `static_cast<EnemyManager*>(ctx)->...`.
Both are private members of the same class, but the lambda is
defined *inside* an `EnemyManager` member function
(`spawn(...)`) — private access is granted to it via the
enclosing scope. So no `public:` move is needed; leaving both
private is fine.

If you get `error C2248 'EnemyManager::m_effects': cannot access private member`
during the build, the lambda escaped the enclosing member-function
scope (e.g., you defined it as a free function at file scope).
Re-inline it inside `spawn(...)`.

---

## 4. `src/Game/EnemyManager.cpp` — wrap fire callback + dispatch FX in `update`

Open `G:\Dev\HBE\MegaX\src\Game\EnemyManager.cpp`.

### 4a. Wrap the fire callback in `spawn(...)`

Find the existing:

```cpp
        e.setFireCallback(
            [](void* ctx, float sx, float sy, float aimX, float aimY, float speed, int damage) {
                static_cast<EnemyManager*>(ctx)->m_enemyBullets->spawn(sx, sy, aimX, aimY, speed, damage);
            },
            this);
```

Replace it with the extended wrapper that spawns the muzzle
flash *before* the bullet:

```cpp
        e.setFireCallback(
            [](void* ctx, float sx, float sy, float aimX, float aimY, float speed, int damage) {
                auto* mgr = static_cast<EnemyManager*>(ctx);
                const int dir = (aimX >= sx) ? +1 : -1;
                if (mgr->m_effects) {
                    mgr->m_effects->spawnEnemyMuzzleFlash(sx, sy, dir);
                }
                mgr->m_enemyBullets->spawn(sx, sy, aimX, aimY, speed, damage);
            },
            this);
```

Order matters: the flash spawns AT the muzzle position (sx,
sy). If we did the bullet first, the visual timing is
identical (both happen inside the same frame), but the code
reads better with the flash before the bullet.

### 4b. Dispatch walk/land/explosion in `update(...)`

Find the current `EnemyManager::update(...)`:

```cpp
    void EnemyManager::update(float dt) {
        if (!m_player) return;

        for (auto& e : m_enemies) e.tick(dt, *m_player);
        m_enemies.erase(
            std::remove_if(m_enemies.begin(), m_enemies.end(),
                [](const Enemy& e) { return e.isFinished(); }),
            m_enemies.end());
    }
```

Replace with the expanded version that dispatches Item 12
effects between `tick` and the erase-remove:

```cpp
    void EnemyManager::update(float dt) {
        if (!m_player) return;

        for (auto& e : m_enemies) e.tick(dt, *m_player);

        // Item 12: dispatch per-frame FX events.
        for (auto& e : m_enemies) {
            if (e.isFinished()) continue;

            // Death explosion -- one-shot on the frame HP hit zero.
            if (e.justDied() && m_effects) {
                m_effects->spawnEnemyExplosion(e.x(), e.feetY() + e.boxHalfH);
                e.consumeJustDied();
            }

            // Only alive enemies emit dust. `isAlive()` returns false
            // while the death fade is in progress, so this correctly
            // skips a dying enemy that still has HP == 0 for the fade
            // duration.
            if (!e.isAlive()) continue;

            if (e.landedThisFrame() && m_effects) {
                m_effects->spawnLandingDust(e.x(), e.feetY(), e.groundTileId());
            }

            if (m_effects && e.consumeWalkDustPuff(dt, walkDustPeriod)) {
                m_effects->spawnWalkDustBurst(e.x(), e.feetY(), e.groundTileId());
            }
        }

        m_enemies.erase(
            std::remove_if(m_enemies.begin(), m_enemies.end(),
                [](const Enemy& e) { return e.isFinished(); }),
            m_enemies.end());
    }
```

Explosion is placed BEFORE the `!isAlive()` early-out because
`justDied()` returns true on the frame HP hit zero, and
`isAlive()` will already be returning false by that point
(`takeDamage` sets `m_dead = true` before returning). If we
skipped dying enemies first, the explosion would never spawn.

The dust dispatches ARE gated by `isAlive()` because we don't
want a dying enemy still tossing walk-dust as its momentum
carries it a few pixels forward during the fade.

### 4c. `consumeJustDied` accessibility

`Enemy::consumeJustDied()` is public (declared in doc 02 §1a
as `void consumeJustDied() { m_justDied = false; }`). The
manager calls it through the mutable reference — no friend
declarations needed.

---

## 5. Sanity check at end of doc 02

At this point:

* `Effects::init` still works unchanged (doc 01 §3 added the
  five new registrations).
* `Enemy::tick` clears `m_landedThisFrame` at the top of the
  frame; `Enemy::takeDamage` sets `m_justDied` on kill.
* `Enemy::applyPhysics` detects landings and samples the
  tile under feet.
* `EnemyManager::spawn` wraps the fire callback with a muzzle-
  flash dispatch.
* `EnemyManager::update` dispatches landing dust, walk dust,
  and death explosions per enemy per frame.

**Still not wired**: 
* `EnemyManager::setEffects(...)` is not yet called from
  `GameLayer::onAttach` — that's doc 03 §1.
* Blood splatter on player hit — doc 03 §3.
* Enemy bullet impact drain — doc 03 §2.
* Hit spark on player-bullet-hits-enemy (swap in
  `checkBulletHits`) — doc 03 §4.

You should be able to build cleanly here, but running the
game will show no visible new effects yet (the manager has a
null `m_effects` — every dispatch site skips silently).

Common errors:

| Symptom | Cause | Fix |
|---|---|---|
| `error C2039 'landedThisFrame': is not a member of 'MegaX::Enemy'` | `Enemy.h` not saved. | Save + rebuild. |
| `error C2039 'setEffects': is not a member of 'MegaX::EnemyManager'` | `EnemyManager.h` not saved. | Save + rebuild. |
| `error C2248 'EnemyManager::m_effects': cannot access private member` | The lambda got moved out of `EnemyManager::spawn`. Non-capturing lambdas defined inside a member function inherit access. | Re-inline the lambda inside `spawn(...)`. |
| Enemies never emit walk dust | `consumeWalkDustPuff` sees `m_vx == 0` because the patrol tick zeros velocity between waypoints. | Correct — that's the design. Enemies mid-wait at a patrol endpoint don't puff dust. Only walking frames emit. |
| Landings emit twice | `m_landedThisFrame` is being cleared somewhere other than top-of-tick. | Grep `m_landedThisFrame` — should be set only in `applyPhysics` (transition) and cleared only at top of `tick`. |
| Explosion fires every frame during death fade | `m_justDied` not being cleared by `consumeJustDied()` from the manager, OR `takeDamage` is being called again on an already-dead enemy. | The early-out `if (m_dead || ... ) return false;` in `takeDamage` prevents re-triggering. Verify `consumeJustDied` is called in the manager. |
| Explosion never fires | `justDied()` accessor mis-typed as `isJustDied()` or similar. Grep — the accessor is `justDied()`. |
| Ground tile sample returns 0 for a clearly-solid tile | The `-1.0f` probe delta landed the sample outside the tile. Increase to `-2.0f` if your `feetY` doesn't sit flush at the tile top. | Rare but possible with non-32px tilesets. |

---

Next: `03_hits_impacts_and_wiring.md` — enemy bullet impact
tracking, blood splatter wiring in `GameLayer`, hit spark
swap in `checkBulletHits`, and the single-line
`m_enemies.setEffects(&m_effects)` call in `onAttach`.
