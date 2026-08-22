# Item 01 — Player Walk / Idle + Camera Lerp ("Ghost State")

**Goal:** draw the player on a **black** screen, roughly **centered**, and let
it free-fly around with **A/D** (left/right) and **SPACE/S** (up/down). The
camera **smoothly lerps** to follow. This is the *ghost state* — no collision,
no gravity, no enemy aggro — the movement mode you'll use to fly around and
build/inspect maps in later items. You can also **swap the helmet / no-helmet
sprite** at runtime.

> **Done when:** black window; player centered; A/D + SPACE/S move it; the
> camera eases (not snaps) to follow; **H** toggles helmet on/off; the player is
> sized sensibly for a 32×32-tile world. (WorkItems.txt, item 1.)

---

## The golden rules (unchanged from item 00)

1. **Only touch MegaX.** Everything here goes under `G:\Dev\HBE\MegaX\`.
2. **Engine = package.** Use only engine **public headers** + built **libs**.
   Do **not** edit any engine code.
3. **Never reference the Sandbox.** We *read* the Sandbox for patterns, but
   MegaX includes/links nothing from it.

---

## Approach (and why)

The engine exposes **two** sprite paths:

| Path | Classes | Notes |
|---|---|---|
| **Lightweight** (chosen) | `SpriteSheet`, `SpriteAnimation`, `SpriteRenderer2D`, `RenderItem`, `Renderer2D`, `CameraController` | Self-contained, no ECS. Perfect for a single ghost player. |
| ECS (Sandbox uses this) | `Scene2D` + `SpriteAnimationStateMachine` + `AnimationPresetRegistry` + prefabs | Heavier: registry, states/transitions, serialization. Overkill for item 1. |

We use the **lightweight path**. It's all public engine classes, needs no
prefab/scene/serializer, and makes the helmet swap a one-liner. (When MegaX
grows a real scene/serialization system in a later item, the player can migrate
to the ECS path — noted, not now.)

### Sprite sheet facts (measured, load-bearing)

`assets\sprites\Player\player.png` and `player-no-helm.png` are **both
750 × 672** and share the **same layout**. They're *packed/trimmed* atlases (the
drawn character is smaller than its cell, with transparent padding), but the
engine treats every sheet as a **uniform grid** — exactly like the Sandbox's
`Soldier.png`. Divide the sheet into:

- **10 columns × 14 rows** → each cell is **75 × 48 px** (`750/10=75`,
  `672/14=48`, both exact).

The two animations we need for item 1 (rows are **0-indexed**):

| Animation | Row index | Columns | Frame count |
|---|---|---|---|
| **Idle** | **3** | 0 … 3 | 4 |
| **Walk** | **6** | 0 … 9 | 10 |

> These map to the sheet's *visual* row 4 (idle, 4 frames) and row 7 (walk, 10
> frames) when counting from 1. The other rows (attack, jump, shoot, etc.) are
> for later items.

### Controls

| Key | Action |
|---|---|
| **A / D** | Move left / right |
| **SPACE** | Move **up** (+Y) |
| **S** | Move **down** (−Y) |
| **H** | Toggle helmet ↔ no-helmet sprite |
| F11 | Fullscreen (engine-global, free) |

> **Note on the WorkItems wording.** Item 1 literally says *"move up with S and
> up with SPACE"* — that's a typo. We implement the sensible mapping:
> **SPACE = up, S = down**. The engine's ortho projection has +Y pointing up
> (`top = +halfH`), so up = increasing world Y. If you ever want them swapped,
> flip the one line in `GameLayer::onUpdate` (doc 02).

### Scale / camera

- The player quad is drawn at **native pixel size** (1 world unit = 1 source
  pixel), so a 75×48 cell = 75×48 world units. The visible character (~24×37 px
  of that cell) then reads as a little over one 32-px tile tall — correct for a
  Mega-Man-X-style world.
- The camera uses **zoom = 3** so the character is comfortably large on screen
  (visible world ≈ 427 × 240 units). Adjust to taste.
- Camera follow uses `CameraController` with `followResponse = 9.0`
  (≈ the classic `lerp(..., 0.15)` at 60 fps). **`pixelSnap` is OFF**: it rounds
  the camera to whole *world* units, which at zoom 3 = 3-screen-pixel steps and
  causes stair-step judder while the player glides. Engine textures are
  `GL_NEAREST`, so sprites stay crisp without it.

---

## Files you'll create / change (all under `G:\Dev\HBE\MegaX\`)

```
include\Game\Player.h        NEW  — the ghost player (sprite + anim + movement)
src\Game\Player.cpp          NEW
include\Game\GameLayer.h     EDIT — hold camera + player + sprite pipeline
src\Game\GameLayer.cpp       EDIT — build pipeline, input, update, render
MegaX.vcxproj                EDIT — add Player.h / Player.cpp to the project
```

No new assets to add — `player.png` / `player-no-helm.png` are already in
`assets\sprites\Player\`, and the sprite shader was seeded in item 00
(`assets\shaders\sprite.vert` / `sprite.frag`).

---

## The 3 steps (read in order)

1. **`01_player_class.md`** — create `Player.h` / `Player.cpp`: load both
   sheets, build idle/walk animations, movement, facing flip, helmet swap.
2. **`02_gamelayer_wiring.md`** — extend `GameLayer`: sprite pipeline (shader +
   quad), black clear color, camera controller, input, update + render loop.
3. **`03_build_run_and_verify.md`** — add the files to `MegaX.vcxproj`, build,
   run, and walk the verification checklist (+ troubleshooting).

Now open **`01_player_class.md`**.
