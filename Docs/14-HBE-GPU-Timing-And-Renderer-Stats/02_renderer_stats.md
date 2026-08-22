# 02 — Extended renderer statistics

This doc modifies four existing files in `HBE.Renderer.GL/`:

* `Renderer2D.h` / `Renderer2D.cpp`
* `SpriteBatch2D.h` / `SpriteBatch2D.cpp`
* `TileMapRenderer.h` / `TileMapRenderer.cpp`
* `PostProcessStack.h` / `PostProcessStack.cpp`

No new files, no vcxproj edits. Every change is additive —
existing behavior is unchanged.

At the end of this doc, four renderer subsystems expose
per-frame stats that the Application layer (doc `03` §4)
will read and publish to the Profiler.

---

## 1. `Renderer2D.h` — extend `Renderer2DStats` + add `resetFrameStats()`

Open `G:\Dev\HBE\HBE.Renderer.GL\include\HBE\Renderer\Renderer2D.h`.

### 1.1  Replace the `Renderer2DStats` struct

Find (currently lines 21-26):

```cpp
		struct Renderer2DStats {
			int drawCalls = 0;
			int quads = 0;
			int stateChanges = 0;
			int passes = 0;
		};
```

Replace with:

```cpp
		struct Renderer2DStats {
			int drawCalls        = 0;
			int quads            = 0;     // legacy alias for submittedQuads
			int stateChanges     = 0;     // legacy alias for materialChanges + textureChanges
			int passes           = 0;
			int submittedQuads   = 0;
			int renderedQuads    = 0;
			int culledSprites    = 0;     // game populates via a setter, defaults 0
			int materialChanges  = 0;
			int textureChanges   = 0;
		};
```

Keep `quads` and `stateChanges` for backward compatibility.
`quads == submittedQuads` and
`stateChanges == materialChanges + textureChanges` after
Item 14. Any existing caller (nothing does today) still gets
valid data.

### 1.2  Add `resetFrameStats()` + game-side setter

Below the `getStats() const;` declaration (currently line 28),
add:

```cpp
		Renderer2DStats getStats() const;

		// Zero every frame counter. Called by Application::run at the top of the
		// frame *before* any beginScene() so per-frame stats mean per-frame.
		void resetFrameStats();

		// Game hook — MegaX (or any game with sprite frustum culling) publishes
		// the count of sprites it decided to skip this frame.
		void addCulledSprites(int n);
```

The complete public block should now read (rough shape):

```cpp
		explicit Renderer2D(GLRenderer& backend);
		~Renderer2D();

		const Camera2D* activeCamera() const { return m_activeCamera; }

		struct Renderer2DStats { ... };

		Renderer2DStats getStats() const;
		void resetFrameStats();
		void addCulledSprites(int n);

		void setSpriteQuadMesh(const Mesh* quadMesh);

		void beginScene(const Camera2D& camera);
		void beginScene(const Camera2D& camera, RenderPass pass);
		void endScene();

		void draw(const RenderItem& item);
		void drawDirect(const RenderItem& item);
```

### 1.3  Add new private members

In the `private:` section, right below the existing frame
counters:

```cpp
		int m_frameDrawCalls = 0;
		int m_frameQuads = 0;
		int m_frameStateChanges = 0;
		int m_framePasses = 0;
```

Add:

```cpp
		int m_frameSubmittedQuads = 0;
		int m_frameRenderedQuads  = 0;
		int m_frameCulledSprites  = 0;
		int m_frameMaterialChanges = 0;
		int m_frameTextureChanges  = 0;
```

That's the full extent of the header edit.

---

## 2. `Renderer2D.cpp` — implement `resetFrameStats`, `addCulledSprites`, populate new fields

Open `G:\Dev\HBE\HBE.Renderer.GL\src\Renderer\Renderer2D.cpp`.

### 2.1  Update `endScene()`

Currently (approx. lines 47-61):

```cpp
	void Renderer2D::endScene() {
		if (!m_activeCamera) return;

		if (m_batch) {
			float vp[16];
			m_backend.getViewProjection(vp);
			m_batch->flush(vp);

			m_frameDrawCalls += m_batch->drawCalls();
			m_frameQuads += m_batch->quadCount();
			m_frameStateChanges += m_batch->stateChanges();
		}

		++m_framePasses;
		m_activeCamera = nullptr;
	}
```

Replace the `if (m_batch) { ... }` body with:

```cpp
		if (m_batch) {
			float vp[16];
			m_backend.getViewProjection(vp);
			m_batch->flush(vp);

			m_frameDrawCalls        += m_batch->drawCalls();
			m_frameQuads            += m_batch->quadCount();
			m_frameSubmittedQuads   += m_batch->quadCount();
			m_frameRenderedQuads    += m_batch->quadCount();
			m_frameStateChanges     += m_batch->stateChanges();
			m_frameMaterialChanges  += m_batch->materialChanges();
			m_frameTextureChanges   += m_batch->textureChanges();
		}
```

`materialChanges()` and `textureChanges()` are added to
`SpriteBatch2D` in §3.

### 2.2  Update `getStats()`

Currently:

```cpp
	Renderer2D::Renderer2DStats Renderer2D::getStats() const {
		Renderer2DStats s{};
		s.drawCalls = m_frameDrawCalls;
		s.quads = m_frameQuads;
		s.stateChanges = m_frameStateChanges;
		s.passes = m_framePasses;
		return s;
	}
```

Replace with:

```cpp
	Renderer2D::Renderer2DStats Renderer2D::getStats() const {
		Renderer2DStats s{};
		s.drawCalls        = m_frameDrawCalls;
		s.quads            = m_frameQuads;
		s.stateChanges     = m_frameStateChanges;
		s.passes           = m_framePasses;
		s.submittedQuads   = m_frameSubmittedQuads;
		s.renderedQuads    = m_frameRenderedQuads;
		s.culledSprites    = m_frameCulledSprites;
		s.materialChanges  = m_frameMaterialChanges;
		s.textureChanges   = m_frameTextureChanges;
		return s;
	}
```

### 2.3  Add `resetFrameStats()` + `addCulledSprites()`

At the very bottom of the file, just before the closing
`}` of `namespace HBE::Renderer`, add:

```cpp
	void Renderer2D::resetFrameStats() {
		m_frameDrawCalls        = 0;
		m_frameQuads            = 0;
		m_frameStateChanges     = 0;
		m_framePasses           = 0;
		m_frameSubmittedQuads   = 0;
		m_frameRenderedQuads    = 0;
		m_frameCulledSprites    = 0;
		m_frameMaterialChanges  = 0;
		m_frameTextureChanges   = 0;
	}

	void Renderer2D::addCulledSprites(int n) {
		if (n > 0) m_frameCulledSprites += n;
	}
```

---

## 3. `SpriteBatch2D.h` — expose material/texture change counts separately

Open `G:\Dev\HBE\HBE.Renderer.GL\include\HBE\Renderer\SpriteBatch2D.h`.

### 3.1  Add accessors

The existing accessor block (currently lines 31-33):

```cpp
        int drawCalls()      const { return m_drawCalls; }
        int quadCount()      const { return m_quadsSubmitted; }
        int stateChanges()   const { return m_stateChanges; } // NEW
```

Extend to:

```cpp
        int drawCalls()      const { return m_drawCalls; }
        int quadCount()      const { return m_quadsSubmitted; }
        int stateChanges()   const { return m_stateChanges; }
        int materialChanges() const { return m_materialChanges; }
        int textureChanges()  const { return m_textureChanges; }
```

### 3.2  Add member variables

In the `private:` section (currently lines 73-75):

```cpp
        int           m_drawCalls = 0;
        int           m_quadsSubmitted = 0;
        int           m_stateChanges = 0;
```

Extend to:

```cpp
        int           m_drawCalls = 0;
        int           m_quadsSubmitted = 0;
        int           m_stateChanges = 0;
        int           m_materialChanges = 0;
        int           m_textureChanges  = 0;
```

---

## 4. `SpriteBatch2D.cpp` — count material vs. texture changes

Open `G:\Dev\HBE\HBE.Renderer.GL\src\Renderer\SpriteBatch2D.cpp`.

### 4.1  Reset the new counters on `begin()`

Find (currently lines ~18-24 in the `begin()` implementation):

```cpp
		m_drawCalls = 0;
		...
		m_stateChanges = 0;
```

Add right after those lines:

```cpp
		m_materialChanges = 0;
		m_textureChanges  = 0;
```

### 4.2  Increment the correct counter in `applyStateDiff`

Find the current `applyStateDiff` implementation (starts at
line 304). Locate the four "changed = true" branches inside
it:

* shader change (line ~309)
* blend change (line ~322)
* texture change (line ~341)
* uniform change (line ~351)

Right after each `changed = true;` statement inside these
four branches, add an increment of the appropriate counter:

**Shader change** (~line 313):

```cpp
		if (mat->shader != m_lastShader) {
			mat->shader->use();
			m_lastShader = mat->shader;
			changed = true;
			++m_materialChanges;   // NEW
		}
```

**Blend change** (~line 336):

```cpp
			m_lastBlend = mat->blend;
			changed = true;
			++m_materialChanges;   // NEW
		}
```

**Texture change** (~line 348):

```cpp
			m_lastTexture = mat->texture;
			changed = true;
			++m_textureChanges;    // NEW
		}
```

**Uniform change** (~line 362):

```cpp
			int sdfSoftLoc = mat->shader->getUniformLocation("uSDFSoftness");
			if (sdfSoftLoc >= 0) glUniform1f(sdfSoftLoc, mat->sdfSoftness);
			changed = true;
			++m_materialChanges;   // NEW
		}
```

The existing `if (changed) ++m_stateChanges;` at the bottom
of `applyStateDiff` stays — it's the aggregate. After this
edit:

```
stateChanges == materialChanges + textureChanges  (approximately)
```

The "approximately" is because a single `applyStateDiff` call
that swaps both a texture AND a material only increments
`m_stateChanges` once but bumps `m_materialChanges` and
`m_textureChanges` each. That's the intended semantics —
`stateChanges` counts "logical" state flips, the split
counters count "physical" bind operations.

---

## 5. `TileMapRenderer.h` — expose `visibleTileChunks()`

Open `G:\Dev\HBE\HBE.Renderer.GL\include\HBE\Renderer\TileMapRenderer.h`.

Right after the `void draw(...)` declaration (currently
line 19), add:

```cpp
		int  visibleTileChunks() const { return m_visibleChunks; }
		void resetFrameStats() { m_visibleChunks = 0; }
```

In the `private:` section, right after the existing member
list, add:

```cpp
		int m_visibleChunks = 0;
```

---

## 6. `TileMapRenderer.cpp` — count drawn layers

Open `G:\Dev\HBE\HBE.Renderer.GL\src\Renderer\TileMapRenderer.cpp`.

Inside `draw(Renderer2D& r2d, const TileMap& map)`, find the
`for (const auto& layer : map.layers) { ... }` loop. Its
current body (line ~114-159) is a tile-drawing loop that
either draws or `continue`s.

Just before the closing `}` of the outer for-loop body (i.e.
after the nested `for (int y ...)` block, still inside the
outer for), add:

```cpp
            // If the culled AABB was non-empty, this layer contributed to a "chunk".
            ++m_visibleChunks;
```

More precisely — insert this immediately after the nested
`for (int y = y0; y <= y1; ++y) { ... }` block terminates
but before the outer `}`. That's the last line of the outer
for-loop's body.

**Concrete placement:** the current outer for-loop ends with:

```cpp
            for (int y = y0; y <= y1; ++y) {
                for (int x = x0; x <= x1; ++x) {
                    ...
                }
            }
        }   // <-- outer for closes here
```

Change to:

```cpp
            for (int y = y0; y <= y1; ++y) {
                for (int x = x0; x <= x1; ++x) {
                    ...
                }
            }
            ++m_visibleChunks;
        }
```

**Do NOT** reset `m_visibleChunks` inside `draw()`. Reset
comes from `Application::run` via `resetFrameStats()` (doc `03`
§4).

---

## 7. `PostProcessStack.h` — expose `lastPassCount()`

Open `G:\Dev\HBE\HBE.Renderer.GL\include\HBE\Renderer\PostProcessStack.h`.

Right after the `bool isInitialized() const { return m_initialized; }`
line (currently line 42), add:

```cpp
		int lastPassCount() const { return m_lastPassCount; }
```

In the `private:` section (currently around line 45-55), add
right after `bool m_initialized = false;`:

```cpp
		int m_lastPassCount = 0;
```

---

## 8. `PostProcessStack.cpp` — set `m_lastPassCount` inside `present`

Open `G:\Dev\HBE\HBE.Renderer.GL\src\Renderer\PostProcessStack.cpp`.

In `present(int vpX, int vpY, int vpW, int vpH)` (currently
starts line 65), the first work done is:

```cpp
		if (!m_initialized) return;
		int enabledCount = 0;
		for (const auto& fx : m_effects) {
			if (fx.enabled && fx.shader) ++enabledCount;
		}
```

Right after the `for` loop that computes `enabledCount`,
add:

```cpp
		m_lastPassCount = enabledCount;
```

That's the only edit — one line.

---

## 9. Sanity check

```powershell
Select-String -Path G:\Dev\HBE\HBE.Renderer.GL\include\HBE\Renderer\Renderer2D.h -Pattern "submittedQuads|renderedQuads|culledSprites|materialChanges|textureChanges|resetFrameStats|addCulledSprites"
# -> expect 7 matches (one per name)

Select-String -Path G:\Dev\HBE\HBE.Renderer.GL\include\HBE\Renderer\SpriteBatch2D.h -Pattern "materialChanges|textureChanges"
# -> expect 4 matches (2 accessors + 2 members)

Select-String -Path G:\Dev\HBE\HBE.Renderer.GL\include\HBE\Renderer\TileMapRenderer.h -Pattern "visibleTileChunks|m_visibleChunks|resetFrameStats"
# -> expect 3 matches

Select-String -Path G:\Dev\HBE\HBE.Renderer.GL\include\HBE\Renderer\PostProcessStack.h -Pattern "lastPassCount|m_lastPassCount"
# -> expect 2 matches
```

Do not build yet. Doc `03` wires the new stats into the
frame loop; doc `04` publishes them to the Profiler. A
full-solution build in doc `06` proves it all links.

Next: `03_glrenderer_frame_scope.md`.
