# Doc 05 — Build, Run, and Verify

Final step: build MegaX, run it, and confirm item 00's success criteria — a
blank window titled **MegaX** with a working **F11** fullscreen toggle.

---

## Step 1 — Set MegaX as the startup project

In Solution Explorer, right-click **MegaX ▸ Set as Startup Project**. (Its name
goes **bold**.) This makes **F5** launch MegaX, not the Sandbox.

---

## Step 2 — Build

1. Set the config bar to **Debug | x64**.
2. **Build ▸ Build MegaX** (or `Ctrl+B`).

Expected: the three engine projects build first (via the project references),
then MegaX links, and you see in the Output window:

```
MegaX: copying runtime deps to G:\Dev\HBE\MegaX\bin\x64\Debug\
========== Build: 4 succeeded, 0 failed ==========
```

Confirm `G:\Dev\HBE\MegaX\bin\x64\Debug\` now contains:

```
MegaX.exe
SDL3.dll
SDL3_mixer.dll
assets\shaders\...    (the 8 shaders)
```

---

## Step 3 — Run

Press **F5** (Debug) or **Ctrl+F5** (Run without debugging).

You should see:
- A **1280×720 window titled "MegaX"** with a solid (cleared) background — no
  crash, no error dialog.
- A console window with engine log lines ending around:
  ```
  [INFO]AssetPaths: asset root    = G:\Dev\HBE\MegaX\bin\x64\Debug\assets
  [INFO]Audio initialized (SDL3_mixer MIX_* API).
  [INFO]Application initialized.
  [INFO]MegaX GameLayer attached.
  ```

---

## Step 4 — Verify the success criteria

- [ ] **Blank window opens** titled "MegaX" and stays open (no crash).
- [ ] **F11 toggles fullscreen** — press it: the window goes borderless
      fullscreen at desktop resolution; press again: back to windowed. (This is
      the engine's global handler; you wrote no code for it.)
- [ ] **Resize** the window — the cleared area tracks the new size (letterbox
      viewport recomputes); no crash.
- [ ] **Close** the window (X) or press its close — the app exits cleanly and
      the console prints `Application exiting run loop.`
- [ ] Build output confirms `MegaX.exe`, `SDL3.dll`, `SDL3_mixer.dll`, and
      `assets\` are all in `bin\x64\Debug\`.
- [ ] (Optional) Switch to **Release | x64**, rebuild, and re-run — same result.

If all boxes are checked, **item 00 is complete.**

---

## Troubleshooting

**`app.initialize failed` + log `AssetPaths::Initialize: could not locate an
'assets' folder`.**
`assets\` didn't end up next to the exe. Check: (a) the `MegaXCopyRuntime`
target ran (Output window), (b) `assets\` is non-empty (the 8 shaders from doc
03), (c) Debugger Working Directory = `$(TargetDir)` (doc 02, Step 5).

**Linker: `cannot open file 'HBE.Core.lib'` (LNK1104).**
The engine libs aren't where MegaX looks. Confirm the three **project
references** (doc 02, Step 1) so the engine builds first, and that
`$(SolutionDir)x64\$(Configuration)\` is in **Additional Library Directories**.

**Linker: unresolved externals referencing `SDL3_*` or `glad`/GL symbols.**
Missing a dependency. Recheck **Additional Dependencies** (doc 02, Step 3):
`HBE.Core.lib;HBE.Platform.SDL.lib;HBE.Renderer.GL.lib;SDL3.lib;SDL3_mixer.lib;OpenGL32.lib`.

**Compiler: `cannot open source file "HBE/Core/Application.h"`.**
Include paths wrong. Recheck **Additional Include Directories** (doc 02, Step 2)
— the three engine `include\` folders must be present.

**Runtime: `SDL3.dll not found` dialog.**
The DLL copy didn't run or you launched the exe from the wrong folder. Ensure
`MegaXCopyRuntime` ran and you're running from `bin\x64\<Config>\` (F5 uses the
Working Directory `$(TargetDir)`).

**Error `LNK2038: mismatch detected for '_MSC_VER'` / toolset mismatch.**
MegaX's **Platform Toolset** isn't `v145`. Set it to match the engine (doc 01,
Step 3).

**Window opens then immediately closes.**
Check the console log for the first `[ERROR]`/`[FATAL]` line — usually an asset
or GL-context failure. `useOpenGL` must be `true` (doc 04).

**F11 does nothing.**
That's engine-global, so it should "just work." Make sure the game window has
focus and you're pressing **F11** (not F1). If a later item rebinds keys, don't
consume F11 in the game layer.

---

## What's next

Item 00 done → the engine is wired in and a blank MegaX window runs. Proceed to
**WorkItems item 1 — Player Walk / Idle with Camera Lerp** (the "Ghost State"),
which is the first item that actually draws into the window using the engine's
sprite renderer and the shaders you seeded in doc 03.
