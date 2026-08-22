# Doc 03 — Add files to the project, build, run, verify

Two new files (`Player.h`, `Player.cpp`) must be added to the project, then it's
a normal build/run. The `player.png` / `player-no-helm.png` assets are already
in place and the copy step from item 00 mirrors `assets\` next to the exe.

---

## Step 1 — Add `Player.h` / `Player.cpp` to the project

### Option A — Visual Studio (recommended)

1. **Solution Explorer** → right-click the **MegaX** project.
2. **Add ▸ Existing Item…**, select `include\Game\Player.h`, **Add**.
3. Repeat for `src\Game\Player.cpp`.

VS drops them into the right `ItemGroup`s automatically.

### Option B — Edit `MegaX.vcxproj` by hand

Add the two lines shown, next to the existing `GameLayer` entries:

```xml
  <ItemGroup>
    <ClInclude Include="include\Game\GameLayer.h" />
    <ClInclude Include="include\Game\Player.h" />        <!-- ADD -->
  </ItemGroup>
  <ItemGroup>
    <ClCompile Include="src\Game\GameLayer.cpp" />
    <ClCompile Include="src\Game\Player.cpp" />           <!-- ADD -->
    <ClCompile Include="src\main.cpp" />
  </ItemGroup>
```

> If VS is open, it will prompt to **Reload** the project — accept.

---

## Step 2 — Build

1. Set configuration to **Debug | x64** (or Release | x64).
2. Make sure **MegaX** is the **startup project** (bold in Solution Explorer;
   right-click ▸ **Set as Startup Project** if not).
3. **Build ▸ Build MegaX** (Ctrl+B).

Expect **0 errors**. The post-build `MegaXCopyRuntime` step copies `SDL3.dll`,
`SDL3_mixer.dll`, and everything under `assets\` next to
`MegaX\bin\x64\Debug\MegaX.exe`.

> Building from the command line instead:
> ```powershell
> & 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' `
>   'G:\Dev\HBE\HonestlyBadEngine.slnx' /t:Build /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal
> ```

---

## Step 3 — Run and verify

Press **F5**. Walk this checklist:

- [ ] **Black screen** (not the dark-blue item-00 clear).
- [ ] The **player sprite** is visible, roughly **centered**.
- [ ] **A / D** move the player left / right; the sprite **faces** the
      direction it moves (mirrors horizontally).
- [ ] **SPACE** moves **up**, **S** moves **down**.
- [ ] While moving, the **walk** cycle plays (10-frame run); when you stop, it
      returns to the **idle** cycle (4-frame).
- [ ] The **camera eases** to follow — it should *lerp*, not snap. Fly to a hard
      stop and watch it glide the last bit.
- [ ] **H** toggles between the **helmet** and **no-helmet** sprite (the head
      changes) with no other visual glitch.
- [ ] **F11** still toggles fullscreen (engine-global).
- [ ] Console shows `MegaX GameLayer attached (player ghost state).` and **no**
      `player init failed` / `failed to load … sprite sheet` errors.

---

## Troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| `Player.cpp` / `Player.h` "unresolved external" or not compiled | Files not added to the project — redo **Step 1**. |
| `failed to load player sprite sheet(s)` in console | `assets\sprites\Player\player.png` / `player-no-helm.png` missing, or the copy step didn't run. Confirm they exist under `MegaX\bin\x64\Debug\assets\sprites\Player\`. Rebuild to re-trigger `MegaXCopyRuntime`. |
| Window opens but **nothing draws** (pure black) | Sprite pipeline failed. Check console for `failed to create sprite shader/quad`. Ensure `assets\shaders\sprite.vert/.frag` exist (seeded in item 00) — or rely on the inline fallback. |
| Player is a **magenta/black checker** | Texture failed to load → engine placeholder. Same as the sheet-missing case above. |
| Character looks **tiny / huge** | Adjust `kCameraZoom` in `GameLayer.cpp` (bigger = closer) and/or `kPixelScale` in `Player.cpp`. |
| Player **judders/looks blurry** while moving and the **camera jitters** | `m_camera.pixelSnap` is `true`. It rounds the camera to whole *world* units, which at `zoom 3` = 3-screen-pixel steps. Set `m_camera.pixelSnap = false` in `onAttach` (textures are `GL_NEAREST`, so sprites stay crisp). |
| **Wrong frames** animate (e.g. attack instead of walk) | Re-check the grid constants in `Player.cpp`: `kFrameW=75`, `kFrameH=48`, idle `row 3 / cols 0..3`, walk `row 6 / cols 0..9`. |
| SPACE/S feel **inverted** | Swap the two scancodes in the `iy` line of `GameLayer::onUpdate`. |
| `0xC0000135` on launch | Missing runtime DLL next to the exe — the `MegaXCopyRuntime` target (added in item 00). Verify it's present in `MegaX.vcxproj` and rebuild. |

---

## What "done" looks like

A black void with the astronaut/soldier player centered, flying smoothly in all
four directions with A/D/SPACE/S, facing its travel direction, animating
idle↔walk, camera gliding to keep it framed, and **H** hot-swapping the helmet.
That satisfies WorkItems item 1's *ghost state*.

> **Deferred (not this item):** collision/gravity, enemy aggro rules, and
> selecting the helmet **"via scene"** (MegaX has no scene/serialization layer
> yet — helmet is switchable **via code** through `Player::setHelmet(...)` and
> the **H** hotkey for now; the scene hook lands when that system arrives).

Item 01 complete. 🎉
