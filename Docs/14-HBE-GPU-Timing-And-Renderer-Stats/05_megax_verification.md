# 05 — MegaX verification hooks

Item 14 is engine work. This doc's edits live in the MegaX
game project and exist only to **verify** the engine changes
by exercising:

* `Profiler::PublishParticleStats(...)`
* `Profiler::PublishLightStats(...)` (0/0 placeholder — no lighting yet)
* an F8 hotkey that dumps a combined CPU + GPU + renderer
  snapshot to the log — the "one combined performance
  snapshot" required by Item 14.

All edits are in **one file**:

* `G:\Dev\HBE\MegaX\src\Game\GameLayer.cpp`

No new files. No header edits. No vcxproj edits.

If you skip this doc, Item 14's engine behavior still works
in any game — but Item 14's done-criteria in workitems.txt
requires you to see a combined snapshot in MegaX, so please
do the F8 hotkey.

---

## 1. Fix the `Comabt` typo

While we're in this file, fix the typo from Item 13.

**Line 190** (search for `HBE_PROFILE_SCOPE("Comabt")`):

```cpp
            HBE_PROFILE_SCOPE("Comabt");
```

Change to:

```cpp
            HBE_PROFILE_SCOPE("Combat");
```

This is a display-only rename. No other code refers to the
string.

---

## 2. Publish live particles + light placeholder every frame

Find the `Particles` scope block near the end of `onUpdate`
(around line 251):

```cpp
        {
            HBE_PROFILE_SCOPE("Particles");
            m_effects.update(dt);
        }
	}
```

Replace **the whole block plus the closing `}` of `onUpdate`**
with:

```cpp
        {
            HBE_PROFILE_SCOPE("Particles");
            m_effects.update(dt);
        }

        // -------- Item 14: publish game-side stats to the profiler --------
        HBE::Core::Profiler::PublishParticleStats(m_effects.liveParticles());
        HBE::Core::Profiler::PublishLightStats(0, 0); // MegaX has no lighting yet.
	}
```

`m_effects.liveParticles()` already exists (see
`Effects.h:56` / `Effects.cpp:718`).

These calls run BEFORE `PublishRendererStats` (which fires
from `Application::run` in doc 03 §4.4), so the merge logic
in `PublishRendererStats` (doc 04 §2.6) preserves these
values.

---

## 3. Add the F8 combined snapshot hotkey

Find the F7 hotkey block:

```cpp
        if (Input::IsKeyPressed(SDL_SCANCODE_F7)) {
            LogInfo("MegaX: soft respawn requested (F7).");
            reloadScene(false);
        }
```

Immediately AFTER it, insert an F8 block:

```cpp
        if (Input::IsKeyPressed(SDL_SCANCODE_F8)) {
            const auto snap = HBE::Core::Profiler::GetSnapshot();
            LogInfo("========== [Profiler F8 Snapshot] ==========");
            LogInfo("CPU frame: {:.2f} ms (avg {:.2f}, min {:.2f}, max {:.2f}, samples {})",
                snap.frameMs, snap.frameAvgMs, snap.frameMinMs, snap.frameMaxMs,
                static_cast<unsigned long long>(snap.frameSampleCount));
            if (snap.gpu.supported) {
                LogInfo("GPU frame: {:.2f} ms (avg {:.2f}, min {:.2f}, max {:.2f}, samples {})",
                    snap.gpu.frameMs, snap.gpu.frameAvgMs, snap.gpu.frameMinMs,
                    snap.gpu.frameMaxMs,
                    static_cast<unsigned long long>(snap.gpu.frameSampleCount));
            } else {
                LogInfo("GPU frame: <unsupported / disabled>");
            }
            LogInfo("Renderer: drawCalls={} passes={} submitQ={} renderQ={} culled={}"
                    " matChg={} texChg={} tileChunks={} ppPasses={}",
                snap.renderer.drawCalls, snap.renderer.passes,
                snap.renderer.submittedQuads, snap.renderer.renderedQuads,
                snap.renderer.culledSprites,
                snap.renderer.materialChanges, snap.renderer.textureChanges,
                snap.renderer.visibleTileChunks, snap.renderer.postProcessPasses);
            LogInfo("Scene: liveParticles={} activeLights={} shadowLights={}",
                snap.renderer.liveParticles,
                snap.renderer.activeLights, snap.renderer.shadowCastingLights);
            LogInfo("CPU sections ({}):", static_cast<unsigned long long>(snap.sections.size()));
            for (const auto& s : snap.sections) {
                LogInfo("  {:<20} cur {:>7.3f} ms  avg {:>7.3f}  min {:>7.3f}  max {:>7.3f}",
                    s.name, s.currentMs, s.avgMs, s.minMs, s.maxMs);
            }
            LogInfo("GPU sections ({}):", static_cast<unsigned long long>(snap.gpu.sections.size()));
            for (const auto& g : snap.gpu.sections) {
                LogInfo("  {:<20} cur {:>7.3f} ms  avg {:>7.3f}  min {:>7.3f}  max {:>7.3f}",
                    g.name, g.currentMs, g.avgMs, g.minMs, g.maxMs);
            }
            LogInfo("============================================");
        }
```

This gives you a **single deterministic** combined dump when
you press F8. It's the concrete artifact you can screenshot
for the "combined performance snapshot" acceptance.

**Assumption:** `HBE::Core::Log::LogInfo` supports fmt-style
formatting with `{}` placeholders. That matches every
`LogInfo("...{}...", ...)` call already present in this file
and in `Application.cpp`. If your `LogInfo` is printf-style
instead, swap `{}` for `%s`/`%d`/`%.2f` — the Item 13 dump
in `Application.cpp` uses the same format tokens as above,
so if that compiled, this will too.

---

## 4. Sanity check

```powershell
Select-String -Path G:\Dev\HBE\MegaX\src\Game\GameLayer.cpp -Pattern "PublishParticleStats|PublishLightStats|SDL_SCANCODE_F8|Profiler F8 Snapshot|Combat"
# -> expect 5 matches:
#   PublishParticleStats
#   PublishLightStats
#   SDL_SCANCODE_F8
#   "========== [Profiler F8 Snapshot] =========="
#   HBE_PROFILE_SCOPE("Combat")

Select-String -Path G:\Dev\HBE\MegaX\src\Game\GameLayer.cpp -Pattern "Comabt"
# -> expect 0 matches (typo fixed)
```

Next: `06_build_run_and_verify.md`.
