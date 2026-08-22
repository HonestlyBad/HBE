# Item 06 · Doc 02 — Player Shooting

Edits to `include/Game/Player.h` and `src/Game/Player.cpp`. The player gains a
fire input, an auto‑fire cadence, three shoot animations, a pose‑dependent muzzle,
and an air‑only recoil impulse. It exposes `consumeShot()` so the layer can spawn
a bullet at the gun tip.

---

## `include/Game/Player.h`

### Public API — add a fire input + a shot query

```cpp
		void setCrouchInput(bool held) { m_crouchHeld = held; }
		void setFireInput(bool pressed, bool held) {
			if (pressed) m_firePressed = true;   // latched until consumed in update()
			m_fireHeld = held;
		}

		// If the player fired this frame, returns true once and outputs the
		// muzzle world position + direction (+1/-1) for a bullet spawn.
		bool consumeShot(float& x, float& y, int& dir);
```

### Private state — latched fire input + shooting bookkeeping

```cpp
		// latched input intents
		bool m_jumpPressed = false;
		bool m_jumpHeld    = false;
		bool m_crouchHeld  = false;
		bool m_firePressed = false;
		bool m_fireHeld    = false;

		// shooting state
		float m_fireCooldown = 0.0f;   // time until the next shot may fire
		float m_shootTimer   = 0.0f;   // time left showing a shoot pose
		float m_recoilVx     = 0.0f;   // decaying air knockback velocity
		bool  m_shotPending  = false;  // a bullet is waiting to be spawned
		float m_shotX = 0.0f, m_shotY = 0.0f;
		int   m_shotDir = 1;
```

### Animation — extend the state comment + add three shoot anims

```cpp
		int m_animState = -1;   // 0 idle,1 walk,2 crouch,3 rise,4 fall,5 land,6 shootStand,7 shootWalk,8 shootAir
		...
		HBE::Renderer::SpriteAnimation m_landAnim;
		HBE::Renderer::SpriteAnimation m_shootStandAnim;
		HBE::Renderer::SpriteAnimation m_shootWalkAnim;
		HBE::Renderer::SpriteAnimation m_shootAirAnim;
```

---

## `src/Game/Player.cpp`

### 1. Shoot rows + shooting tunables (near the other constants)

```cpp
	// shoot poses
	static constexpr int kShootStandRow = 10, kShootStandCol0 = 0, kShootStandCol1 = 1;  // standing + muzzle flash
	static constexpr int kShootWalkRow  = 5,  kShootWalkCol0  = 0, kShootWalkCol1  = 9;  // run-and-gun
	static constexpr int kShootAirRow   = 8,  kShootAirCol0   = 0, kShootAirCol1   = 2;  // airborne shoot
	...
	static constexpr float kShootFps  = 14.0f;

	// ---- shooting tunables (frame-local: +x forward, +y up) ----------------
	static constexpr float kFireCooldown = 0.15f;   // seconds between shots (auto-fire while held)
	static constexpr float kShootHold    = 0.22f;   // how long the shoot pose stays up
	static constexpr float kRecoilImpulse = 120.0f; // air knockback velocity (px/s)
	static constexpr float kRecoilDamp    = 7.0f;   // recoil decay rate

	static constexpr float kMuzzleFwdStand = 30.0f, kMuzzleUpStand = 1.0f;
	static constexpr float kMuzzleFwdAir   = 30.0f, kMuzzleUpAir   = 2.0f;
	static constexpr float kMuzzleFwdCrouch = 28.0f, kMuzzleUpCrouch = -14.0f;
```

### 2. Build the shoot anims in `init` (alongside the other `SpriteAnimation`s)

```cpp
		m_shootStandAnim = SpriteAnimation(&m_activeSheet, kShootStandCol0, kShootStandCol1, kShootStandRow, kShootFps, true);
		m_shootWalkAnim  = SpriteAnimation(&m_activeSheet, kShootWalkCol0,  kShootWalkCol1,  kShootWalkRow,  kWalkFps,  true);
		m_shootAirAnim   = SpriteAnimation(&m_activeSheet, kShootAirCol0,   kShootAirCol1,   kShootAirRow,   kShootFps, true);
```

### 3. Map states 6/7/8 in `animForState`

```cpp
			case 6: return m_shootStandAnim;
			case 7: return m_shootWalkAnim;
			case 8: return m_shootAirAnim;
```

### 4. `consumeShot` — hand the pending shot to the layer

```cpp
	bool Player::consumeShot(float& x, float& y, int& dir) {
		if (!m_shotPending) return false;
		x = m_shotX;
		y = m_shotY;
		dir = m_shotDir;
		m_shotPending = false;
		return true;
	}
```

### 5. Consume the latched fire press in `update()`

Right where the jump press is consumed each frame, also clear the fire press:

```cpp
		m_firePressed = false;   // consume the latched fire press
```

### 6. Recoil in the horizontal‑velocity line (`updatePlay`)

`updatePlay` rewrites `m_vx` from input every frame, which would erase any
impulse — so recoil lives in a **separate decaying** `m_recoilVx` that's added on
top, and cleared the instant we're grounded:

```cpp
		// grounded cancels any leftover air recoil so feet grip the floor
		if (onGroundPrev) m_recoilVx = 0.0f;
		...
		m_vx = ix * speed + m_recoilVx;
		m_recoilVx *= std::exp(-kRecoilDamp * dt);
```

### 7. The shooting block (after gravity/movement, before collision resolve)

```cpp
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
			else                  { mfx = kMuzzleFwdStand;  mfy = kMuzzleUpStand; }

			m_shotX = m_x + static_cast<float>(m_facing) * mfx;
			m_shotY = m_y + mfy;
			m_shotDir = m_facing;
			m_shotPending = true;

			if (airborne) m_recoilVx = -static_cast<float>(m_facing) * kRecoilImpulse;  // gun pushes back in air
		}
```

### 8. Shoot‑pose overlay at the end of the animation state machine

After the normal state is picked (idle/walk/crouch/rise/fall/land), override it
with the matching shoot pose while `m_shootTimer` is active:

```cpp
		// shooting overlay: swap to the matching shoot pose while it's active
		if (m_shootTimer > 0.0f) {
			if (!onGround)        st = 8;   // air shoot
			else if (m_crouching) st = 2;   // stay prone (still fired)
			else if (ix != 0.0f)  st = 7;   // run-and-gun
			else                  st = 6;   // stand shoot
		}
		setAnimState(st);
```

---

## Why it's built this way

| Concern | Solution |
|---------|----------|
| Instant first shot **and** sustained auto‑fire | `m_firePressed` (latched, one‑shot) OR `m_fireHeld`, gated by `m_fireCooldown`. |
| Pose must linger between rapid shots | `m_shootTimer` (`kShootHold` 0.22 s > cooldown 0.15 s) keeps the shoot state selected. |
| `updatePlay` overwrites `m_vx` every frame | Recoil kept in `m_recoilVx`, exp‑decayed, added on top of input velocity. |
| Recoil should only matter in the air | Set only when `airborne`; zeroed when `onGroundPrev`. |
| Bullet must leave the gun tip both ways | Muzzle offset is frame‑local, `x` scaled by `m_facing`. |
| No shooting during map building | Whole block lives in `updatePlay` (Play mode only). |

**Crouch shooting** keeps the prone pose (row 1) and fires from a low muzzle;
there's no dedicated crouch‑shoot frame on the sheet. Swap `kShootStandRow` etc.
if you prefer a different gun row.

Next: `03_gamelayer_and_project.md`.
