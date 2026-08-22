# 16 — MegaX entities on the fixed clock (`Player`, `Enemy`, bullets)

This doc splits every simulated MegaX entity into a
simulation half and a presentation half, and teaches each one
to interpolate. Eight files, all under
`/home/atulo/Projects/HBE/MegaX/`:

| File | Indentation |
|---|---|
| `include/Game/Player.h` | **tabs** |
| `src/Game/Player.cpp` | **tabs** |
| `include/Game/Enemy.h` | **tabs** |
| `src/Game/Enemy.cpp` | **4 spaces** |
| `include/Game/EnemyManager.h` | **4 spaces** |
| `src/Game/EnemyManager.cpp` | **4 spaces** |
| `include/Game/Bullet.h` | **tabs** |
| `src/Game/Bullet.cpp` | **tabs** |
| `include/Game/EnemyBullet.h` | **4 spaces** |
| `src/Game/EnemyBullet.cpp` | **tabs** |

> **PASTE NOTE.** That table is not a mistake — MegaX
> genuinely mixes the two, sometimes between a header and
> its own `.cpp`. Every code block below is already written
> in the style of the file it goes into. Check the table
> before you paste, and do not "fix" a file's style while
> you are in it.

> **LINE NUMBERS IN THIS DOC.** Every line number below
> refers to the file **as it is before you start this doc** —
> they are all measured against the pristine file, never
> against a partly-edited one. Once you apply an edit, the
> numbers for every later edit in the same file shift down by
> whatever you inserted. That is why each edit is *also*
> anchored by the exact text at the insertion point: when the
> number and the text disagree, **the text wins**. Search for
> the quoted line, do not scroll to the number.

**MegaX will not compile in the middle of this doc.** Every
`render` signature gains an `alpha` parameter and the call
sites are all in `GameLayer.cpp`, which doc `06` handles.
§9 lists exactly which errors are expected.

---

## 1. The shape being applied, four times

Each entity gets the same three-part treatment. Read this
once and the rest of the doc is mechanical.

1. **A previous position.** Two floats, written at the top
   of the fixed step, before anything can move the entity.
2. **A split update.** Whatever moves the entity or changes
   its game state runs on the fixed step. Whatever only
   changes how it looks — animation frames, tints — runs
   once per rendered frame on the real `dt`.
3. **A lerping `render`.** It takes `alpha` and draws at
   `prev + (cur - prev) * alpha`. Simulation code keeps
   reading `m_x` / `m_y`; the interpolated value never
   leaves the transform.

And the rule that catches everyone: **every teleport writes
both positions.** `Player::setPosition`, `Enemy::spawn`, and
both bullet `spawn` methods. Miss one and that entity draws
a one-frame smear from where it used to be.

---

## 2. `Player.h`

Open `/home/atulo/Projects/HBE/MegaX/include/Game/Player.h`.

### 2.1 The update / render declarations

Lines 80-81 currently read:

```cpp
		void update(float dt);
		void render(HBE::Renderer::Renderer2D& r2d);
```

Replace both lines with:

```cpp
		// --- item 16: simulation half. Runs on the fixed step, 0..N times
		// per rendered frame, always with the same h. Moves the player and
		// consumes the latched one-shot intents.
		void fixedUpdate(float h);

		// --- item 16: presentation half. Runs once per rendered frame with
		// the real frame dt so animation and the hurt flash stay smooth no
		// matter what the simulation rate is. Writes no position.
		void updateVisual(float dt);

		// alpha comes from Application::interpolationAlpha() and is only
		// valid inside a layer's onRender().
		void render(HBE::Renderer::Renderer2D& r2d, float alpha);
```

### 2.2 The previous position

The private position block currently reads (lines 102-105):

```cpp
		// position = sprite center (world space)
		float m_x = 0.0f, m_y = 0.0f;
		float m_vx = 0.0f, m_vy = 0.0f;
		float m_inX = 0.0f, m_inY = 0.0f;
```

Insert right after `float m_x = 0.0f, m_y = 0.0f;`
(line 103) and before `float m_vx = 0.0f, m_vy = 0.0f;`
(line 104):

```cpp
		// item 16: position at the START of the current fixed step, so
		// render() can draw somewhere between here and m_x/m_y. Simulation
		// must never read these -- they are presentation state.
		float m_prevX = 0.0f, m_prevY = 0.0f;
```

Final block:

```cpp
		// position = sprite center (world space)
		float m_x = 0.0f, m_y = 0.0f;
		// item 16: position at the START of the current fixed step, so
		// render() can draw somewhere between here and m_x/m_y. Simulation
		// must never read these -- they are presentation state.
		float m_prevX = 0.0f, m_prevY = 0.0f;
		float m_vx = 0.0f, m_vy = 0.0f;
		float m_inX = 0.0f, m_inY = 0.0f;
```

---

## 3. `Player.cpp`

Open `/home/atulo/Projects/HBE/MegaX/src/Game/Player.cpp`.

### 3.1 `setPosition` — the teleport

Lines 116-131 currently read:

```cpp
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
```

Insert right after `m_x = x; m_y = y;` (line 120) and before
`m_vx = m_vy = 0.0f;` (line 121):

```cpp
		m_prevX = x; m_prevY = y;   // item 16: teleport, do not interpolate
```

Final function:

```cpp
	void Player::setPosition(float x, float y) {
		m_hp = startHp;
		m_invulnTimer = 0.0f;
		m_hurtFlashTimer = 0.0f;
		m_x = x; m_y = y;
		m_prevX = x; m_prevY = y;   // item 16: teleport, do not interpolate
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
```

> **WHY THIS ONE LINE MATTERS.** `setPosition` is what `F5`
> and `F7` call through `reloadScene`. Without it the player
> respawns at the start point with `m_prev` still holding
> wherever he died, and for exactly one frame he is drawn
> smeared across the map. It is the single most visible
> symptom of a missed teleport, and it is one line.

`resetForRespawn` (lines 133 onward) needs **no** change:
`reloadScene` always calls `setPosition` immediately after
it, and `resetForRespawn` never touches `m_x` / `m_y`.

### 3.2 Split `update` into `fixedUpdate` + `updateVisual`

Lines 219-252 currently hold the whole of `Player::update`:

```cpp
	void Player::update(float dt) {
		if (m_invulnTimer > 0.0f) m_invulnTimer = std::max(0.0f, m_invulnTimer - dt);
		if (m_hurtFlashTimer > 0.0f) m_hurtFlashTimer = std::max(0.0f, m_hurtFlashTimer - dt);

		if (m_mode == Mode::Ghost) updateGhost(dt);
		else                       updatePlay(dt);

		m_jumpPressed = false;
		m_firePressed = false;

		m_item.transform.posX = m_x;
		...
		SpriteAnimation& a = animForState(m_animState < 0 ? 0 : m_animState);
		a.update(dt);
		a.apply(m_item);
	}
```

**Replace lines 219-252 inclusive** — the entire
`Player::update` function, from `void Player::update(float dt) {`
through its closing brace, up to but not including the blank
line before `void Player::updateGhost(float dt) {` — with:

```cpp
	// --- item 16: the simulation half -----------------------------------
	// Runs on the fixed step, so h is the same value on every machine at
	// every render rate. Everything here can change where the player IS.
	void Player::fixedUpdate(float h) {
		// Snapshot where this step starts, so render() can draw between
		// here and wherever the step ends. This must be the first statement
		// in the function -- anything that moves m_x before it makes the
		// interpolation lie by a whole step.
		m_prevX = m_x;
		m_prevY = m_y;

		if (m_invulnTimer > 0.0f) m_invulnTimer = std::max(0.0f, m_invulnTimer - h);
		if (m_hurtFlashTimer > 0.0f) m_hurtFlashTimer = std::max(0.0f, m_hurtFlashTimer - h);

		if (m_mode == Mode::Ghost) updateGhost(h);
		else                       updatePlay(h);

		// One press = one action, even when two fixed steps run in the same
		// rendered frame: the second step finds these already cleared.
		// GameLayer re-latches them from real input in onUpdate, once per
		// frame, which is the whole reason onUpdate runs first.
		m_jumpPressed = false;
		m_firePressed = false;
	}

	// --- item 16: the presentation half ---------------------------------
	// Runs once per rendered frame with the real frame dt. Animation and
	// the hurt/invulnerability flashes are visual-only and the work item
	// says explicitly not to tie them to the fixed step -- at --fixed-hz 15
	// the game should crawl and the sprite should still animate smoothly.
	//
	// Writes tint and animation frame only. Position and facing are written
	// by render(), which is the first point in the frame where the
	// interpolation alpha is valid.
	void Player::updateVisual(float dt) {
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
```

Note what did **not** move: `updateGhost`, `updatePlay`,
`syncRenderFromBox`, `setAnimState`, `animForState`,
`consumeShot` and `takeDamage` are untouched. `updateGhost`
and `updatePlay` keep their `float dt` parameter name even
though they are now handed `h` — renaming a parameter in two
400-line functions is churn with no payoff, and both are
private.

### 3.3 `render` — the interpolation

Lines 403-405 currently read:

```cpp
	void Player::render(Renderer2D& r2d) {
		r2d.draw(m_item);
	}
```

Replace all three lines with:

```cpp
	void Player::render(Renderer2D& r2d, float alpha) {
		// alpha is Application::interpolationAlpha(): how far this frame
		// sits between the last completed fixed step and the next one.
		// Clamped defensively -- a caller passing a stale or uninitialised
		// value should produce a still frame, never an extrapolated one.
		const float t = (alpha < 0.0f) ? 0.0f : ((alpha > 1.0f) ? 1.0f : alpha);

		m_item.transform.posX = m_prevX + (m_x - m_prevX) * t;
		m_item.transform.posY = m_prevY + (m_y - m_prevY) * t;

		// Facing and size are read here rather than in updateVisual because
		// m_facing is written by the fixed step, which runs AFTER onUpdate.
		// Reading it at draw time keeps the flip in sync with the position.
		m_item.transform.scaleX = kFrameW * kPixelScale * static_cast<float>(m_facing);
		m_item.transform.scaleY = kFrameH * kPixelScale;

		r2d.draw(m_item);
	}
```

`kFrameW`, `kFrameH` and `kPixelScale` are file-static
constants at the top of `Player.cpp` (lines 15-17), so they
are in scope here — they were already being used from
`update` a few lines up.

---

## 4. `Enemy.h`

Open `/home/atulo/Projects/HBE/MegaX/include/Game/Enemy.h`.
**Tabs.**

### 4.1 The tick / render declarations

Lines 42-43 currently read:

```cpp
		void tick(float dt, const Player& player);
		void render(HBE::Renderer::Renderer2D& r2d);
```

Replace both lines with:

```cpp
		// --- item 16: renamed from tick(). Runs on the fixed step. The
		// rename is deliberate: "tick" read as "once a frame" and it no
		// longer is, and there is exactly one call site to update.
		void fixedTick(float h, const Player& player);

		// --- item 16: advances the sprite animation on the render clock.
		// Called for dead and dying enemies too -- the death fade is an
		// animation.
		void updateVisual(float dt);

		void render(HBE::Renderer::Renderer2D& r2d, float alpha);
```

### 4.2 The previous position

The private position block currently reads (lines 180-182 —
the first three members after the `private:` helper
declarations, right below `void tickShooting(...)` on line
178):

```cpp
			float m_x = 0.0f;
			float m_y = 0.0f;
			float m_feetY = 0.0f;
```

Insert right after `float m_y = 0.0f;` (line 181) and
before `float m_feetY = 0.0f;` (line 182):

```cpp
			// item 16: position at the START of the current fixed step.
			float m_prevX = 0.0f;
			float m_prevY = 0.0f;
```

Final block:

```cpp
			float m_x = 0.0f;
			float m_y = 0.0f;
			// item 16: position at the START of the current fixed step.
			float m_prevX = 0.0f;
			float m_prevY = 0.0f;
			float m_feetY = 0.0f;
```

> **PASTE NOTE.** The private members in `Enemy.h` sit at
> **three** tabs, not two — the `private:` label itself is
> indented one level further than the `public:` section
> above it. Match the lines you are inserting between.

---

## 5. `Enemy.cpp`

Open `/home/atulo/Projects/HBE/MegaX/src/Game/Enemy.cpp`.
**4 spaces.**

### 5.1 `spawn` — the teleport

Lines 85-89 currently read:

```cpp
    void Enemy::spawn(float x, float groundY, int facing) {
        m_x = x;
        m_feetY = groundY;
        m_y = posYForFeet(groundY);
        m_facing = (facing >= 0) ? +1 : -1;
```

Insert right after `m_facing = (facing >= 0) ? +1 : -1;`
(line 89) and before the blank line that precedes the
`// Physics body anchored to feet:` comment:

```cpp
        m_prevX = m_x;   // item 16: spawn is a teleport, do not interpolate
        m_prevY = m_y;
```

Final head of `spawn`:

```cpp
    void Enemy::spawn(float x, float groundY, int facing) {
        m_x = x;
        m_feetY = groundY;
        m_y = posYForFeet(groundY);
        m_facing = (facing >= 0) ? +1 : -1;
        m_prevX = m_x;   // item 16: spawn is a teleport, do not interpolate
        m_prevY = m_y;

        // Physics body anchored to feet: box bottom == groundY, box top ==
        // groundY + boxHalfH*2. Cy sits halfH above feet.
```

This matters more than the `Player` case: `spawnDemoEnemies`
runs on every `F5` **and** every `F7`, and an enemy is
usually further from its spawn point than the player is.

### 5.2 `tick` becomes `fixedTick`, and loses its animation

Lines 165-204 currently hold the whole of `Enemy::tick`.
**Replace lines 165-204 inclusive** — from
`void Enemy::tick(float dt, const Player& player) {` through
its closing brace — with:

```cpp
    // --- item 16: renamed from tick(); now runs on the fixed step.
    // Two changes beyond the rename and the parameter name: the previous
    // position is snapshotted at the top, and the two currentAnim().update()
    // calls have moved out to updateVisual() so animation stays on the
    // render clock.
    void Enemy::fixedTick(float h, const Player& player) {
        // First statement, for the same reason as Player::fixedUpdate.
        m_prevX = m_x;
        m_prevY = m_y;

        m_landedThisFrame = false;

        if (m_invulnTimer > 0.0f) m_invulnTimer = std::max(0.0f, m_invulnTimer - h);
        if (m_flashTimer > 0.0f) m_flashTimer = std::max(0.0f, m_flashTimer - h);
        if (m_jumpCooldown > 0.0f) m_jumpCooldown = std::max(0.0f, m_jumpCooldown - h);

        if (m_dead) {
            if (m_deathTimer > 0.0f) m_deathTimer = std::max(0.0f, m_deathTimer - h);
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

    // --- item 16: the render-clock half. Just the animation frame -- the
    // enemy's tint is computed in render() from timers the fixed step owns,
    // so there is nothing else to do here.
    void Enemy::updateVisual(float dt) {
        currentAnim().update(dt);
    }
```

Two deletions inside that replacement are easy to miss:

* the `currentAnim().update(dt);` that used to sit on **line
  174**, inside the `if (m_dead)` branch, and
* the `currentAnim().update(dt);` that used to sit on **line
  203**, immediately after `setAnimState(...)`.

Both are now `updateVisual`'s job. The dead branch still
`return`s early, and `EnemyManager::updateVisual` calls
`updateVisual` on **every** enemy in the vector including
dead ones, so the death animation still plays out.

`tickPatrol`, `tickSuspicious`, `tickAlert`, `tickChase`,
`tickSearch`, `tickReturn`, `tickShooting`, `applyPhysics`
and `syncFromBox` are all unchanged — they keep their `float
dt` parameter names for the same reason `Player::updatePlay`
does.

### 5.3 `render` — the interpolation

Lines 499-520 currently read:

```cpp
    void Enemy::render(Renderer2D& r2d) {
        if (isFinished()) return;

        m_item.transform.posX = m_x;
        m_item.transform.posY = m_y;
        m_item.transform.scaleX = kFrameW * kPixelScale * static_cast<float>(m_facing);
        m_item.transform.scaleY = kFrameH * kPixelScale;
```

Replace the signature line and the two `posX` / `posY`
lines. The final function head should read:

```cpp
    void Enemy::render(Renderer2D& r2d, float alpha) {
        if (isFinished()) return;

        const float t = (alpha < 0.0f) ? 0.0f : ((alpha > 1.0f) ? 1.0f : alpha);

        m_item.transform.posX = m_prevX + (m_x - m_prevX) * t;
        m_item.transform.posY = m_prevY + (m_y - m_prevY) * t;
        m_item.transform.scaleX = kFrameW * kPixelScale * static_cast<float>(m_facing);
        m_item.transform.scaleY = kFrameH * kPixelScale;
```

Everything from `if (m_dead) {` onward — the tint block,
`currentAnim().apply(m_item);` and `r2d.draw(m_item);` — is
unchanged.

> **DO NOT interpolate the bubble icons.**
> `EnemyManager::renderBubbles` and `debugDrawSenses` draw
> from `e.x()` and `e.feetY()`, and they stay that way. A
> `?` bubble sitting one sub-step ahead of the head it
> belongs to is not a bug anyone will ever notice, and
> plumbing alpha into two debug overlays is cost with no
> return.

---

## 6. `EnemyManager`

### 6.1 `EnemyManager.h`

Open `/home/atulo/Projects/HBE/MegaX/include/Game/EnemyManager.h`.
**4 spaces.**

Lines 69-70 currently read:

```cpp
        void update(float dt);
        void render(HBE::Renderer::Renderer2D& r2d);
```

Replace both lines with:

```cpp
        // --- item 16: was update(dt). Runs on the fixed step: ticks every
        // enemy, dispatches death/landing FX, and culls finished enemies.
        void fixedUpdate(float h);

        // --- item 16: render-clock half. Advances every enemy's sprite
        // animation, including the dead ones mid-fade.
        void updateVisual(float dt);

        void render(HBE::Renderer::Renderer2D& r2d, float alpha);
```

### 6.2 `EnemyManager.cpp`

Open `/home/atulo/Projects/HBE/MegaX/src/Game/EnemyManager.cpp`.
**4 spaces.**

Lines 159-183 currently hold `update` and `render`:

```cpp
    void EnemyManager::update(float dt) {
        if (!m_player) return;

        for (auto& e : m_enemies) e.tick(dt, *m_player);
        ...
    }

    void EnemyManager::render(Renderer2D& r2d) {
        for (auto& e : m_enemies) e.render(r2d);
    }
```

**Replace lines 159-183 inclusive** with:

```cpp
    void EnemyManager::fixedUpdate(float h) {
        if (!m_player) return;

        for (auto& e : m_enemies) e.fixedTick(h, *m_player);

        for (auto& e : m_enemies) {
            if (e.isFinished()) continue;
            if (e.justDied() && m_effects) {
                m_effects->spawnEnemyExplosion(e.x(), e.feetY() + e.boxHalfH);
                e.consumeJustDied();
            }
            if (!e.isAlive()) continue;
            if (e.landedThisFrame() && m_effects) {
                m_effects->spawnLandingDust(e.x(), e.feetY(), e.groundTileId());
            }
        }
        m_enemies.erase(
            std::remove_if(m_enemies.begin(), m_enemies.end(),
                [](const Enemy& e) { return e.isFinished(); }),
            m_enemies.end());
    }

    // item 16: no guard on m_player -- animation must keep running even if
    // the player reference is momentarily null (it is cleared and reset
    // around a scene reload), or an enemy freezes mid-frame during F5.
    void EnemyManager::updateVisual(float dt) {
        for (auto& e : m_enemies) e.updateVisual(dt);
    }

    void EnemyManager::render(Renderer2D& r2d, float alpha) {
        for (auto& e : m_enemies) e.render(r2d, alpha);
    }
```

The body of the old `update` is carried over **verbatim**
apart from `dt` → `h` and `tick` → `fixedTick`. The FX
dispatch and the `erase` stay on the fixed step because
`justDied()` and `landedThisFrame()` are one-shot flags that
the fixed step sets and clears — reading them from the
render clock would miss them whenever two steps ran in one
frame, and read them twice whenever none did.

---

## 7. The two bullet managers

Both are the same edit twice. Bullets are the entity where
skipping interpolation shows most: they are the fastest
things on screen (`speed = 640` world px/s for the player's,
`bulletSpeed = 480` for the enemy's), so at 120 FPS with a
60 Hz step an uninterpolated bullet visibly advances in
5-plus-pixel jumps every other frame.

### 7.1 `Bullet.h`

Open `/home/atulo/Projects/HBE/MegaX/include/Game/Bullet.h`.
**Tabs.**

The `Bullet` struct currently reads (lines 47-51):

```cpp
		struct Bullet {
			float x = 0.0f, y = 0.0f;
			float vx = 0.0f;
			bool  alive = true;
		};
```

Replace all five lines with:

```cpp
		struct Bullet {
			float x = 0.0f, y = 0.0f;
			// item 16: position at the START of the current fixed step.
			float px = 0.0f, py = 0.0f;
			float vx = 0.0f;
			bool  alive = true;
		};
```

Then line 36, which currently reads:

```cpp
		void render(HBE::Renderer::Renderer2D& r2d);
```

becomes:

```cpp
		void render(HBE::Renderer::Renderer2D& r2d, float alpha);
```

`update` keeps its signature — it is simply called from a
different place now.

### 7.2 `Bullet.cpp`

Open `/home/atulo/Projects/HBE/MegaX/src/Game/Bullet.cpp`.
**Tabs.** Three small edits.

**Edit A — `spawn`.** Lines 32-39 currently read:

```cpp
	void BulletManager::spawn(float x, float y, int dir) {
		Bullet b;
		b.x = x;
		b.y = y;
		b.vx = (dir >= 0 ? 1.0f : -1.0f) * speed;
		b.alive = true;
		m_bullets.push_back(b);
	}
```

Insert right after `b.y = y;` (line 35):

```cpp
		b.px = x; b.py = y;   // item 16: spawn frame draws at the muzzle
```

**Edit B — `update`.** The per-bullet loop currently reads
(lines 62-64):

```cpp
		for (auto& b : m_bullets) {
			if (!b.alive) continue;
			b.x += b.vx * dt;
```

Insert between `if (!b.alive) continue;` (line 63) and
`b.x += b.vx * dt;` (line 64):

```cpp
			b.px = b.x; b.py = b.y;   // item 16
```

Final head of the loop:

```cpp
		for (auto& b : m_bullets) {
			if (!b.alive) continue;
			b.px = b.x; b.py = b.y;   // item 16
			b.x += b.vx * dt;
```

**Edit C — `render`.** Lines 82-90 currently read:

```cpp
	void BulletManager::render(Renderer2D& r2d) {
		for (const auto& b : m_bullets) {
			m_item.transform.posX = b.x;
			m_item.transform.posY = b.y;
			m_item.transform.scaleX = length;
			m_item.transform.scaleY = height;
			r2d.draw(m_item);
		}
	}
```

Replace all nine lines with:

```cpp
	void BulletManager::render(Renderer2D& r2d, float alpha) {
		const float t = (alpha < 0.0f) ? 0.0f : ((alpha > 1.0f) ? 1.0f : alpha);

		for (const auto& b : m_bullets) {
			m_item.transform.posX = b.px + (b.x - b.px) * t;
			m_item.transform.posY = b.py + (b.y - b.py) * t;
			m_item.transform.scaleX = length;
			m_item.transform.scaleY = height;
			r2d.draw(m_item);
		}
	}
```

The clamp is hoisted out of the loop — it does not depend on
the bullet.

### 7.3 `EnemyBullet.h`

Open `/home/atulo/Projects/HBE/MegaX/include/Game/EnemyBullet.h`.
**4 spaces** (unlike its own `.cpp`).

The `Bullet` struct currently reads (lines 26-31):

```cpp
        struct Bullet {
            float x = 0.0f, y = 0.0f;
            float vx = 0.0f, vy = 0.0f;
            int damage = 1;
            bool alive = true;
        };
```

Replace all six lines with:

```cpp
        struct Bullet {
            float x = 0.0f, y = 0.0f;
            // item 16: position at the START of the current fixed step.
            float px = 0.0f, py = 0.0f;
            float vx = 0.0f, vy = 0.0f;
            int damage = 1;
            bool alive = true;
        };
```

Then line 40, which currently reads:

```cpp
        void render(HBE::Renderer::Renderer2D& r2d);
```

becomes:

```cpp
        void render(HBE::Renderer::Renderer2D& r2d, float alpha);
```

### 7.4 `EnemyBullet.cpp`

Open `/home/atulo/Projects/HBE/MegaX/src/Game/EnemyBullet.cpp`.
**Tabs.**

**Edit A — `spawn`.** Insert right after `b.y = sy;`
(line 43) and before `b.vx = dx * speed;` (line 44):

```cpp
		b.px = sx; b.py = sy;   // item 16
```

**Edit B — `update`.** The per-bullet loop currently reads
(lines 72-75):

```cpp
		for (auto& b : m_bullets) {
			if (!b.alive) continue;
			b.x += b.vx * dt;
			b.y += b.vy * dt;
```

Insert between `if (!b.alive) continue;` (line 73) and
`b.x += b.vx * dt;` (line 74):

```cpp
			b.px = b.x; b.py = b.y;   // item 16
```

**Edit C — `render`.** Lines 100-111 currently read:

```cpp
	void EnemyBulletManager::render(Renderer2D& r2d) {
		for (auto& b : m_bullets) {
			if (!b.alive) continue;
			m_item.transform.posX = b.x;
			m_item.transform.posY = b.y;
			const float ang = std::atan2(b.vy, b.vx);
			m_item.transform.rotation = ang;
			m_item.transform.scaleX = length;
			m_item.transform.scaleY = height;
			r2d.draw(m_item);
		}
	}
```

Replace all twelve lines with:

```cpp
	void EnemyBulletManager::render(Renderer2D& r2d, float alpha) {
		const float t = (alpha < 0.0f) ? 0.0f : ((alpha > 1.0f) ? 1.0f : alpha);

		for (auto& b : m_bullets) {
			if (!b.alive) continue;
			m_item.transform.posX = b.px + (b.x - b.px) * t;
			m_item.transform.posY = b.py + (b.y - b.py) * t;
			// Rotation is NOT interpolated: these bullets travel in a
			// straight line at constant velocity, so the angle is the same
			// at both ends of the step and a lerp would be arithmetic with
			// no visible effect.
			const float ang = std::atan2(b.vy, b.vx);
			m_item.transform.rotation = ang;
			m_item.transform.scaleX = length;
			m_item.transform.scaleY = height;
			r2d.draw(m_item);
		}
	}
```

---

## 8. What NOT to touch

* **Do not touch `Effects.h` / `Effects.cpp`.** Particles are
  visual-only and the work item forbids putting them on the
  fixed step. They keep running from `GameLayer::onUpdate`
  with the real `dt`, and they are never interpolated —
  their positions are already sub-frame smooth because they
  integrate on the render clock.
* **Do not touch `World.h` / `World.cpp`.** `World::update`
  advances animated tiles. That is animation.
* **Do not interpolate `hurtbox()` or `hitbox()`.** They are
  built from `m_box` / `m_x`, which is the authoritative
  simulated position, and combat must resolve against the
  simulation, not against what happens to be on screen.
  Golden rule 7.
* **Do not add `m_prevFeetY` to `Enemy`.** `feetY()` feeds
  landing dust and the sense overlays, both of which are
  spawned or drawn from simulated state.
* **Do not "optimise" the previous position into a single
  `m_prev` struct** shared through a base class. Four
  entities, two floats each; a `Transform2D`-shaped
  abstraction here would touch every file in the game for
  no behavioural gain.
* **Do not make `Player::updateVisual` write
  `m_item.transform`.** It runs before the fixed step. The
  position and facing it wrote would be a step stale, and
  `render` would overwrite the position but not the facing —
  producing a sprite that flips one frame late.

---

## 9. Sanity check before doc `06`

```fish
cd /home/atulo/Projects/HBE/MegaX

# Every render() that needs alpha now takes it — 5 declarations:
grep -rn 'render(HBE::Renderer::Renderer2D& r2d, float alpha)' include/Game/
# -> Player.h, Enemy.h, EnemyManager.h, Bullet.h, EnemyBullet.h

# The old names are gone from the entity sources:
grep -rn 'void Player::update(\|void Enemy::tick(\|void EnemyManager::update(' src/Game/
# -> no output

# Each entity snapshots its previous position once per fixed step, and
# writes both positions on every teleport. Player and Enemy therefore
# each carry two 'm_prevX =' assignments; the bullets carry two 'b.px ='.
grep -c 'm_prevX = x\|m_prevX = m_x' src/Game/Player.cpp src/Game/Enemy.cpp
# -> 2 and 2   (one teleport + one step snapshot each)
grep -c 'b.px = x\|b.px = sx\|b.px = b.x' src/Game/Bullet.cpp src/Game/EnemyBullet.cpp
# -> 2 and 2

# Enemy animation left the fixed step entirely. Match the call, not the
# word -- the comment above fixedTick() mentions currentAnim().update()
# and would otherwise inflate the count to 2.
grep -c 'currentAnim().update(dt);' src/Game/Enemy.cpp
# -> 1   (was 2; the survivor is inside updateVisual)
```

**Should MegaX compile now?** **No, and these are the errors
to expect.** Every one of them is a `GameLayer.cpp` call
site that doc `06` rewrites:

```
error: too few arguments to function call, expected 2, have 1
    m_player.render(r2d);
error: no member named 'update' in 'MegaX::Player'; did you mean 'updateVisual'?
    m_player.update(dt);
error: no member named 'update' in 'MegaX::EnemyManager'
    m_enemies.update(dt);
error: too few arguments to function call, expected 2, have 1
    m_bullets.render(r2d);
```

`HBE.Core` and `HBE.Sandbox` still build cleanly — nothing
in this doc left `MegaX/`. If you want to confirm the engine
is still fine before moving on:

```fish
cd /home/atulo/Projects/HBE
cmake --build --preset linux-clang-debug --target HBE.Sandbox
```

Any error mentioning a file outside `MegaX/src/Game/` means
something in this doc landed in the wrong file.

Next: `06_megax_gamelayer_and_main.md`.
