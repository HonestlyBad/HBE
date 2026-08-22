# 01 — Difficulty profile, `EnemyManager` plumbing, `Enemy::applyDifficulty`

This doc covers three things:

1. The `Difficulty` enum + `DifficultyProfile` struct + the
   three baked profiles (Casual / Difficult / Challenging) —
   these live on `EnemyManager`.
2. `EnemyManager::setDifficulty(...)` and the manager-side hook
   that re-applies the profile to every enemy currently in the
   vector plus every future `spawn(...)`.
3. `Enemy::applyDifficulty(profile)` and the `m_baseXxx` stat
   snapshot pattern that keeps switching difficulties
   idempotent (F1 → F3 → F1 lands back on Casual values, not
   Casual × Challenging × Casual).

Item 09's `Enemy.h`, `Enemy.cpp`, `EnemyManager.h` and
`EnemyManager.cpp` are the starting point. Every code block in
this doc is a **surgical add** or targeted replacement; nothing
from Item 09 should be deleted.

---

## 1. `EnemyManager.h` — new types + method

Add these blocks to `include/Game/EnemyManager.h`.

### 1a. New `Difficulty` enum and `DifficultyProfile` struct

Insert **just below** the existing `namespace MegaX {` line and
**above** the forward-declared `class BulletManager;` block.

```cpp
    // -----------------------------------------------------------------
    // Difficulty (Item 10)
    // -----------------------------------------------------------------
    enum class Difficulty {
        Casual,
        Difficult,     // default
        Challenging
    };

    // Multipliers and raw values applied to every Enemy when the
    // difficulty is (re)set. Multipliers scale each enemy's
    // designer-base stats (captured at spawn); raw values overwrite the
    // corresponding fields wholesale.
    struct DifficultyProfile {
        // Scalars over base stats (1.0 == no change from designer defaults)
        float chaseSpeedMul     = 1.0f;
        float sightRangeMul     = 1.0f;
        float hearingRadiusMul  = 1.0f;
        float loseAggroDelayMul = 1.0f;
        float startHpMul        = 1.0f;

        // Raw values applied wholesale each time the profile is set.
        int   bulletDamage      = 1;
        float fireCooldownSec   = 0.90f;
        float bulletSpeed       = 480.0f;
        float leadFactor        = 0.0f;      // 0 = no lead; 1 = full velocity lead

        // Bookkeeping so the HUD can label + tint the difficulty pip.
        const char* label       = "Difficult";
        float labelR = 1.0f, labelG = 0.9f, labelB = 0.25f;   // yellow
    };

    // The three baked profiles the game ships with.
    DifficultyProfile MakeProfile(Difficulty d);
```

### 1b. Extend the class

Inside `class EnemyManager` add these public members. The exact
insertion point is right after `notifyGunshot(...)`:

```cpp
        // Item 10: current difficulty + profile setter.
        // Calling setDifficulty(...) does three things:
        //   1. stores the profile,
        //   2. re-applies it to every already-spawned Enemy (idempotent
        //      via the base-stat snapshot in Enemy),
        //   3. any future spawn() automatically applies the same profile.
        void setDifficulty(Difficulty d);
        Difficulty        difficulty()        const { return m_difficulty; }
        const DifficultyProfile& profile()    const { return m_profile; }

        // Item 10: enemy-side bullets. GameLayer calls update on this each
        // frame after this manager's update(). See doc 02 for the manager
        // internals.
        class EnemyBulletManager& enemyBullets() { return *m_enemyBullets; }
        const class EnemyBulletManager& enemyBullets() const { return *m_enemyBullets; }
```

Then in the private section, add two new fields **and** switch
the enemy-bullet manager to a `unique_ptr` so we don't need to
include `EnemyBullet.h` from `EnemyManager.h` (same header
hygiene rule as `Effects.h`):

```cpp
        Difficulty        m_difficulty = Difficulty::Difficult;
        DifficultyProfile m_profile{};

        // Held as unique_ptr so we don't leak the include into this header.
        std::unique_ptr<class EnemyBulletManager> m_enemyBullets;
```

And add `#include <memory>` at the top of `EnemyManager.h` (next
to `#include <vector>`).

### 1c. Full expected top of `EnemyManager.h` after edits

For quick sanity check, the top of the file should now look like:

```cpp
#pragma once

#include "Game/Enemy.h"

#include <memory>
#include <vector>

namespace HBE::Renderer {
    class ResourceCache;
    class Mesh;
    class GLShader;
    class Renderer2D;
    class DebugDraw2D;
    struct TileMap;
    struct TileMapLayer;
}

namespace MegaX {

    // ---------- Item 10 difficulty ----------
    enum class Difficulty { Casual, Difficult, Challenging };

    struct DifficultyProfile {
        float chaseSpeedMul     = 1.0f;
        float sightRangeMul     = 1.0f;
        float hearingRadiusMul  = 1.0f;
        float loseAggroDelayMul = 1.0f;
        float startHpMul        = 1.0f;
        int   bulletDamage      = 1;
        float fireCooldownSec   = 0.90f;
        float bulletSpeed       = 480.0f;
        float leadFactor        = 0.0f;
        const char* label = "Difficult";
        float labelR = 1.0f, labelG = 0.9f, labelB = 0.25f;
    };

    DifficultyProfile MakeProfile(Difficulty d);

    class BulletManager;
    class Effects;
    class Player;

    class EnemyManager {
    public:
        // ... unchanged Item 09 API ...
        void setDifficulty(Difficulty d);
        Difficulty difficulty() const { return m_difficulty; }
        const DifficultyProfile& profile() const { return m_profile; }
        class EnemyBulletManager& enemyBullets();
        const class EnemyBulletManager& enemyBullets() const;

    private:
        // ... unchanged ...
        Difficulty m_difficulty = Difficulty::Difficult;
        DifficultyProfile m_profile{};
        std::unique_ptr<class EnemyBulletManager> m_enemyBullets;
    };
}
```

---

## 2. `EnemyManager.cpp` — `MakeProfile`, ctor plumbing, `setDifficulty`

`EnemyManager.cpp` gains three pieces. Doc 02 will layer the
enemy-bullet forwarding on top of these.

### 2a. Include and forward the new header

At the top of the file, add:

```cpp
#include "Game/EnemyBullet.h"    // full include is fine in the .cpp
```

### 2b. `MakeProfile` — the three baked profiles

Add above the anonymous `namespace MegaX {` closing brace, or
just after the `using namespace HBE::Renderer;` line — either
works. Suggested placement is right at the top of the
namespace, so a reader sees "here are the numbers" before the
class implementation.

```cpp
    // -----------------------------------------------------------------
    // Item 10: baked difficulty profiles
    //
    // Notes on the numbers:
    //   * chaseSpeedMul is calibrated against the player's
    //     Player::moveSpeed = 200 px/s and Enemy::chaseSpeed = 120 px/s.
    //     Casual = 0.5x  -> ~60 px/s (half player speed)
    //     Difficult = 1.35x -> ~162 px/s (still slower than player, feels aggressive)
    //     Challenging = 1.85x -> ~222 px/s (marginally faster than player)
    //   * fireCooldownSec: shots per second = 1 / cooldown.
    //     Casual = 1.6s -> ~0.6 shots/s
    //     Difficult = 0.9s -> ~1.1 shots/s
    //     Challenging = 0.55s -> ~1.8 shots/s
    //   * leadFactor: only Challenging predicts player motion.
    // -----------------------------------------------------------------
    DifficultyProfile MakeProfile(Difficulty d) {
        DifficultyProfile p{};
        switch (d) {
            case Difficulty::Casual:
                p.chaseSpeedMul     = 0.50f;
                p.sightRangeMul     = 0.75f;
                p.hearingRadiusMul  = 0.75f;
                p.loseAggroDelayMul = 0.55f;
                p.startHpMul        = 0.66f;   // (int-truncated per enemy)
                p.bulletDamage      = 1;
                p.fireCooldownSec   = 1.60f;
                p.bulletSpeed       = 380.0f;
                p.leadFactor        = 0.0f;
                p.label             = "Casual";
                p.labelR = 0.35f; p.labelG = 1.0f; p.labelB = 0.35f;   // green
                break;

            case Difficulty::Difficult:
                p.chaseSpeedMul     = 1.35f;
                p.sightRangeMul     = 1.10f;
                p.hearingRadiusMul  = 1.10f;
                p.loseAggroDelayMul = 1.20f;
                p.startHpMul        = 1.00f;
                p.bulletDamage      = 1;
                p.fireCooldownSec   = 0.90f;
                p.bulletSpeed       = 480.0f;
                p.leadFactor        = 0.0f;
                p.label             = "Difficult";
                p.labelR = 1.0f; p.labelG = 0.9f; p.labelB = 0.25f;    // yellow
                break;

            case Difficulty::Challenging:
                p.chaseSpeedMul     = 1.85f;
                p.sightRangeMul     = 1.35f;
                p.hearingRadiusMul  = 1.25f;
                p.loseAggroDelayMul = 2.00f;
                p.startHpMul        = 1.66f;
                p.bulletDamage      = 2;
                p.fireCooldownSec   = 0.55f;
                p.bulletSpeed       = 560.0f;
                p.leadFactor        = 1.0f;
                p.label             = "Challenging";
                p.labelR = 1.0f; p.labelG = 0.3f; p.labelB = 0.3f;      // red
                break;
        }
        return p;
    }
```

### 2c. Manager constructor + destructor

Because we added a `unique_ptr<EnemyBulletManager>`, we need a
non-trivial destructor so the compiler doesn't inline it in
translation units that only see the fwd-declared type. Put
these next to `init(...)`:

```cpp
    EnemyManager::EnemyManager() = default;
    EnemyManager::~EnemyManager() = default;
```

And declare them in the header, in the `public:` section, right
above `bool init(...)`:

```cpp
        EnemyManager();
        ~EnemyManager();
```

### 2d. Instantiate the enemy-bullet manager in `init(...)`

Replace the current `init` body with:

```cpp
    bool EnemyManager::init(ResourceCache& resources, Mesh* quadMesh, GLShader* spriteShader) {
        if (!quadMesh || !spriteShader) {
            HBE::Core::LogError("EnemyManager::init: quadMesh or spriteShader is null.");
            return false;
        }
        m_resources    = &resources;
        m_quadMesh     = quadMesh;
        m_spriteShader = spriteShader;

        // Item 10: own the enemy-bullet manager.
        m_enemyBullets = std::make_unique<EnemyBulletManager>();
        if (!m_enemyBullets->init(resources, quadMesh, spriteShader)) {
            HBE::Core::LogError("EnemyManager::init: enemy bullet manager init failed.");
            return false;
        }

        // Default profile.
        m_difficulty = Difficulty::Difficult;
        m_profile    = MakeProfile(m_difficulty);
        return true;
    }
```

### 2e. `enemyBullets()` accessors

```cpp
    EnemyBulletManager& EnemyManager::enemyBullets() { return *m_enemyBullets; }
    const EnemyBulletManager& EnemyManager::enemyBullets() const { return *m_enemyBullets; }
```

### 2f. `setDifficulty` — apply to every alive enemy

```cpp
    void EnemyManager::setDifficulty(Difficulty d) {
        m_difficulty = d;
        m_profile    = MakeProfile(d);
        for (auto& e : m_enemies) {
            if (!e.isAlive()) continue;
            e.applyDifficulty(m_profile);
        }
    }
```

### 2g. Auto-apply the profile inside `spawn(...)`

Update `spawn(...)` so newly-spawned enemies also inherit the
current profile:

```cpp
    Enemy* EnemyManager::spawn(float x, float groundY, int facing) {
        if (!m_resources || !m_quadMesh || !m_spriteShader) return nullptr;
        m_enemies.emplace_back();
        Enemy& e = m_enemies.back();
        if (!e.init(*m_resources, m_quadMesh, m_spriteShader)) {
            m_enemies.pop_back();
            return nullptr;
        }
        if (m_map && m_solid) e.setCollision(m_map, m_solid);
        e.spawn(x, groundY, facing);

        // Item 10: snapshot BASE stats now, then apply the current profile.
        // GameLayer may still override individual fields after this returns
        // (e.g. e->startHp = 5). If it does, call e->snapshotBaseStats()
        // again -- see doc 03 § "GameLayer wiring".
        e.snapshotBaseStats();
        e.applyDifficulty(m_profile);
        return &e;
    }
```

---

## 3. `Enemy.h` — new shooting tunables, base-stat snapshot, API

Item 09's public tunables stay. Add the following to `Enemy.h`:

### 3a. New public tunables

Add underneath the existing shooting-range block:

```cpp
        // -- Item 10: shooting behavior --
        int   bulletDamage      = 1;      // per-bullet damage (profile overrides)
        float fireCooldownSec   = 0.90f;  // seconds between shots (profile overrides)
        float bulletSpeed       = 480.0f; // px/s (profile overrides)
        float leadFactor        = 0.0f;   // 0..1; profile overrides
        float muzzleForwardX    = 22.0f;  // muzzle X offset in facing dir
        float muzzleAboveFeet   = 26.0f;  // muzzle Y above feet (world px)
        float shootingLosStep   = 12.0f;  // LOS step for the fire test
```

### 3b. New public methods

Right below `void onHeardGunshot(...)` in the header:

```cpp
        // Item 10: called by EnemyManager when difficulty changes. Applies
        // the profile multipliers to a saved BASE snapshot (see
        // snapshotBaseStats). Safe to call repeatedly; NOT compounding.
        void applyDifficulty(const struct DifficultyProfile& p);

        // Item 10: captures current designer stats as the base the profile
        // multiplies against. Called once at spawn time by EnemyManager
        // (and again if a caller edits base stats after spawn).
        void snapshotBaseStats();

        // Item 10: world-space muzzle position (facing-aware). Used by
        // EnemyBulletManager when spawning a shot.
        void muzzleWorldPos(float& mx, float& my) const;
```

### 3c. Bullet spawn hook

The `Enemy` needs a way to spawn bullets without owning the
bullet vector. We give it a small function pointer that the
manager wires up. Add a callback field and a setter:

```cpp
        // Item 10: bullet spawn callback set by EnemyManager. Signature
        // matches EnemyBulletManager::spawn.
        using FireFn = void(*)(void* ctx, float sx, float sy,
                               float aimX, float aimY,
                               float speed, int damage);
        void setFireCallback(FireFn fn, void* ctx) { m_fireFn = fn; m_fireCtx = ctx; }
```

### 3d. New private fields

Add to the `private:` block, alongside `m_state`, etc.:

```cpp
        // ---- Item 10: shooting ----
        float m_fireCooldown = 0.0f;
        FireFn m_fireFn = nullptr;
        void*  m_fireCtx = nullptr;

        // Designer BASE stats (multiplied by the difficulty profile).
        float m_baseChaseSpeed     = 0.0f;
        float m_baseSightRange     = 0.0f;
        float m_baseHearingRadius  = 0.0f;
        float m_baseLoseAggroDelay = 0.0f;
        int   m_baseStartHp        = 0;
```

### 3e. Forward-declaration for `DifficultyProfile`

At the top of `Enemy.h`, next to the `class Player;` forward
declaration inside `namespace MegaX`, add:

```cpp
    struct DifficultyProfile;   // defined in EnemyManager.h
```

**Do NOT `#include "Game/EnemyManager.h"` from `Enemy.h`** —
that would create a circular include (`EnemyManager.h` already
includes `Enemy.h`). Forward declaration + a full include in
`Enemy.cpp` is the correct pattern.

---

## 4. `Enemy.cpp` — implement `applyDifficulty` + snapshot

Add these two functions inside `namespace MegaX { ... }`. A
good spot is right after `onHeardGunshot(...)` at the bottom of
the file.

```cpp
    void Enemy::snapshotBaseStats() {
        m_baseChaseSpeed     = chaseSpeed;
        m_baseSightRange     = sightRange;
        m_baseHearingRadius  = hearingRadius;
        m_baseLoseAggroDelay = loseAggroDelay;
        m_baseStartHp        = startHp;
    }

    void Enemy::applyDifficulty(const DifficultyProfile& p) {
        // Fresh multiply from BASE snapshot -> idempotent switching.
        chaseSpeed     = m_baseChaseSpeed     * p.chaseSpeedMul;
        sightRange     = m_baseSightRange     * p.sightRangeMul;
        hearingRadius  = m_baseHearingRadius  * p.hearingRadiusMul;
        loseAggroDelay = m_baseLoseAggroDelay * p.loseAggroDelayMul;

        // HP scales relative to baseStartHp. Casual truncates fractions
        // downward (int math) -- that's intentional; 3 hp * 0.66 = 1 or 2
        // depending on integer floor(), which is fine for how squishy
        // Casual enemies should feel.
        const int newStart = std::max(1, static_cast<int>(m_baseStartHp * p.startHpMul));
        startHp = newStart;
        m_maxHp = newStart;
        // We deliberately DO NOT snap m_hp up to the new max. Live enemies
        // keep their current HP when difficulty changes mid-fight; only the
        // ceiling (m_maxHp) moves. If you'd rather refill, uncomment:
        //   if (m_hp > m_maxHp || m_hp < m_maxHp) m_hp = m_maxHp;

        // Raw shooting values (no base snapshot needed -- they're the
        // profile's own values).
        bulletDamage    = p.bulletDamage;
        fireCooldownSec = p.fireCooldownSec;
        bulletSpeed     = p.bulletSpeed;
        leadFactor      = p.leadFactor;
    }
```

Include `EnemyManager.h` at the top of `Enemy.cpp` so
`DifficultyProfile` is visible to the definitions above:

```cpp
#include "Game/EnemyManager.h"
```

(Yes, `Enemy.cpp` including `EnemyManager.h` is fine — the
circle is broken because we forward-declared `DifficultyProfile`
in `Enemy.h`.)

### `muzzleWorldPos`

```cpp
    void Enemy::muzzleWorldPos(float& mx, float& my) const {
        mx = m_x + static_cast<float>(m_facing) * muzzleForwardX;
        my = m_feetY + muzzleAboveFeet;
    }
```

Tune `muzzleForwardX` / `muzzleAboveFeet` at spawn if the muzzle
sprite lands off the visible gun tip on your specific atlas —
current defaults align with the Robot Soldier idle sheet.

---

## 5. Sanity check

At this point the project **does not yet compile** — `Enemy.cpp`
now refers to `bulletDamage` etc. but the tick logic hasn't been
updated. Doc 02 adds the tick-side shooting + the
`EnemyBulletManager` and unblocks the build. Do not attempt a
build between docs 01 and 02.

Also expected during doc 01: `snapshotBaseStats()` is called
from `EnemyManager::spawn()` right after `e.spawn(...)`. This
means the field it snapshots — `startHp`, `chaseSpeed`, etc. —
have their **class-default values** at that point unless
`GameLayer` already overrode them. That's the point: the
snapshot captures the *designer* base. If your `GameLayer`
sets `e->startHp = 5;` **after** `spawn()` returns, call
`e->snapshotBaseStats();` and `e->applyDifficulty(mgr.profile());`
right after that assignment (doc 03 shows the exact placement).

---

Next: `02_enemy_shooting.md` — `EnemyBullet.h` / `.cpp`, the
`tickShooting` block that plugs into `tickChase`, and how the
manager wires the fire callback so `Enemy` never sees a
`std::vector`.
