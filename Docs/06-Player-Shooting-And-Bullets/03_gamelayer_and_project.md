# Item 06 · Doc 03 — GameLayer Wiring & Project Registration

Wire the fire key, spawn bullets from the player's reported shots, and
update/render the bullet manager. Then register the two new files in the project.

---

## `include/Game/GameLayer.h`

Include the header and add the manager + a cached solid‑layer pointer:

```cpp
#include "Game/Bullet.h"
...
		BulletManager m_bullets{};
		const HBE::Renderer::TileMapLayer* m_ground = nullptr;
```

`m_ground` caches the `Ground` layer so both the player **and** the bullets use
the exact same solid data (and we don't re‑`findLayer` every frame).

---

## `src/Game/GameLayer.cpp`

### `onAttach` — cache the ground layer, init the bullets

```cpp
		// --- collision: the "Ground" layer carries the solid tiles ---
		m_ground = m_world.map().findLayer("Ground");
		if (!m_ground) {
			LogError("MegaX GameLayer: no 'Ground' layer in map — player will not collide.");
		}
		m_player.setCollision(&m_world.map(), m_ground);

		if (!m_bullets.init(app.resources(), m_quadMesh, m_spriteShader)) {
			LogError("MegaX GameLayer: bullet manager init failed.");
		}
```

The bullet manager reuses the **same quad mesh + sprite shader** the player and
tiles already use, so nothing new is loaded.

### `onUpdate` — read `J`, feed the player, spawn + advance bullets

```cpp
		// shooting (J) — semi/auto-fire handled in Player at a cadence
		const bool firePressed = Input::IsKeyPressed(SDL_SCANCODE_J);
		const bool fireHeld    = Input::IsKeyDown(SDL_SCANCODE_J);
		...
		m_player.setFireInput(firePressed, fireHeld);
		m_player.update(dt);

		// spawn any bullet the player fired this frame, then advance bullets
		float bx, by; int bdir;
		if (m_player.consumeShot(bx, by, bdir)) {
			m_bullets.spawn(bx, by, bdir);
		}
		m_bullets.update(dt, &m_world.map(), m_ground, m_camera.camera());
```

`IsKeyPressed` gives the instant first shot; `IsKeyDown` lets the player's cadence
auto‑fire while held. `consumeShot` returns the muzzle world position + facing so
the bullet leaves the gun tip.

### `onRender` — draw the bullets after the player

```cpp
		m_world.render(r2d);
		m_player.render(r2d);
		m_bullets.render(r2d);
```

Bullets are on render layer 101 (> player 100), so they draw over the player
regardless of submission order.

### Full input map (for reference)

| Key | Action |
|-----|--------|
| A / D | Move left / right |
| Space | Jump (Play) / fly up (Ghost) |
| S | Crouch (Play) / fly down (Ghost) |
| **J** | **Fire** (tap = one shot, hold = auto‑fire) |
| H | Toggle helmet |
| G | Toggle Play ↔ Ghost |

---

## `MegaX.vcxproj` — register the new files

Add the bullet header to the `ClInclude` group and the source to the `ClCompile`
group:

```xml
    <ClInclude Include="include\Game\Bullet.h" />
    ...
    <ClCompile Include="src\Game\Bullet.cpp" />
```

Without these two entries the new files won't compile/link and you'll get
unresolved `BulletManager` symbols.

Next: `04_build_run_and_verify.md`.
