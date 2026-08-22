# Item 02 · Doc 04 — Add files, build, run, verify

## 1. Add the new source files to the MegaX project

The three new files must be compiled/visible in `MegaX.vcxproj`. Two ways:

### Option A — Visual Studio (recommended)

1. In **Solution Explorer**, right‑click the **MegaX** project → **Add → New
   Filter** (or reuse folders) and create the files under:
   - `include\World\AnimatedTile.h`
   - `include\World\World.h`
   - `src\World\World.cpp`
2. If you created them on disk first, use **Add → Existing Item…**, select all
   three, and add them. VS puts `.h` under `ClInclude` and `.cpp` under
   `ClCompile` automatically.
3. Save the project (**Ctrl+S** on the project node).

### Option B — Edit `MegaX.vcxproj` directly

Add the header entries to the existing `ClInclude` group and the source to the
`ClCompile` group:

```xml
  <ItemGroup>
    <ClInclude Include="include\Game\GameLayer.h" />
    <ClInclude Include="include\Game\Player.h" />
    <ClInclude Include="include\World\AnimatedTile.h" />   <!-- add -->
    <ClInclude Include="include\World\World.h" />          <!-- add -->
  </ItemGroup>
  <ItemGroup>
    <ClCompile Include="src\Game\GameLayer.cpp" />
    <ClCompile Include="src\Game\Player.cpp" />
    <ClCompile Include="src\World\World.cpp" />            <!-- add -->
    <ClCompile Include="src\main.cpp" />
  </ItemGroup>
```

(Optional, for map visibility in Solution Explorer — not required for build:)

```xml
  <ItemGroup>
    <None Include="assets\maps\level_01.json" />
  </ItemGroup>
```

## 2. Assets copy automatically

`MegaX.vcxproj` already globs **all** assets to the output each build:

```xml
<MegaXAsset Include="$(ProjectDir)assets\**\*" />
```

So `assets\maps\level_01.json` **and** `assets\tilesets\Military\Static\*.png`
are copied to `bin\x64\Debug\assets\...` on the next build. No project change is
needed for the tilesets or the map — just make sure the files exist on disk:

- `assets\maps\level_01.json`
- `assets\tilesets\Military\Static\Tileset1H.png` (and `2H`, `3H`)

## 3. Build

Build the **solution** (not the bare project) — Debug | x64:

- Visual Studio: **Build → Build Solution** (Ctrl+Shift+B), or
- Command line:

```
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' `
  'G:\Dev\HBE\HonestlyBadEngine.slnx' /t:Build /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal
```

Expect `MegaX.vcxproj -> ...\MegaX.exe` and `MegaX: copying runtime deps ...`.

## 4. Run and verify

Launch **MegaX** (F5 in VS, or run `bin\x64\Debug\MegaX.exe`). Check:

- [ ] Console logs `World loaded 'maps/level_01.json' (3 tilesets, 3 layers).`
- [ ] A **floor** of tiles spans the bottom, with two **floating platforms**.
- [ ] **Machine blocks** and **vertical pipe columns** appear (all 3 tilesets
      visibly render — different art in each cluster).
- [ ] The ghost player is centered over the map and renders **on top** of tiles.
- [ ] `A`/`D` move, `SPACE`/`S` fly up/down, `H` toggles the helmet, and the
      **camera follows smoothly** (no judder — item‑01 fix still in place).
- [ ] `F11` still toggles fullscreen.

## 5. Troubleshooting

| Symptom | Cause / Fix |
|---|---|
| Console: `failed to load 'maps/level_01.json'` | File missing or bad JSON. Confirm it's at `assets\maps\level_01.json` and validate the JSON (comma/bracket). Rebuild so it copies to `bin\...\assets`. |
| `TileMapRenderer::build failed` or magenta/placeholder tiles | A tileset texture path is wrong. `texture` is **relative to the map file** → from `assets/maps/` use `../tilesets/Military/Static/Tileset1H.png`. Confirm the PNGs copied to `bin\...\assets\tilesets\...`. |
| Tiles show but look shifted / wrong tiles | Remember `data` is **bottom‑up** (first row = bottom) and ids are **1‑based, local** to that layer's tileset. Check `w*h == data length` (384). |
| Only one tileset's art appears | Each layer must set the right `tileset` index (0/1/2) and place ids from **that** set. Empty (`0`) cells are skipped. |
| Player hidden behind tiles | Ensure `m_world.render(r2d)` is called **before** `m_player.render(r2d)` and the player item keeps render layer 100. |
| `unresolved external` / `World::...` link error | `src\World\World.cpp` wasn't added to `ClCompile`. Re‑add it (step 1) and rebuild. |
| `cannot open include "World/World.h"` | The project include dir is `$(ProjectDir)include\` — put headers under `include\World\`, not elsewhere. |
| Build fails only on bare `MegaX.vcxproj` | Build the **`.slnx` solution** so the engine projects resolve their own headers. |

## 6. What's ready for item 03

- `World::update(dt)` already ticks an animation clock.
- `World::renderAnimatedTiles()` is the empty seam to fill.
- `AnimatedTile` struct + `"animatedTiles": []` in the map are ready to populate.
- MegaX already links `nlohmann/json` (via `$(SolutionDir)external\nlohmann`),
  so item 03 can parse the `animatedTiles` array directly.
