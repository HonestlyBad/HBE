# Item 04 · Doc 02 — Wiring the Player into `GameLayer`

Only `src/Game/GameLayer.cpp` changes here. `GameLayer.h` stays exactly as it is
(no new members). We do three things:

1. In `onAttach`, hand the player the **`Ground`** collision layer after the
   world loads.
2. Keep the spawn at the map center (the player now **falls** onto the ground).
3. In `onUpdate`, map inputs to the new Play‑mode intents (run, jump, crouch)
   while still driving the old ghost `iy` for when item 05 flips modes.

---

## 1. `onAttach` — give the player the collision layer

Find this block in `onAttach`:

```cpp
        if (!m_world.load(app.renderer2D(), app.resources(),
            m_spriteShader, m_quadMesh, "maps/level_01.json")) {
            LogError("MegaX GameLayer: world load failed (continuing empty.)");
        }

		if (!m_player.init(app.resources(), m_quadMesh, m_spriteShader)) {
			LogError("MegaX GameLayer: player init failed.");
		}
```

Replace it with (adds the `setCollision` wiring after both are initialized):

```cpp
        if (!m_world.load(app.renderer2D(), app.resources(),
            m_spriteShader, m_quadMesh, "maps/level_01.json")) {
            LogError("MegaX GameLayer: world load failed (continuing empty.)");
        }

		if (!m_player.init(app.resources(), m_quadMesh, m_spriteShader)) {
			LogError("MegaX GameLayer: player init failed.");
		}

		// --- collision: the "Ground" layer carries the solid tiles ---
		const HBE::Renderer::TileMapLayer* ground = m_world.map().findLayer("Ground");
		if (!ground) {
			LogError("MegaX GameLayer: no 'Ground' layer in map — player will not collide.");
		}
		m_player.setCollision(&m_world.map(), ground);
```

> If your collision layer is named differently in the map JSON, change the string
> `"Ground"`. `setCollision` tolerates a null layer (player just won't collide),
> so a typo shows up as "falls through the world" rather than a crash.

The spawn code right after is unchanged — the player spawns at the map center and
gravity pulls it down onto the first solid tile below:

```cpp
        const float startX = m_world.pixelWidth() * 0.5f;
        const float startY = m_world.pixelHeight() * 0.5f;
        m_player.setPosition(startX, startY);
        m_camera.snapTo(startX, startY);
		app.gl().setCamera(m_camera.camera());
```

> If the map center has no floor beneath it the player will fall out of the
> world. Adjust `startX/startY` to sit above solid ground (see doc 03
> troubleshooting).

---

## 2. `onUpdate` — map the platformer inputs

Replace the current input block at the top of `onUpdate`:

```cpp
	void GameLayer::onUpdate(float dt) {
		const float ix = (Input::IsKeyDown(SDL_SCANCODE_D) ? 1.0f : 0.0f)
			- (Input::IsKeyDown(SDL_SCANCODE_A) ? 1.0f : 0.0f);
		const float iy = (Input::IsKeyDown(SDL_SCANCODE_SPACE) ? 1.0f : 0.0f)
			- (Input::IsKeyDown(SDL_SCANCODE_S) ? 1.0f : 0.0f);

		if (Input::IsKeyPressed(SDL_SCANCODE_H)) {
			m_player.toggleHelmet();
		}

        m_world.update(dt);
		m_player.setMoveInput(ix, iy);
		m_player.update(dt);
```

with:

```cpp
	void GameLayer::onUpdate(float dt) {
		// horizontal run (both modes)
		const float ix = (Input::IsKeyDown(SDL_SCANCODE_D) ? 1.0f : 0.0f)
			- (Input::IsKeyDown(SDL_SCANCODE_A) ? 1.0f : 0.0f);

		// vertical fly intent — only used by Ghost mode; Play mode ignores iy
		const float iy = (Input::IsKeyDown(SDL_SCANCODE_SPACE) ? 1.0f : 0.0f)
			- (Input::IsKeyDown(SDL_SCANCODE_S) ? 1.0f : 0.0f);

		// platformer intents (Play mode)
		const bool jumpPressed = Input::IsKeyPressed(SDL_SCANCODE_SPACE); // one-shot
		const bool jumpHeld    = Input::IsKeyDown(SDL_SCANCODE_SPACE);    // for variable height
		const bool crouchHeld  = Input::IsKeyDown(SDL_SCANCODE_S);

		if (Input::IsKeyPressed(SDL_SCANCODE_H)) {
			m_player.toggleHelmet();
		}

        m_world.update(dt);
		m_player.setMoveInput(ix, iy);
		m_player.setJumpInput(jumpPressed, jumpHeld);
		m_player.setCrouchInput(crouchHeld);
		m_player.update(dt);
```

The rest of `onUpdate` (camera follow) is unchanged:

```cpp
		m_camera.setFollowTarget(m_player.x(), m_player.y());
		m_camera.setFollowVelocity(m_player.velX(), m_player.velY());
		m_camera.update(dt);
		m_app->gl().setCamera(m_camera.camera());
	}
```

---

## Input summary (Play mode)

| Key | Action |
|-----|--------|
| `A` / `D` | Run left / right |
| `Space` (tap) | Jump |
| `Space` (hold) | Higher jump (variable height) |
| `S` (hold) | Crouch (shrinks hitbox; blocks running) |
| `H` (tap) | Toggle helmet |

In **Ghost mode** (item 05), `Space`/`S` become fly up/down again via the `iy`
intent that we still compute — no wiring change needed later.

## Why no other files change

- `GameLayer.h` — `m_player`, `m_world`, `m_camera` already exist; no new members.
- `World.*` — we only *read* it via the existing `map()` accessor.
- `MegaX.vcxproj` — `Player.*`, `GameLayer.*`, `World.*` are already compiled.
- Engine / `HBE.Sandbox` — untouched, as required.

Next: `03_build_run_and_verify.md`.
