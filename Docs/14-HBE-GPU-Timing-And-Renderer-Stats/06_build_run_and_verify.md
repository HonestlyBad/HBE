# 06 — Build, run and verify

Final doc for Item 14. After all previous edits (docs 01-05)
are in place, do these steps in order.

---

## 1. Project file sanity checks

Run these `Select-String` probes. Each is a "did I miss a
step?" checkpoint.

```powershell
# GpuTimer files exist and are in the vcxproj (doc 01)
Get-ChildItem G:\Dev\HBE\HBE.Renderer.GL\include\HBE\Renderer\GpuTimer.h
Get-ChildItem G:\Dev\HBE\HBE.Renderer.GL\src\Renderer\GpuTimer.cpp
Select-String -Path G:\Dev\HBE\HBE.Renderer.GL\HBE.Renderer.GL.vcxproj -Pattern "GpuTimer"
# -> expect 2 matches (one <ClInclude>, one <ClCompile>)

# Renderer2DStats extended (doc 02 §1)
Select-String -Path G:\Dev\HBE\HBE.Renderer.GL\include\HBE\Renderer\Renderer2D.h -Pattern "submittedQuads|renderedQuads|culledSprites|materialChanges|textureChanges"
# -> expect 5 matches

# Profiler extended (doc 04)
Select-String -Path G:\Dev\HBE\HBE.Core\include\HBE\Core\Profiler.h -Pattern "PublishGpuSection|PublishRendererStats|SetGpuSupported|GpuTimings|RendererStats"
# -> expect 5+ matches

# GLRenderer wired (doc 03)
Select-String -Path G:\Dev\HBE\HBE.Renderer.GL\src\Renderer\GLRenderer.cpp -Pattern "GpuTimer::|s_gpuSceneHandle|GpuScene|GpuPostProcess"
# -> expect 4+ matches

# Application wired (doc 03 §4)
Select-String -Path G:\Dev\HBE\HBE.Core\src\Core\Application.cpp -Pattern "GpuTimer::NewFrame|resetFrameStats|PublishRendererStats|GpuTimer::Shutdown"
# -> expect 4 matches

# MegaX wired (doc 05)
Select-String -Path G:\Dev\HBE\MegaX\src\Game\GameLayer.cpp -Pattern "PublishParticleStats|PublishLightStats|SDL_SCANCODE_F8|HBE_PROFILE_SCOPE\(""Combat""\)"
# -> expect 4 matches; and 0 matches for "Comabt"
```

Any zero result → jump back to the doc that owned that
feature. Don't try to build until all probes pass.

---

## 2. Incremental builds (fail fast)

Build one target at a time so error scope stays small.

Enter a VS shell:

```powershell
& $env:ComSpec /k '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"'
```

Then, from that shell:

```
cd /d G:\Dev\HBE

msbuild HonestlyBadEngine.slnx /p:Configuration=Debug /p:Platform=x64 /t:HBE_Core /m /nologo /v:m
```

Expected: **HBE_Core builds clean.** If not, most likely
you missed a piece of doc 04 (Profiler additions).

```
msbuild HonestlyBadEngine.slnx /p:Configuration=Debug /p:Platform=x64 /t:HBE_Renderer_GL /m /nologo /v:m
```

Expected: **HBE_Renderer_GL builds clean.** If not, look at
doc 01 (GpuTimer), 02 (renderer stats), or 03 (GLRenderer wiring).

```
msbuild HonestlyBadEngine.slnx /p:Configuration=Debug /p:Platform=x64 /t:MegaX /m /nologo /v:m
```

Expected: **MegaX builds clean.** If not, look at doc 05
(GameLayer.cpp edits).

The full solution build:

```
msbuild HonestlyBadEngine.slnx /p:Configuration=Debug /p:Platform=x64 /m /nologo /v:m
```

should now also build clean end-to-end (Sandbox / MapMaker
build too, and see nothing but the enhanced `Renderer2DStats`
— they're neutral consumers).

Ignore the `'vswhere.exe' is not recognized` warning; it's
harmless.

---

## 3. Run MegaX and inspect the log

```
G:\Dev\HBE\bin\Debug-x86_64\MegaX\MegaX.exe
```

*(exact path may vary — use whichever output dir your
solution emits MegaX.exe into)*.

Once in-game, observe the ~1 Hz `[Profiler]` log block.
Compared to Item 13's version, it now includes:

* A `GPU frame: X.XX ms` line (or `GPU: unsupported`).
* Extra renderer counters: `submitQ`, `renderQ`, `culled`,
  `matChg`, `texChg`, `tileChunks`, `ppPasses`.
* A `Scene:` line with `liveParticles`.
* An `GPU sections:` block after the CPU sections block,
  listing `GpuScene` and (if a post-process stack is
  active) `GpuPostProcess`.

Then press **F8**. You should get exactly one big
`========== [Profiler F8 Snapshot] ==========` block — the
combined CPU + GPU + renderer dump from doc 05.

---

## 4. Verify checklist

Group A — regression (nothing from Items 12/13 broke):

* [ ] Game still launches, plays, reloads on F5.
* [ ] Ghost mode (G) still toggles.
* [ ] Item 13 log block still appears every ~1 s with
      identical CPU-side content.
* [ ] Frame times still `~2.9-3.5 ms` in Debug (no
      regression from GPU timer overhead).

Group B — GPU timing works:

* [ ] Log block shows `GPU frame: N.NN ms`, not `unsupported`
      *(on any modern GL 3.3+ desktop GPU)*.
* [ ] `GpuScene` appears in the GPU sections list with a
      non-zero `cur`/`avg`.
* [ ] `GpuScene` avg is in a sane range (typically
      **0.10 – 2.0 ms** for MegaX in Debug on desktop).
* [ ] No `GL_INVALID_OPERATION` in the log around
      `glBeginQuery` / `glEndQuery`.
* [ ] No spam of `TIME_ELAPSED` nesting errors.

Group C — renderer stats are sane:

* [ ] `drawCalls > 0` and typically < 20 for MegaX.
* [ ] `submitQ >= renderQ`.
* [ ] `renderQ + culled <= submitQ` (Wait — culled is a
      subset of submit, not adding on. Actually verify:
      `renderQ + culled` should be roughly `submitQ`. See
      §5 troubleshooting if this is wildly off.)
* [ ] `matChg <= texChg` (materials are coarser than textures).
* [ ] `tileChunks > 0` when the map is loaded.
* [ ] `ppPasses` matches the number of enabled post-process
      passes in your stack (0 if none active).
* [ ] `liveParticles` moves as you fire / take damage /
      run (it flows through the muzzle-flash / dust systems).

Group D — fallback works:

* [ ] Set env var `HBE_FORCE_NO_GPU_TIMER=1` and launch:
      ```powershell
      $env:HBE_FORCE_NO_GPU_TIMER="1"
      G:\Dev\HBE\bin\Debug-x86_64\MegaX\MegaX.exe
      ```
* [ ] Log says `GPU frame: <unsupported / disabled>`.
* [ ] `GpuScene` / `GpuPostProcess` absent from GPU sections.
* [ ] CPU section timings unchanged (regression check).
* [ ] Then unset it:
      ```powershell
      Remove-Item Env:\HBE_FORCE_NO_GPU_TIMER
      ```

Group E — latency correctness:

* [ ] After booting, the very first frames log `GPU frame: 0.00 ms`
      or omit the sample (that's expected — the query result
      is 3 frames behind).
* [ ] Within ~5 frames the number stabilizes.
* [ ] No `[GpuTimer] WARN result unavailable` spam continuously
      (a few at start is fine; ongoing means the ring is too small).

Group F — F8 combined snapshot:

* [ ] Pressing F8 produces exactly ONE
      `========== [Profiler F8 Snapshot] ==========` block.
* [ ] The block contains a CPU section line, a GPU section
      line, and a renderer line.
* [ ] Pressing F8 repeatedly doesn't spam or crash.

---

## 5. Troubleshooting matrix

| Symptom | Likely cause | Fix |
| --- | --- | --- |
| `error C2039: 'gpu': is not a member of 'Snapshot'` | Doc 04 §1.2 skipped | Add `GpuTimings gpu;` + `RendererStats renderer;` to `Snapshot` |
| `error LNK2019: unresolved external symbol ... PublishGpuSection` | Doc 04 §2.6 skipped | Add publish function definitions to `Profiler.cpp` |
| `error LNK2019: unresolved external symbol ... GpuTimer::NewFrame` | Doc 01 vcxproj step skipped | Add `<ClCompile Include="src\Renderer\GpuTimer.cpp"/>` to `HBE.Renderer.GL.vcxproj` |
| `error C2039: 'visibleTileChunks': is not a member of 'TileMapRenderer'` | Doc 02 §5 skipped | Add member + getter |
| Game runs but `GPU frame: <unsupported>` on a good GPU | Extension probe rejected the driver | Check GL vendor in log; also confirm `HBE_FORCE_NO_GPU_TIMER` is not set |
| `GL_INVALID_OPERATION` on `glBeginQuery` | Two `GL_TIME_ELAPSED` queries active at once — you nested scopes | GPU scopes must be serial siblings; check doc 03 §3 that `GpuScene` closes before `GpuPostProcess` opens |
| `WARN result unavailable` every frame | Ring too small; or previous frame's query was orphaned | Confirm `kResultLatencyFrames = 3` in `GpuTimer.cpp`; verify `NewFrame()` runs exactly once per Application frame |
| `renderQ + culled` far off from `submitQ` | Cull counter incremented BEFORE `submittedQuads` in the batch path | Confirm doc 02 §2 order: submit++ first, then either cull++ (skip) or continue to render |
| MegaX log spams `Combat` scope errors | You changed the scope name in `HBE_PROFILE_SCOPE("Combat")` but forgot to remove `Comabt` earlier | Ensure only one scope named `Combat`, not both |
| F8 produces two snapshot blocks | You pasted the F8 block inside a repeating scope like the update loop's `Physics` block by mistake | Verify F8 is a top-level check inside `onUpdate`, not nested inside another `HBE_PROFILE_SCOPE` |

---

## 6. Done criteria (from `workitems.txt` Item 14)

* [x] GPU timer queries wired around the scene render and
      post-process passes.
* [x] `Renderer2DStats` extended with the requested counters.
* [x] `Profiler::Snapshot` carries GPU timings + renderer stats.
* [x] 1 Hz log block includes GPU + expanded renderer info.
* [x] A single hotkey (F8) produces a combined snapshot.
* [x] Runtime fallback (`HBE_FORCE_NO_GPU_TIMER`) works.
* [x] All engine work is generic — Sandbox / MapMaker not touched.
* [x] Verification game (MegaX) exercises the new APIs.

If every checkbox in §4 clears and the log shows the
expected shape, Item 14 is complete. Move on to Item 15.
