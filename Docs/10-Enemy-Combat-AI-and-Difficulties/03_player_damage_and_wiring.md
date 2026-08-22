# 03 — Player HP + damage, GameLayer wiring, difficulty HUD

At the end of docs 01 and 02 we have:

* A `Difficulty` enum, three baked profiles, and
  `EnemyManager::setDifficulty(...)`.
* Enemies that shoot from standoff distance, with lead
  prediction available for Challenging.
* An `EnemyBulletManager` that owns the shots but nobody yet
  drives its `update(...)` or does the player-hit test.
* Nothing on the Player side that responds to a hit.

Doc 03 closes the loop:

1. Player HP + `takeDamage(...)` + i-frames + hurt flash +
   knockback.
2. GameLayer's per-frame ordering: sense/tick enemies → advance
   enemy bullets → test enemy bullets against the player
   hurtbox → draw everything → draw HUD.
3. Hotkeys `F1` / `F2` / `F3` for difficulty; `H` for HP refill.
4. HUD via `DebugDraw2D` anchored to the camera: HP pips
   top-left, difficulty pill top-right, both stationary on
   screen while the camera moves.

Same rules as before: no engine files touched.

---

## 1. `Player.h` — add HP, i-frames, hurt flash

### 1a. Public API additions

Insert right below the existing `Mode mode() const { ... }`:

```cpp
        // -----------------------------------------------------------------
        // Item 10: HP + damage
        // -----------------------------------------------------------------
        int   hp()            const { return m_hp; }
        int   maxHp()         const { return startHp; }
        bool  isInvulnerable() const { return m_invulnTimer > 0.0f; }

        // Deal `amount` damage. Returns true if damage was actually
        // applied. Ignored while in Ghost mode or during i-frames.
        // If `knockbackDir` is non-zero (typically the incoming bullet's
        // sign(dx)), a horizontal impulse is added.
        bool  takeDamage(int amount, int knockbackDir);

        // Convenience -- called by F1/F2/F3 / H hotkey handling.
        void  refillHp() { m_hp = startHp; m_hurtFlashTimer = 0.0f; }
```

### 1b. New tunables

Right below `float maxFall = 900.0f;` (or wherever the tunables
group ends):

```cpp
        // -- Item 10 damage tunables --
        int   startHp         = 5;
        float invulnDuration  = 0.55f;   // seconds of i-frames after a hit
        float hurtFlashTime   = 0.20f;   // sprite tint duration (<= invulnDuration)
        float knockbackImpulse = 260.0f; // px/s horizontal shove on hit
```

### 1c. New private fields

Next to the other combat/render state:

```cpp
        // Item 10: HP + i-frames
        int   m_hp = 5;
        float m_invulnTimer   = 0.0f;
        float m_hurtFlashTimer = 0.0f;
```

### 1d. Reset HP on setPosition (optional but expected)

`setPosition` is called at scene start; use it as a natural
"give me full HP" point. In `Player.cpp`, at the top of
`setPosition(...)`:

```cpp
        m_hp = startHp;
        m_invulnTimer = 0.0f;
        m_hurtFlashTimer = 0.0f;
```

If you'd rather keep HP across position resets, skip this and
call `player.refillHp()` explicitly from GameLayer.

---

## 2. `Player.cpp` — timers, hurt tint, takeDamage

### 2a. Tick timers each frame

Both `updatePlay` and `updateGhost` tick physics; the i-frame
timer should also tick regardless of mode. The safest spot is
at the very top of `Player::update(...)`. Add:

```cpp
        if (m_invulnTimer    > 0.0f) m_invulnTimer    = std::max(0.0f, m_invulnTimer    - dt);
        if (m_hurtFlashTimer > 0.0f) m_hurtFlashTimer = std::max(0.0f, m_hurtFlashTimer - dt);
```

If your `Player::update` fans out to `updatePlay(dt)` /
`updateGhost(dt)` before doing anything else, put the timer
decay in *the switch's parent* — one place, always ticks.

### 2b. Apply hurt tint in the render fn

Find the block in `Player::update` (or wherever `m_item.tint` is
assigned) that looks like:

```cpp
        m_item.tint = (m_mode == Mode::Ghost) ? Color4{ 0.6f, 0.8f, 1.0f, 0.5f }
                                              : Color4{ 1.0f, 1.0f, 1.0f, 1.0f };
```

Replace with:

```cpp
        if (m_mode == Mode::Ghost) {
            m_item.tint = Color4{ 0.6f, 0.8f, 1.0f, 0.5f };
        }
        else if (m_hurtFlashTimer > 0.0f) {
            // Angry red, pulses down to full alpha as the flash ends.
            const float k = m_hurtFlashTimer / hurtFlashTime;
            m_item.tint = Color4{ 1.0f, 0.35f + (1.0f - k) * 0.65f, 0.35f + (1.0f - k) * 0.65f, 1.0f };
        }
        else if (m_invulnTimer > 0.0f) {
            // Post-flash i-frame: soft flicker (25% duty cycle).
            const int frame = static_cast<int>(m_invulnTimer * 40.0f);
            m_item.tint = (frame & 1) ? Color4{ 1.0f, 1.0f, 1.0f, 0.35f }
                                      : Color4{ 1.0f, 1.0f, 1.0f, 1.0f };
        }
        else {
            m_item.tint = Color4{ 1.0f, 1.0f, 1.0f, 1.0f };
        }
```

### 2c. `takeDamage` implementation

At the end of `Player.cpp` inside the `namespace MegaX { ... }`
block:

```cpp
    bool Player::takeDamage(int amount, int knockbackDir) {
        if (amount <= 0) return false;
        if (m_mode == Mode::Ghost) return false;
        if (m_invulnTimer > 0.0f) return false;

        m_hp = std::max(0, m_hp - amount);
        m_invulnTimer    = invulnDuration;
        m_hurtFlashTimer = hurtFlashTime;

        // Knockback: nudge the horizontal velocity only. Gravity keeps the
        // vertical arc feeling normal.
        if (knockbackDir != 0) {
            const float dir = (knockbackDir > 0) ? +1.0f : -1.0f;
            m_vx += dir * knockbackImpulse;
        }
        return true;
    }
```

Note: no "death" branch. Item 10 keeps `m_hp` bottoming out at
0; you'll still see the player, still take control, but the HUD
will read 0. Death animation, respawn, and game-over screen are
out of scope for this item. If you want a hard block:

```cpp
        if (m_hp == 0) m_mode = Mode::Ghost;   // "downed", waiting for refill
```

is a workable stub; recommend leaving it out until you build the
proper death sequence.

---

## 3. `GameLayer.h` — cache the difficulty

Add a public field:

```cpp
        Difficulty m_difficulty = Difficulty::Difficult;
```

`GameLayer.h` already includes `Game/EnemyManager.h`, so
`Difficulty` is visible without extra work.

---

## 4. `GameLayer.cpp` — hotkeys, enemy-bullet drive, HUD

Four edit blocks. All in `src/Game/GameLayer.cpp`.

### 4a. `onAttach` — set difficulty and re-snapshot enemy stats

Item 09 already sets `startHp = 3` **after** `spawn(...)` — that
means the base-stat snapshot we take inside `EnemyManager::spawn`
captured `startHp = default (3)`, then the code overrides to 3
again. That's fine (still 3) but if you tune `startHp = 5` in a
future edit, snapshot again to avoid Casual's `startHpMul` biting
into the old default.

Find your existing spawn block (Item 09) and update it to
re-snapshot + re-apply after any manual field tweaks:

```cpp
        m_enemies.setPlayerRef(&m_player);
        m_enemies.setCollision(&m_world.map(), m_ground);
        m_enemies.setDifficulty(m_difficulty);   // establishes initial profile

        {
            constexpr float kTilePx = 32.0f;
            const float ex = startX + 5.0f * kTilePx;
            const float eGroundY = 130.0f;
            if (Enemy* e = m_enemies.spawn(ex, eGroundY, -1)) {
                // Designer-level base overrides (before difficulty scaling).
                e->startHp = 3;

                // Patrol +/- 3 tiles from spawn X.
                e->setPatrolPath(ex - 3.0f * kTilePx, ex + 3.0f * kTilePx, 1.0f);

                // Item 10: rebuild the base snapshot with the new startHp,
                // then re-apply the current profile so multipliers use it.
                e->snapshotBaseStats();
                e->applyDifficulty(m_enemies.profile());

                // Re-spawn to snap HP to the new max.
                e->spawn(ex, eGroundY, -1);
            }
        }
```

### 4b. `onUpdate` — hotkeys + enemy-bullet update + player-hit

Add near the top of `onUpdate(dt)`, wherever you handle other
one-shot key presses (e.g. `G` for ghost, `B` for debug). The
API is `Input::IsKeyPressed(...)` (Pascal-case) — the existing
handlers in `GameLayer::onUpdate` already use this form.

```cpp
        // -------- Item 10: difficulty hotkeys --------
        // Uses the same Input:: alias that the WASD/G/B/H handlers already use
        // (`namespace Input = HBE::Platform::Input;` at the top of this file).
        // NOTE: H is already bound to `toggleHelmet()` (Item 01), so we use R
        // for the HP-refill dev shortcut here.
        if (Input::IsKeyPressed(SDL_SCANCODE_F1)) {
            m_difficulty = Difficulty::Casual;
            m_enemies.setDifficulty(m_difficulty);
            m_player.refillHp();
            LogInfo("Difficulty: Casual");
        }
        if (Input::IsKeyPressed(SDL_SCANCODE_F2)) {
            m_difficulty = Difficulty::Difficult;
            m_enemies.setDifficulty(m_difficulty);
            m_player.refillHp();
            LogInfo("Difficulty: Difficult");
        }
        if (Input::IsKeyPressed(SDL_SCANCODE_F3)) {
            m_difficulty = Difficulty::Challenging;
            m_enemies.setDifficulty(m_difficulty);
            m_player.refillHp();
            LogInfo("Difficulty: Challenging");
        }
        if (Input::IsKeyPressed(SDL_SCANCODE_R)) {
            m_player.refillHp();
            LogInfo("HP refilled");
        }
```

Confirm your Input namespace matches the alias already at the
top of `GameLayer.cpp` (`namespace Input = HBE::Platform::Input;`
from Item 09). The existing handlers use
`Input::IsKeyPressed(SDL_SCANCODE_...)` — use the same form
here.

After `m_enemies.update(dt);` (which now drives enemy sensing
and shooting), advance the enemy bullets and test the player
hit. GameLayer owns the camera and player, so this is the
natural place:

```cpp
        // -------- Item 10: enemy bullets --------
        auto& ebm = m_enemies.enemyBullets();
        ebm.update(dt, &m_world.map(), m_ground, m_camera.camera());

        {
            const AABB pb = m_player.hurtbox();
            for (auto& b : ebm.bullets()) {
                if (!b.alive) continue;
                if (std::fabs(b.x - pb.cx) > pb.w * 0.5f) continue;
                if (std::fabs(b.y - pb.cy) > pb.h * 0.5f) continue;

                const int kbDir = (b.vx >= 0.0f) ? +1 : -1;
                if (m_player.takeDamage(b.damage, kbDir)) {
                    b.alive = false;
                    // In Item 12 we'll spawn blood-splatter particles here.
                }
                // In Ghost or during i-frames, takeDamage returns false and
                // the bullet keeps flying -- feels intentional.
            }
        }
```

`m_camera.camera()` is the `Camera2D&` accessor already used by
the player-bullet update.

Make sure `#include <cmath>` and `AABB` are visible in this
translation unit — GameLayer already uses them for player
bullet checks.

### 4c. `onRender` — draw enemy bullets + HUD

Item 09 already had the World-pass block with `m_enemies.render(r2d)`,
`m_bullets.render(r2d)`, etc. Add two draws after the existing
bullet render, and the HUD as a final on-top pass:

```cpp
            m_enemies.render(r2d);
            m_bullets.render(r2d);
            m_enemies.enemyBullets().render(r2d);   // Item 10

            m_effects.render(r2d);

            // Item 09 always-on bubbles.
            m_enemies.renderBubbles(m_debug, r2d);

            // Debug overlays (B toggle) -- unchanged.
            if (m_showHitBoxes) {
                m_enemies.debugDrawBoxes(m_debug, r2d);
                m_enemies.debugDrawSenses(m_debug, r2d);
                // ... player hurtbox draw stays ...
            }

            // Item 10 HUD -- draw last, always on.
            drawHud(r2d);
```

`drawHud` is a new private method on `GameLayer`. Add:

**`GameLayer.h`:**

```cpp
    private:
        void drawHud(HBE::Renderer::Renderer2D& r2d);
```

**`GameLayer.cpp`:**

```cpp
    void GameLayer::drawHud(Renderer2D& r2d) {
        const Camera2D& cam = m_camera.camera();
        const float zoom  = (cam.zoom <= 0.0f) ? 1.0f : cam.zoom;
        const float halfW = cam.viewportWidth  / (2.0f * zoom);
        const float halfH = cam.viewportHeight / (2.0f * zoom);
        const float leftX  = cam.x - halfW;
        const float rightX = cam.x + halfW;
        const float topY   = cam.y + halfH;

        // ---- HP pips (top-left) ----
        // Each pip is a 12x12 rect with a 2px gap.
        const int   maxHp = m_player.maxHp();
        const int   curHp = m_player.hp();
        const float pipSize = 12.0f;
        const float pipGap  = 4.0f;
        const float pipY    = topY - 20.0f;
        const float pipX0   = leftX + 20.0f;

        for (int i = 0; i < maxHp; ++i) {
            const float cx = pipX0 + (pipSize + pipGap) * i + pipSize * 0.5f;
            const bool filled = (i < curHp);
            if (filled) {
                m_debug.rect(r2d, cx, pipY, pipSize, pipSize, 0.95f, 0.15f, 0.15f, 1.0f, true);
                m_debug.rect(r2d, cx, pipY, pipSize, pipSize, 1.0f, 1.0f, 1.0f, 1.0f, false);
            }
            else {
                m_debug.rect(r2d, cx, pipY, pipSize, pipSize, 0.25f, 0.05f, 0.05f, 0.7f, true);
                m_debug.rect(r2d, cx, pipY, pipSize, pipSize, 0.8f, 0.8f, 0.8f, 1.0f, false);
            }
        }

        // ---- Difficulty pill (top-right) ----
        // Baked as a colored bar; the label text is expressed as small pips
        // whose count matches the difficulty tier (Casual=1, Difficult=2,
        // Challenging=3) since we don't have text infrastructure wired.
        const DifficultyProfile& prof = m_enemies.profile();
        const float pillW = 44.0f, pillH = 12.0f;
        const float pillX = rightX - 20.0f - pillW * 0.5f;
        const float pillY = topY   - 20.0f;
        m_debug.rect(r2d, pillX, pillY, pillW, pillH,
                     prof.labelR, prof.labelG, prof.labelB, 0.95f, true);
        m_debug.rect(r2d, pillX, pillY, pillW, pillH, 1, 1, 1, 1, false);

        int tier = 2;   // Difficult
        if (m_difficulty == Difficulty::Casual)      tier = 1;
        else if (m_difficulty == Difficulty::Challenging) tier = 3;

        const float dotSize = 4.0f;
        const float dotY = pillY;
        const float dotGap = 3.0f;
        const float rowW = tier * dotSize + (tier - 1) * dotGap;
        const float dotX0 = pillX - rowW * 0.5f + dotSize * 0.5f;
        for (int i = 0; i < tier; ++i) {
            const float dx = dotX0 + (dotSize + dotGap) * i;
            m_debug.rect(r2d, dx, dotY, dotSize, dotSize, 0.05f, 0.05f, 0.05f, 1.0f, true);
        }
    }
```

**HUD design notes:**

* Everything anchors to `cam.x/cam.y` so the HUD is stationary
  on screen even though we're drawing at world coordinates.
* No text infra needed — we draw filled rects for HP pips and
  small black dots on the difficulty pill to indicate tier
  (1 dot = Casual, 2 = Difficult, 3 = Challenging). The pill's
  fill color already communicates the difficulty via green /
  yellow / red.
* If/when you wire up `TextRenderer2D` in GameLayer, replace the
  dot loop with a `drawText(cam.x + ..., "Casual", ...)` call —
  the rest of the HUD stays the same.

---

## 5. Update `GameLayer` includes

At the top of `GameLayer.cpp`:

```cpp
#include "Game/EnemyManager.h"    // already there (Item 09)
```

If you routed `Difficulty` through a header not currently
included, ensure `EnemyManager.h` reaches this TU (it already
does through the `m_enemies` member).

---

## 6. Ghost-mode policy for damage (documented)

You may notice that:

* `Player::takeDamage` early-outs during Ghost mode.
* Enemy bullets *don't* despawn when hitting a ghosting player.
  They keep flying and continue their normal culling.

That's intentional. Ghost mode is the "invisible builder" — a
bullet that already left the barrel keeps going, but you can't
be hit. If you'd rather have bullets *vanish* on ghost contact
(cleaner visually), swap the loop's `if (m_player.takeDamage(...)`
branch to also `b.alive = false;` when `m_player.mode() ==
Player::Mode::Ghost`, but be aware that gives away where you're
standing.

---

## 7. Sanity check

At the end of doc 03:

* `Player` has HP, i-frames, hurt flash, and takeDamage.
* GameLayer drives enemy bullets, tests hits, applies damage.
* F1/F2/F3 swap difficulty; H refills HP.
* HUD renders HP pips + difficulty pill anchored to the camera.

Doc 04 runs the build and the manual verify list, and includes
a tuning table + troubleshooting matrix.

---

Next: `04_build_run_and_verify.md`.
