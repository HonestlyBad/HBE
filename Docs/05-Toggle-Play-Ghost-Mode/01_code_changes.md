# Item 05 · Doc 01 — Code Changes

Two small edits. Both files already exist from items 01–04.

---

## 1. `src/Game/Player.cpp` — tint the sprite by mode

In `Player::update`, in the **shared** block that runs after the Play/Ghost
dispatch (right where the transform is written), add the tint line. Find:

```cpp
	void Player::update(float dt) {
		if (m_mode == Mode::Ghost) updateGhost(dt);
		else                       updatePlay(dt);

		m_jumpPressed = false;   // consume the latched one-shot press

		// shared: write transform + advance the current animation
		m_item.transform.posX = m_x;
		m_item.transform.posY = m_y;
		m_item.transform.scaleX = kFrameW * kPixelScale * static_cast<float>(m_facing);
		m_item.transform.scaleY = kFrameH * kPixelScale;

		SpriteAnimation& a = animForState(m_animState < 0 ? 0 : m_animState);
		a.update(dt);
		a.apply(m_item);
	}
```

and insert the two tint lines after the transform assignment:

```cpp
		m_item.transform.scaleX = kFrameW * kPixelScale * static_cast<float>(m_facing);
		m_item.transform.scaleY = kFrameH * kPixelScale;

		// Ghost mode reads as a translucent, bluish "spirit"; Play mode is opaque.
		m_item.tint = (m_mode == Mode::Ghost) ? Color4{ 0.6f, 0.8f, 1.0f, 0.5f }
		                                      : Color4{ 1.0f, 1.0f, 1.0f, 1.0f };

		SpriteAnimation& a = animForState(m_animState < 0 ? 0 : m_animState);
		a.update(dt);
		a.apply(m_item);
```

`Color4` is already in scope via `using namespace HBE::Renderer;` (it comes from
`RenderItem.h`/`Color.h`, already included by `Player.h`). `apply()` only writes
`uvRect`, so setting the tint here holds for the whole frame.

> The tint relies on the material's default `BlendMode::Alpha`, which the sprite
> pipeline already uses — so alpha `0.5` renders translucent with no extra setup.

---

## 2. `src/Game/GameLayer.cpp` — the `G` hot‑key

### a) Add the toggle next to the existing `H` (helmet) key in `onUpdate`

Find:

```cpp
		if (Input::IsKeyPressed(SDL_SCANCODE_H)) {
			m_player.toggleHelmet();
		}
```

and add the `G` block right after it:

```cpp
		if (Input::IsKeyPressed(SDL_SCANCODE_H)) {
			m_player.toggleHelmet();
		}

		// G toggles Play <-> Ghost (fly, no gravity/collision — for map building)
		if (Input::IsKeyPressed(SDL_SCANCODE_G)) {
			m_player.toggleMode();
			LogInfo(m_player.mode() == Player::Mode::Ghost
				? "MegaX: Ghost mode (fly, no collision)."
				: "MegaX: Play mode (gravity + collision).");
		}
```

`Input::IsKeyPressed` is edge‑triggered (fires once per press), so a single tap
flips the mode once. `LogInfo` is already available via
`using namespace HBE::Core;`.

### b) Fix the stale attach log (optional but tidy)

Find:

```cpp
		LogInfo("MegaX GameLayer attached (player ghost state).");
```

and replace with:

```cpp
		LogInfo("MegaX GameLayer attached (Play mode; press G for Ghost).");
```

---

## That's all

No header edits, no new members. Everything the toggle needs (`Mode`,
`toggleMode`, `mode`, the Ghost update path, and the dual input feed in
`GameLayer::onUpdate`) already shipped in item 04.

Next: `02_build_run_and_verify.md`.
