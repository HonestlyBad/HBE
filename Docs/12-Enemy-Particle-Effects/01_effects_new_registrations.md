# 01 — New `Effects` registrations + spawn API

This doc adds the raw material every other Item 12 doc
depends on: five new `EmitterConfig` composite factories
inside `Effects.cpp`, one small extract (`spawnWalkDustBurst`)
so the enemy can trigger walk dust from an external timer,
and the six new public spawn methods on `Effects`.

All additions land in the two files already carrying Item 07
+ Item 11 code:

* `G:\Dev\HBE\MegaX\include\Game\Effects.h`
* `G:\Dev\HBE\MegaX\src\Game\Effects.cpp`

**Do not create new files.** The Effects header hygiene rule
still applies — keep `HBE/Renderer/ParticleSystem.h` out of
`Effects.h`.

---

## 1. `include/Game/Effects.h` — six new spawn declarations

Open `G:\Dev\HBE\MegaX\include\Game\Effects.h`.

Find the existing block of public spawn declarations:

```cpp
        void spawnMuzzleFlash(float x, float y, int dir);
        void spawnCasing(float x, float y, int dir);
        void spawnLandingDust(float feetX, float feetY, int tileId);
        void spawnBulletImpact(float x, float y, int tileId);
```

Add the six new declarations **immediately below**
`spawnBulletImpact` (before `tickWalkDust`):

```cpp
        void spawnBulletImpact(float x, float y, int tileId);

        // ---------- Item 12: enemy-side + hit reactions ----------

        // Enemy muzzle flash (red / orange, additive). Mirror across
        // facing via `dir` (+1 = right, -1 = left).
        void spawnEnemyMuzzleFlash(float x, float y, int dir);

        // Enemy bullet impact against a tile. Chunk burst tinted by the
        // tile's top color + short red spark burst. Same signature as
        // spawnBulletImpact so drainers can call the correct variant
        // per bullet source.
        void spawnEnemyBulletImpact(float x, float y, int tileId);

        // Enemy death: one big yellow-white core burst + orange-red chunks
        // + short-lived black smoke via color fade. Fires exactly once
        // per enemy death (managed by EnemyManager, not by Effects).
        void spawnEnemyExplosion(float x, float y);

        // Player bullet hits robot enemy. Metallic sparks, cool white ->
        // icy blue, additive. Directional cone back toward the shooter
        // (dir points AWAY from the shooter -- i.e. same convention as
        // spawnMuzzleFlash).
        void spawnHitSpark(float x, float y, int dir);

        // Enemy bullet hits player. Small deep-red splatter, NON-additive
        // (so it reads as blood, not fire). `dir` is the bullet's travel
        // direction (+1 right, -1 left).
        void spawnBloodSplatter(float x, float y, int dir);

        // Item 12 extract: single walk-dust puff at (feetX, feetY) tinted
        // to the tile below. The player still uses tickWalkDust (which
        // owns its own cadence via m_walkAccum). Enemies get their
        // cadence from Enemy::consumeWalkDustPuff and just call this
        // one-shot to actually emit the puff.
        void spawnWalkDustBurst(float feetX, float feetY, int tileId);
```

`tickWalkDust` and everything after it stays exactly as-is.

That's every header change in Item 12 for `Effects`. No new
includes required — the new methods take primitives + an
integer tile id, same as the existing surface.

---

## 2. `src/Game/Effects.cpp` — five new `make...` factories

Open `G:\Dev\HBE\MegaX\src\Game\Effects.cpp`.

Find the last existing `make...` static above `bool Effects::init(...)`.
As of Item 11 that's `makeBulletImpact()`. Add the five new
factories **immediately below** `makeBulletImpact()` and
**above** `bool Effects::init(...)`:

### 2a. `makeEnemyMuzzleFlash`

```cpp
    static EffectDef makeEnemyMuzzleFlash() {
        // Two-emitter composite: a bright red-orange core + a fast
        // directional spark burst (mirrored by spawnEnemyMuzzleFlash
        // when dir < 0). Additive blend on both, same as the player
        // muzzle flash.
        EmitterConfig core;
        core.name = "enemy_muzzle_flash_core";
        core.emissionRate = 0.0f;
        core.duration = 0.04f;
        core.worldSpace = true;
        core.maxParticles = 6;
        core.additiveBlend = true;
        core.bursts.push_back({ 0.0f, 3, 1 });

        core.lifetimeMin = 0.04f; core.lifetimeMax = 0.08f;
        core.shape = EmitterConfig::Shape::Point;
        core.speedMin = 0.0f;  core.speedMax = 0.0f;
        core.startSizeMin = 8.0f;  core.startSizeMax = 12.0f;
        core.endSizeMin   = 2.0f;  core.endSizeMax   = 3.0f;

        // Red-orange -> deep red fade.
        core.startR = 1.0f; core.startG = 0.55f; core.startB = 0.20f; core.startA = 1.0f;
        core.endR   = 0.9f; core.endG   = 0.10f; core.endB   = 0.0f;  core.endA   = 0.0f;

        core.sortLayer = 102;

        EmitterConfig sparks;
        sparks.name = "enemy_muzzle_flash_sparks";
        sparks.emissionRate = 0.0f;
        sparks.duration = 0.05f;
        sparks.worldSpace = true;
        sparks.maxParticles = 10;
        sparks.additiveBlend = true;
        sparks.bursts.push_back({ 0.0f, 5, 1 });

        sparks.lifetimeMin = 0.06f; sparks.lifetimeMax = 0.12f;
        sparks.shape = EmitterConfig::Shape::Point;
        sparks.speedMin = 140.0f; sparks.speedMax = 260.0f;

        // Facing-right cone; spawnEnemyMuzzleFlash mirrors for facing-left.
        sparks.dirMin = -16.0f; sparks.dirMax = 16.0f;

        sparks.gravityY = 0.0f;
        sparks.drag = 4.0f;

        sparks.startSizeMin = 2.5f;  sparks.startSizeMax = 4.0f;
        sparks.endSizeMin   = 0.0f;  sparks.endSizeMax   = 0.0f;

        sparks.startR = 1.0f; sparks.startG = 0.85f; sparks.startB = 0.35f; sparks.startA = 1.0f;
        sparks.endR   = 1.0f; sparks.endG   = 0.20f; sparks.endB   = 0.0f;  sparks.endA   = 0.0f;

        sparks.sortLayer = 102;

        return { core, sparks };
    }
```

### 2b. `makeEnemyBulletImpact`

```cpp
    static EffectDef makeEnemyBulletImpact() {
        // Same shape as makeBulletImpact, but the spark burst is red
        // (enemy-flavor) instead of yellow.
        EmitterConfig chunks;
        chunks.name = "enemy_bullet_impact_chunks";
        chunks.emissionRate = 0.0f;
        chunks.duration = 0.05f;
        chunks.worldSpace = true;
        chunks.maxParticles = 16;
        chunks.bursts.push_back({ 0.0f, 8, 1 });

        chunks.lifetimeMin = 0.20f; chunks.lifetimeMax = 0.36f;
        chunks.shape = EmitterConfig::Shape::Point;
        chunks.speedMin = 110.0f; chunks.speedMax = 240.0f;
        chunks.dirMin = 0.0f;   chunks.dirMax = 360.0f;

        chunks.gravityY = -420.0f;
        chunks.drag = 1.4f;

        chunks.startSizeMin = 3.0f; chunks.startSizeMax = 5.0f;
        chunks.endSizeMin   = 0.0f; chunks.endSizeMax   = 0.0f;

        // Chunks are tile-tinted by spawnEnemyBulletImpact. These defaults
        // are the fallback if the tile top-color lookup fails.
        chunks.startR = 0.75f; chunks.startG = 0.70f; chunks.startB = 0.60f; chunks.startA = 1.0f;
        chunks.endR   = 0.55f; chunks.endG   = 0.50f; chunks.endB   = 0.40f; chunks.endA   = 0.0f;

        chunks.sortLayer = 102;

        EmitterConfig sparks;
        sparks.name = "enemy_bullet_impact_sparks";
        sparks.emissionRate = 0.0f;
        sparks.duration = 0.05f;
        sparks.worldSpace = true;
        sparks.maxParticles = 16;
        sparks.additiveBlend = true;
        sparks.bursts.push_back({ 0.0f, 10, 1 });

        sparks.lifetimeMin = 0.10f; sparks.lifetimeMax = 0.20f;
        sparks.shape = EmitterConfig::Shape::Point;
        sparks.speedMin = 160.0f; sparks.speedMax = 320.0f;
        sparks.dirMin = 0.0f;   sparks.dirMax = 360.0f;

        sparks.gravityY = -220.0f;
        sparks.drag = 1.2f;

        sparks.startSizeMin = 2.5f; sparks.startSizeMax = 4.0f;
        sparks.endSizeMin   = 0.0f; sparks.endSizeMax   = 0.0f;

        // Red-orange sparks so the impact reads as "enemy bullet, not
        // yours". Yellow sparks are already used by makeBulletImpact.
        sparks.startR = 1.0f; sparks.startG = 0.55f; sparks.startB = 0.20f; sparks.startA = 1.0f;
        sparks.endR   = 1.0f; sparks.endG   = 0.10f; sparks.endB   = 0.0f;  sparks.endA   = 0.0f;

        sparks.sortLayer = 102;

        return { chunks, sparks };
    }
```

### 2c. `makeEnemyExplosion`

```cpp
    static EffectDef makeEnemyExplosion() {
        // Three emitters, all at (x, y) supplied by spawnEnemyExplosion:
        //   1. Bright yellow-white core (short, hot).
        //   2. Orange-red chunks flying outward.
        //   3. Black-fading smoke (rendered as gray puff that fades to
        //      transparent, giving the impression of a smoke poof).
        EmitterConfig core;
        core.name = "enemy_explosion_core";
        core.emissionRate = 0.0f;
        core.duration = 0.06f;
        core.worldSpace = true;
        core.maxParticles = 10;
        core.additiveBlend = true;
        core.bursts.push_back({ 0.0f, 6, 1 });

        core.lifetimeMin = 0.10f; core.lifetimeMax = 0.18f;
        core.shape = EmitterConfig::Shape::Point;
        core.speedMin = 0.0f;  core.speedMax = 0.0f;
        core.startSizeMin = 30.0f; core.startSizeMax = 42.0f;
        core.endSizeMin   = 8.0f;  core.endSizeMax   = 12.0f;

        core.startR = 1.0f; core.startG = 1.0f;  core.startB = 0.85f; core.startA = 1.0f;
        core.endR   = 1.0f; core.endG   = 0.45f; core.endB   = 0.0f;  core.endA   = 0.0f;

        core.sortLayer = 103;

        EmitterConfig chunks;
        chunks.name = "enemy_explosion_chunks";
        chunks.emissionRate = 0.0f;
        chunks.duration = 0.05f;
        chunks.worldSpace = true;
        chunks.maxParticles = 32;
        chunks.bursts.push_back({ 0.0f, 20, 1 });

        chunks.lifetimeMin = 0.35f; chunks.lifetimeMax = 0.65f;
        chunks.shape = EmitterConfig::Shape::Point;
        chunks.speedMin = 180.0f; chunks.speedMax = 360.0f;
        chunks.dirMin = 0.0f;   chunks.dirMax = 360.0f;

        chunks.gravityY = -520.0f;
        chunks.drag = 1.6f;

        chunks.startSizeMin = 4.0f; chunks.startSizeMax = 7.0f;
        chunks.endSizeMin   = 0.0f; chunks.endSizeMax   = 0.0f;

        chunks.startR = 1.0f; chunks.startG = 0.55f; chunks.startB = 0.15f; chunks.startA = 1.0f;
        chunks.endR   = 0.4f; chunks.endG   = 0.05f; chunks.endB   = 0.0f;  chunks.endA   = 0.0f;

        chunks.sortLayer = 102;

        EmitterConfig smoke;
        smoke.name = "enemy_explosion_smoke";
        smoke.emissionRate = 0.0f;
        smoke.duration = 0.10f;
        smoke.worldSpace = true;
        smoke.maxParticles = 20;
        smoke.bursts.push_back({ 0.0f, 12, 1 });

        smoke.lifetimeMin = 0.50f; smoke.lifetimeMax = 0.90f;
        smoke.shape = EmitterConfig::Shape::Point;
        smoke.speedMin = 20.0f;  smoke.speedMax = 60.0f;
        smoke.dirMin = 40.0f;   smoke.dirMax = 140.0f;   // rises

        smoke.gravityY = 40.0f;   // slightly buoyant
        smoke.drag = 1.0f;

        smoke.startSizeMin = 10.0f; smoke.startSizeMax = 16.0f;
        smoke.endSizeMin   = 4.0f;  smoke.endSizeMax   = 6.0f;

        smoke.startR = 0.20f; smoke.startG = 0.20f; smoke.startB = 0.20f; smoke.startA = 0.85f;
        smoke.endR   = 0.05f; smoke.endG   = 0.05f; smoke.endB   = 0.05f; smoke.endA   = 0.0f;

        smoke.sortLayer = 101;   // behind chunks + core, in front of world

        return { core, chunks, smoke };
    }
```

### 2d. `makeHitSpark`

```cpp
    static EffectDef makeHitSpark() {
        // Player bullet -> robot enemy: metallic electric spark.
        // Additive white -> icy-blue fade, small radial cone.
        EmitterConfig sparks;
        sparks.name = "hit_spark";
        sparks.emissionRate = 0.0f;
        sparks.duration = 0.04f;
        sparks.worldSpace = true;
        sparks.maxParticles = 14;
        sparks.additiveBlend = true;
        sparks.bursts.push_back({ 0.0f, 8, 1 });

        sparks.lifetimeMin = 0.08f; sparks.lifetimeMax = 0.16f;
        sparks.shape = EmitterConfig::Shape::Point;
        sparks.speedMin = 180.0f; sparks.speedMax = 320.0f;

        // Directional cone; spawnHitSpark mirrors for dir < 0.
        sparks.dirMin = 150.0f; sparks.dirMax = 210.0f;   // back toward the shooter

        sparks.gravityY = -60.0f;
        sparks.drag = 3.0f;

        sparks.startSizeMin = 2.5f; sparks.startSizeMax = 4.5f;
        sparks.endSizeMin   = 0.0f; sparks.endSizeMax   = 0.0f;

        sparks.startR = 1.0f; sparks.startG = 1.0f;  sparks.startB = 0.95f; sparks.startA = 1.0f;
        sparks.endR   = 0.4f; sparks.endG   = 0.75f; sparks.endB   = 1.0f;  sparks.endA   = 0.0f;

        sparks.sortLayer = 103;

        return { sparks };
    }
```

### 2e. `makeBloodSplatter`

```cpp
    static EffectDef makeBloodSplatter() {
        // Enemy bullet -> player: small non-additive deep-red splatter.
        // Faster falling than the other bursts (blood has weight).
        EmitterConfig drops;
        drops.name = "blood_splatter";
        drops.emissionRate = 0.0f;
        drops.duration = 0.05f;
        drops.worldSpace = true;
        drops.maxParticles = 14;
        drops.bursts.push_back({ 0.0f, 8, 1 });

        drops.lifetimeMin = 0.20f; drops.lifetimeMax = 0.40f;
        drops.shape = EmitterConfig::Shape::Point;
        drops.speedMin = 90.0f;  drops.speedMax = 220.0f;

        // spawnBloodSplatter mirrors dirMin/Max across the vertical axis
        // when dir < 0. Defaults face right (bullet travelling right ->
        // drops eject rightward + downward).
        drops.dirMin = -35.0f;  drops.dirMax = 35.0f;

        drops.gravityY = -680.0f;   // heavier than dust
        drops.drag = 1.2f;

        drops.startSizeMin = 2.5f; drops.startSizeMax = 4.5f;
        drops.endSizeMin   = 0.0f; drops.endSizeMax   = 0.0f;

        drops.startR = 0.75f; drops.startG = 0.05f; drops.startB = 0.05f; drops.startA = 1.0f;
        drops.endR   = 0.35f; drops.endG   = 0.0f;  drops.endB   = 0.0f;  drops.endA   = 0.0f;

        drops.sortLayer = 102;

        return { drops };
    }
```

---

## 3. `src/Game/Effects.cpp` — register the five new effects

Find `Effects::init(...)`. Locate the existing block of
`m_ps->registerEffect(...)` calls (right after
`m_ps->initialize(...)` succeeded):

```cpp
        m_ps->registerEffect("walk_dust", makeWalkDust());
        m_ps->registerEffect("land_dust", makeLandDust());
        m_ps->registerEffect("muzzle_flash", makeMuzzleFlash());
        // NOTE: casings are NOT registered ...
        m_ps->registerEffect("bullet_impact", makeBulletImpact());
```

Add the five new registrations **immediately below** the
`bullet_impact` line:

```cpp
        m_ps->registerEffect("bullet_impact", makeBulletImpact());

        // ---------- Item 12 ----------
        m_ps->registerEffect("enemy_muzzle_flash", makeEnemyMuzzleFlash());
        m_ps->registerEffect("enemy_bullet_impact", makeEnemyBulletImpact());
        m_ps->registerEffect("enemy_explosion", makeEnemyExplosion());
        m_ps->registerEffect("hit_spark", makeHitSpark());
        m_ps->registerEffect("blood_splatter", makeBloodSplatter());
```

Registration name = the string the `spawn` implementations
pass to `m_ps->spawn("<name>", x, y)`. Keep them in sync
with the emitter `c.name` fields where possible (they're
not required to match — the `EmitterConfig::name` is only
used for debugging).

---

## 4. `src/Game/Effects.cpp` — extract `spawnWalkDustBurst`

Find the existing `Effects::tickWalkDust(...)`. It ends with:

```cpp
        EffectDef def = makeWalkDust();
        EmitterConfig& c = def[0];
        c.startR = r;         c.startG = g;         c.startB = b;
        c.endR = r * 0.55f; c.endG = g * 0.55f; c.endB = b * 0.55f;

        m_ps->registerEffect("walk_dust", def);
        m_ps->spawn("walk_dust", feetX, feetY);
    }
```

Extract those last five lines into a new public method
`spawnWalkDustBurst(feetX, feetY, tileId)` and have
`tickWalkDust` call it. Full replacement:

```cpp
    void Effects::spawnWalkDustBurst(float feetX, float feetY, int tileId) {
        if (!m_ps) return;
        float r, g, b;
        colorForTile(tileId, r, g, b);

        EffectDef def = makeWalkDust();
        EmitterConfig& c = def[0];
        c.startR = r;         c.startG = g;         c.startB = b;
        c.endR   = r * 0.55f; c.endG   = g * 0.55f; c.endB   = b * 0.55f;

        m_ps->registerEffect("walk_dust", def);
        m_ps->spawn("walk_dust", feetX, feetY);
    }

    void Effects::tickWalkDust(float dt,
        float feetX, float feetY,
        int tileId,
        bool moving, bool grounded)
    {
        if (!m_ps || !moving || !grounded) {
            m_walkAccum = 0.0f;
            return;
        }

        m_walkAccum += dt;
        if (m_walkAccum < walkDustPeriod) return;
        m_walkAccum -= walkDustPeriod;

        spawnWalkDustBurst(feetX, feetY, tileId);
    }
```

The behavior is identical to Item 07 for the player; the
only change is that the "emit a puff" body is now callable
from an external accumulator (the per-enemy one added in
doc 02).

---

## 5. `src/Game/Effects.cpp` — implement the five new `spawn...` functions

Add these five functions **immediately below**
`spawnBulletImpact(...)` (the existing Item 07 function).
Order doesn't matter, but grouping them together keeps
grep-friendly.

### 5a. `spawnEnemyMuzzleFlash`

```cpp
    void Effects::spawnEnemyMuzzleFlash(float x, float y, int dir) {
        if (!m_ps) return;
        EffectDef def = makeEnemyMuzzleFlash();
        if (def.size() >= 2) {
            EmitterConfig& sparks = def[1];
            if (dir < 0) {
                const float lo = 180.0f - sparks.dirMax;
                const float hi = 180.0f - sparks.dirMin;
                sparks.dirMin = lo;
                sparks.dirMax = hi;
            }
        }
        m_ps->registerEffect("enemy_muzzle_flash", def);
        m_ps->spawn("enemy_muzzle_flash", x, y);
    }
```

### 5b. `spawnEnemyBulletImpact`

```cpp
    void Effects::spawnEnemyBulletImpact(float x, float y, int tileId) {
        if (!m_ps) return;
        float r, g, b;
        colorForTile(tileId, r, g, b);

        EffectDef def = makeEnemyBulletImpact();
        EmitterConfig& chunks = def[0];
        chunks.startR = r;         chunks.startG = g;         chunks.startB = b;
        chunks.endR   = r * 0.55f; chunks.endG   = g * 0.55f; chunks.endB   = b * 0.55f;

        m_ps->registerEffect("enemy_bullet_impact", def);
        m_ps->spawn("enemy_bullet_impact", x, y);
    }
```

### 5c. `spawnEnemyExplosion`

```cpp
    void Effects::spawnEnemyExplosion(float x, float y) {
        if (!m_ps) return;
        // No color parameterization -- the explosion recipe is fully
        // baked. If per-enemy tinting becomes desirable, take a
        // (r, g, b) tuple and shift each emitter's start/end colors.
        m_ps->spawn("enemy_explosion", x, y);
    }
```

Note: no `registerEffect` call here — we don't mutate the
def per-spawn (unlike muzzle flash's directional dirMin/Max
mirroring). The registration from `init` is enough. Same
idiom applies to the next two.

### 5d. `spawnHitSpark`

```cpp
    void Effects::spawnHitSpark(float x, float y, int dir) {
        if (!m_ps) return;
        EffectDef def = makeHitSpark();
        EmitterConfig& sparks = def[0];
        if (dir < 0) {
            // Same convention as the muzzle flash: mirror across vertical.
            const float lo = 180.0f - sparks.dirMax;
            const float hi = 180.0f - sparks.dirMin;
            sparks.dirMin = lo;
            sparks.dirMax = hi;
        }
        m_ps->registerEffect("hit_spark", def);
        m_ps->spawn("hit_spark", x, y);
    }
```

### 5e. `spawnBloodSplatter`

```cpp
    void Effects::spawnBloodSplatter(float x, float y, int dir) {
        if (!m_ps) return;
        EffectDef def = makeBloodSplatter();
        EmitterConfig& drops = def[0];
        if (dir < 0) {
            const float lo = 180.0f - drops.dirMax;
            const float hi = 180.0f - drops.dirMin;
            drops.dirMin = lo;
            drops.dirMax = hi;
        }
        m_ps->registerEffect("blood_splatter", def);
        m_ps->spawn("blood_splatter", x, y);
    }
```

---

## 6. Sanity check — compile only at end of doc 01

The functions declared here are not yet called by any client.
A build here should compile clean (only warning: `unused
private member function makeEnemyExplosion` if MSVC decides
to complain — it won't, because the function IS called from
`Effects::init`). If anything else warns, re-read the header
tweaks in §1.

Do NOT run the game yet — nothing visible changes until doc 02
wires the manager, and even then, the wiring is incomplete
until doc 03. Only the build-clean check matters at this
step.

Common build-time errors and fixes:

| Symptom | Cause | Fix |
|---|---|---|
| `error C2039 'spawnEnemyMuzzleFlash': is not a member of 'MegaX::Effects'` | Header not saved. | Save `Effects.h` and rebuild. |
| `error C2065 'makeEnemyExplosion': undeclared identifier` | The `make...` factory was pasted below `Effects::init` instead of above it. | Move the factory above `init`, or forward-declare it at the top of the anonymous scope. Above-init is easier. |
| `error C2027 use of undefined type 'HBE::Renderer::ParticleSystem'` | You added `#include "HBE/Renderer/ParticleSystem.h"` to `Effects.h`. | Remove it. The include lives in `Effects.cpp`. |
| Spark cone points in the wrong direction after `dir` flip | You copy-pasted the mirror formula but forgot to swap `dirMin` / `dirMax` after computing `lo`/`hi`. | Check the order: `lo = 180 - dirMax; hi = 180 - dirMin; dirMin = lo; dirMax = hi;`. |
| The muzzle flash is bright yellow like the player's | You called `makeMuzzleFlash()` instead of `makeEnemyMuzzleFlash()` inside `spawnEnemyMuzzleFlash`. | Fix the function name. |
| Blood splatter glows (looks like fire) | `additiveBlend = true` somewhere in `makeBloodSplatter`. | Blood must be `additiveBlend = false` (which is the default — do not set true anywhere in that def). |

---

Next: `02_enemy_accessors_and_movement_fx.md` — Enemy self-
observation accessors (`landedThisFrame`, `groundTileId`,
`justDied`, `consumeWalkDustPuff`) and the
`EnemyManager::update` dispatch that ties them to the new
Effects spawn API.
