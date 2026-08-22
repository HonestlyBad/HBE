# Item 07 · Doc 02 — Player & Bullet Hooks

Small, surgical additions so the effects layer can react to what's already
happening. **No behavioural changes** — we only surface signals that already
exist inside `Player::updatePlay` and `BulletManager::update`.

---

## 1. `Player` — expose "landed this frame", ground tile, and feet Y

### `include/Game/Player.h`

Add three public getters and two private members. Everything else in the
file stays the same.

Public block (put next to `hasHelmet()`):

```cpp
        // --- item 07: signals the effects layer reads --------------------
        // True for exactly one frame: the frame we touched ground after being
        // airborne. Cleared at the top of the next update().
        bool  landedThisFrame() const { return m_landedThisFrame; }

        // 1-based tile ID directly beneath the feet while grounded; 0 if
        // airborne or the sample missed a tile.
        int   groundTileId()  const { return m_groundTileId; }

        // World-space Y of the physics box bottom (i.e. the player's feet).
        float feetY()         const { return m_box.cy - m_box.h * 0.5f; }
```

Private members (put with the other physics box fields — near
`m_landTimer`):

```cpp
        bool m_landedThisFrame = false;
        int  m_groundTileId    = 0;
```

> Why a "landed" flag on `Player` rather than the layer computing it:
> `Player` already knows `onGroundPrev` and the new `res.grounded` — a
> duplicate transition detector in `GameLayer` would just risk drifting
> from the animation state machine.

### `src/Game/Player.cpp` — set the two signals inside `updatePlay`

Two tiny edits in `Player::updatePlay(float dt)`.

**Edit A** — clear the one-shot flag at the *very start* of the function,
right after the timer decrement lines (`m_coyote  = std::max(...)`):

```cpp
        m_landedThisFrame = false;
```

**Edit B** — right after the existing `landedThisFrame` local is computed
and the land timer set:

```cpp
        // (existing lines you already have)
        const bool landedThisFrame = (!onGroundPrev && res.grounded);
        m_landTimer = std::max(0.0f, m_landTimer - dt);
        if (landedThisFrame) m_landTimer = kLandTime;

        // --- item 07: publish the landing edge --------------------------
        m_landedThisFrame = landedThisFrame;

        // --- item 07: sample the tile directly under the feet -----------
        // Use the physics box bottom (feet), then step one pixel into the
        // solid tile below. `m_collLayer` may be null on maps without a
        // Ground layer, in which case we leave the id at 0.
        m_groundTileId = 0;
        if (m_grounded && m_map && m_collLayer) {
            const float tw = m_map->worldTileW();
            const float th = m_map->worldTileH();
            if (tw > 0.0f && th > 0.0f) {
                const float bottom = m_box.cy - m_box.h * 0.5f;
                const int tx = static_cast<int>(std::floor(m_box.cx / tw));
                const int ty = static_cast<int>(std::floor((bottom - 0.5f) / th));
                m_groundTileId = m_collLayer->at(tx, ty);
            }
        }
```

Rules:

- We sample **`bottom - 0.5f`** because the collision solver leaves the
  feet flush with the top of the tile below; stepping half a pixel into
  the tile guarantees we hit the *floor* tile, not the empty tile the
  feet occupy.
- If the Ground layer's `at(tx, ty)` returns `0`, we keep it as `0` —
  the effects layer falls back to a tan colour.

Confirm imports are already present at the top of `Player.cpp` (they are
in the current file: `<cmath>`, `TileMap.h` via `TileCollision.h`, etc.).

---

## 2. `BulletManager` — record impacts, drain them once per frame

### `include/Game/Bullet.h`

Add an `Impact` type + a consume-queue API. The rest of the file is
unchanged.

Inside `class BulletManager` (right below the `count()` line):

```cpp
        struct Impact {
            float x = 0.0f;
            float y = 0.0f;
            int   tileId = 0;   // 1-based tile ID hit (0 if unknown)
        };

        // Drain the impact queue built during the last update().
        // Returns true if any impact was popped.
        bool consumeImpacts(std::vector<Impact>& out) {
            if (m_impacts.empty()) return false;
            out.insert(out.end(), m_impacts.begin(), m_impacts.end());
            m_impacts.clear();
            return true;
        }
```

Private data (next to `std::vector<Bullet> m_bullets;`):

```cpp
        std::vector<Impact> m_impacts;
```

### `src/Game/Bullet.cpp` — capture the tile ID at the death point

Two changes.

**Change 1** — make `pointInSolid` (or a new helper) also return the tile
ID at the sample point. The cleanest approach is a second small helper
that reuses the same math:

```cpp
    // returns 0 if the point is not inside a solid tile
    static int solidTileIdAt(const TileMap* map, const TileMapLayer* layer,
                             float x, float y)
    {
        if (!map || !layer) return 0;
        const float tw = map->worldTileW();
        const float th = map->worldTileH();
        if (tw <= 0.0f || th <= 0.0f) return 0;
        const int tx = static_cast<int>(std::floor(x / tw));
        const int ty = static_cast<int>(std::floor(y / th));
        const int id = layer->at(tx, ty);
        if (id == 0) return 0;
        return map->tilesets[layer->tilesetIndex].isSolid(id) ? id : 0;
    }
```

Put it in the same anonymous scope near the existing `pointInSolid`
member (or replace `pointInSolid` — it's now trivially derivable as
`solidTileIdAt(...) != 0`).

**Change 2** — inside `BulletManager::update`, when a bullet dies on a
tile, push an `Impact` before marking it dead:

```cpp
    for (auto& b : m_bullets) {
        if (!b.alive) continue;
        b.x += b.vx * dt;

        // Tile hit → record where and which tile, then kill the bullet.
        if (const int id = solidTileIdAt(map, layer, b.x, b.y); id != 0) {
            m_impacts.push_back(Impact{ b.x, b.y, id });
            b.alive = false;
            continue;
        }

        // Off-screen despawn — no impact event.
        if (b.x < minX || b.x > maxX || b.y < minY || b.y > maxY) {
            b.alive = false;
        }
    }
```

(If you kept the original `pointInSolid`, adjust it to call
`solidTileIdAt(...) != 0` internally, then in `update` call
`solidTileIdAt` directly so you have the ID. Either way, we want the ID
right where the bullet dies so we don't have to re-sample after the fact.)

**Change 3** — `clear()` should drain the impact queue too, otherwise a
scene reload could carry stale events:

```cpp
        void clear() { m_bullets.clear(); m_impacts.clear(); }
```

(Update the inline `clear` in `Bullet.h`.)

---

## 3. Sanity: what these hooks *do not* change

- Animation state machine — untouched.
- Bullet flight, cull margins, colour — untouched.
- Player physics box, gravity, jump — untouched.
- Ghost mode — untouched (shooting is already blocked in Ghost, so no
  impact / muzzle / casing effects can fire there either).

All the new fields are pure reads except `m_landedThisFrame` which is
cleared once per update. If none of the getters is called, the
runtime cost is a couple of assignments per frame.

---

Next: `03_gamelayer_and_project.md` — wire `Effects` into `GameLayer`
and register the two new files in `MegaX.vcxproj`.
