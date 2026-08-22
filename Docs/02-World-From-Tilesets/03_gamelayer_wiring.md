# Item 02 · Doc 03 — Wire the `World` into `GameLayer`

Four small edits: include + member in the header, then load / update / render in
the source. Each edit shows the **existing** code (from item 01) and the
**replacement**.

---

## `include/Game/GameLayer.h`

**A. Add the include** (next to the existing `Game/Player.h` include):

```cpp
#include "Game/Player.h"
#include "World/World.h"          // <-- add
```

**B. Add the member** (next to `Player m_player{};`):

```cpp
        HBE::Renderer::CameraController m_camera{};
        Player m_player{};
        World  m_world{};          // <-- add
```

---

## `src/Game/GameLayer.cpp`

### C. Load the world in `onAttach`

Find the camera setup that ends with `m_camera.snapZoom(kCameraZoom);` and insert
the world load **right after it** (the world needs the sprite shader + quad mesh
built by `buildSpritePipeline()`, which already ran above):

```cpp
		m_camera.snapZoom(kCameraZoom);

		// ---- Load the tile world ------------------------------------------
		if (!m_world.load(app.renderer2D(), app.resources(),
			m_spriteShader, m_quadMesh, "maps/level_01.json")) {
			LogError("MegaX GameLayer: world load failed (continuing empty).");
		}
```

### D. Spawn the ghost at the map center

Replace the fixed `(0,0)` spawn:

```cpp
		m_player.setPosition(0.0f, 0.0f);
		m_camera.snapTo(0.0f, 0.0f);
```

with a center-of-map spawn (falls back to origin when the world is empty, since
`pixelWidth/Height` return `0`):

```cpp
		// Spawn the ghost near the middle of the map so tiles surround it.
		const float startX = m_world.pixelWidth()  * 0.5f;
		const float startY = m_world.pixelHeight() * 0.5f;
		m_player.setPosition(startX, startY);
		m_camera.snapTo(startX, startY);
```

> `m_player.init(...)` must still run **before** this (it already does — leave it
> where it is). Only the two spawn lines change.

### E. Tick the world in `onUpdate`

Add one line (advances the animated-tile clock; harmless in item 02). Put it just
before the player update:

```cpp
		m_world.update(dt);
		m_player.setMoveInput(ix, iy);
		m_player.update(dt);
```

### F. Draw the world in `onRender`

Draw tiles **inside the existing World pass, before the player** so the player
(render layer 100) sorts on top:

```cpp
	void GameLayer::onRender() {
		Renderer2D& r2d = m_app->renderer2D();

		r2d.beginScene(m_camera.camera(), RenderPass::World);
		m_world.render(r2d);    // tiles first (behind)
		m_player.render(r2d);   // player on top
		r2d.endScene();
	}
```

---

## Why this order works

- **`World::render` is called inside `beginScene(...World)`** — required, because
  `TileMapRenderer::draw` reads the active camera for culling.
- Even though `m_world.render` is called before `m_player.render`, final on‑screen
  order is decided by **render layer** (tiles = 0, player = 100), so the player is
  always drawn on top regardless of call order.
- `m_world.update(dt)` does nothing visible yet; it exists so item 03 needs zero
  wiring changes here.

No other files change. Next: add the new files to the project and build
(`04_build_run_and_verify.md`).
