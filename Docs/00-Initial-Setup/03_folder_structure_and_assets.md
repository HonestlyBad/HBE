# Doc 03 — Folder Structure + Seed Assets

Two jobs here:

1. Create a clean, organized folder tree so future items (player, tiles,
   enemies…) have an obvious home.
2. **Seed the engine's stock shaders** into `assets\shaders\` so the asset
   system initializes — the app **fails to start** if it can't find an
   `assets\` folder.

> **Why assets are mandatory even for a blank window:** `AssetPaths::Initialize`
> searches for an `assets\` folder from the exe dir and CWD; if none is found it
> returns `false` and `Application::initialize` aborts. So `assets\` must exist
> **and be non-empty** (empty folders don't get mirrored next to the exe by the
> copy step). Seeding the shaders satisfies this *and* prepares you for item 01.

---

## Step 1 — Create the folder tree

Under `G:\Dev\HBE\MegaX\`, create:

```
include\
  Game\
src\
  Game\
assets\
  shaders\
  sprites\
    Player\
  tilesets\
    Military\
      Static\
      Animated\
  maps\
  scenes\
  prefabs\
  audio\
  fonts\
  ui\
```

You can do it in Explorer, or from PowerShell:

```powershell
$g = 'G:\Dev\HBE\MegaX'
$dirs = @(
  'include\Game','src\Game',
  'assets\shaders','assets\sprites\Player',
  'assets\tilesets\Military\Static','assets\tilesets\Military\Animated',
  'assets\maps','assets\scenes','assets\prefabs',
  'assets\audio','assets\fonts','assets\ui'
)
foreach ($d in $dirs) { New-Item -ItemType Directory -Force -Path (Join-Path $g $d) | Out-Null }
```

> The `sprites\Player`, `tilesets\Military\...` folders map directly to the
> asset paths in WorkItems.txt (`Player/player.png`,
> `TileSets/Military/Static/Tileset1H.png`, etc.) so later items drop straight
> in. Creating them now keeps things tidy; they can stay empty for item 00.

---

## Step 2 — Seed the engine's stock shaders

The engine's 2D renderer and post-process stack load these GLSL files at
runtime. They are **engine shaders** (generic, not game logic), currently kept
alongside the Sandbox's assets. Copy them into MegaX so MegaX owns its copy and
stays self-contained.

Copy these 8 files from `G:\Dev\HBE\HBE.Sandbox\assets\shaders\` into
`G:\Dev\HBE\MegaX\assets\shaders\`:

```
sprite.vert
sprite.frag
pp_pass.vert
pp_bloom.frag
pp_colorgrade.frag
pp_crt.frag
pp_pixelate.frag
pp_vignette.frag
```

PowerShell:

```powershell
$src = 'G:\Dev\HBE\HBE.Sandbox\assets\shaders'
$dst = 'G:\Dev\HBE\MegaX\assets\shaders'
Copy-Item (Join-Path $src '*.vert') $dst
Copy-Item (Join-Path $src '*.frag') $dst
```

> This is a **one-time file copy**, not a project/code reference to the Sandbox
> — it does not violate the "never reference the Sandbox" rule. You're copying
> the engine's shader assets, the same way a downloaded engine package would
> ship its default shaders.
>
> **Future engine improvement (note, don't act now):** these shaders really
> belong to the engine, not the Sandbox. A later engine task could ship them in
> the engine package so consumers don't copy from the Sandbox. Out of scope for
> item 00.

For item 00 the window is blank, so nothing actually draws with these yet — but
their presence makes `assets\` non-empty and unblocks item 01 immediately.

---

## Step 3 — Add the files to the project (optional but tidy)

So the shaders and future assets show in Solution Explorer:

1. In Solution Explorer, toggle **Show All Files** (top of the pane) — the new
   `include\`, `src\`, `assets\` folders appear.
2. Right-click the `assets` items you want tracked → **Include In Project**, or
   just leave **Show All Files** on. Assets don't need to be "in the project"
   to be copied — the `MegaXAsset` glob in doc 02 copies everything under
   `assets\**` regardless.

---

## Step 4 — `.gitignore`

MegaX's **source, project file, and assets are tracked**; its **build output is
not**. Append to `G:\Dev\HBE\.gitignore`:

```gitignore
# MegaX build output
/MegaX/bin
/MegaX/obj
```

Notes:
- Do **not** ignore `MegaX.vcxproj`, `include\`, `src\`, or `assets\` — those
  are the game and should be committed.
- The repo already ignores `*.zip`, `/x64`, `/external`, `.vs`, etc., so the
  engine libs and third-party DLLs are handled.
- If you want the generated `MegaX.vcxproj.user` untracked (it's machine-local),
  add `/MegaX/MegaX.vcxproj.user` too — optional.

---

## Verify (doc 03)

- [ ] The folder tree above exists under `G:\Dev\HBE\MegaX\`.
- [ ] `assets\shaders\` contains the **8** `.vert`/`.frag` files.
- [ ] `.gitignore` ignores `/MegaX/bin` and `/MegaX/obj` (only).

Next: **`04_minimal_game_code.md`**.
