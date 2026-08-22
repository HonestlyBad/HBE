# 04 — Build, run and verify

Everything you need to compile the enemy work, run it, and prove
that Item 08 is complete.

---

## 1. Register the new files with MSBuild

Open `G:\Dev\HBE\MegaX\MegaX.vcxproj`. Add the four new entries
alongside the existing enemy-adjacent items.

### `ClInclude` group

Find:

```xml
  <ItemGroup>
    <ClInclude Include="include\Game\Effects.h" />
    <ClInclude Include="include\Game\GameLayer.h" />
    <ClInclude Include="include\Game\Player.h" />
    <ClInclude Include="include\Game\Bullet.h" />
    <ClInclude Include="include\World\AnimatedTile.h" />
    <ClInclude Include="include\World\World.h" />
  </ItemGroup>
```

Add:

```xml
  <ItemGroup>
    <ClInclude Include="include\Game\Effects.h" />
    <ClInclude Include="include\Game\GameLayer.h" />
    <ClInclude Include="include\Game\Player.h" />
    <ClInclude Include="include\Game\Bullet.h" />
    <ClInclude Include="include\Game\Enemy.h" />          <!-- NEW -->
    <ClInclude Include="include\Game\EnemyManager.h" />   <!-- NEW -->
    <ClInclude Include="include\World\AnimatedTile.h" />
    <ClInclude Include="include\World\World.h" />
  </ItemGroup>
```

### `ClCompile` group

Find:

```xml
  <ItemGroup>
    <ClCompile Include="src\Game\Effects.cpp" />
    <ClCompile Include="src\Game\GameLayer.cpp" />
    <ClCompile Include="src\Game\Player.cpp" />
    <ClCompile Include="src\Game\Bullet.cpp" />
    <ClCompile Include="src\main.cpp" />
    <ClCompile Include="src\World\World.cpp" />
  </ItemGroup>
```

Add:

```xml
  <ItemGroup>
    <ClCompile Include="src\Game\Effects.cpp" />
    <ClCompile Include="src\Game\GameLayer.cpp" />
    <ClCompile Include="src\Game\Player.cpp" />
    <ClCompile Include="src\Game\Bullet.cpp" />
    <ClCompile Include="src\Game\Enemy.cpp" />            <!-- NEW -->
    <ClCompile Include="src\Game\EnemyManager.cpp" />     <!-- NEW -->
    <ClCompile Include="src\main.cpp" />
    <ClCompile Include="src\World\World.cpp" />
  </ItemGroup>
```

> Reminder from the repository memory: `MegaX.vcxproj` and
> `MegaX.vcxproj.filters` are git-ignored. Changes will build
> locally but never appear in your git diff.

### `.filters` — create an "Enemies" sub-filter

Open `G:\Dev\HBE\MegaX\MegaX.vcxproj.filters`. In the `<ItemGroup>`
that lists `<Filter>` entries add one new filter (any new GUID
works; only its name is meaningful to Visual Studio):

```xml
    <Filter Include="assets\sprites\Enemies">
      <UniqueIdentifier>{7c1a5b4e-6a48-4b71-b02b-2a4bcf8ed001}</UniqueIdentifier>
    </Filter>
    <Filter Include="assets\sprites\Enemies\RobotSoldier">
      <UniqueIdentifier>{7c1a5b4e-6a48-4b71-b02b-2a4bcf8ed002}</UniqueIdentifier>
    </Filter>
```

Then in the same file, add the two new source entries to their
respective `<ClInclude>` and `<ClCompile>` groups (they inherit
the top-level "include" / "src" filter, no explicit `<Filter>`
child needed):

```xml
    <ClInclude Include="include\Game\Enemy.h" />
    <ClInclude Include="include\Game\EnemyManager.h" />
```

```xml
    <ClCompile Include="src\Game\Enemy.cpp" />
    <ClCompile Include="src\Game\EnemyManager.cpp" />
```

Optional but nice: register the three sprite sheets under the
new filter so they show up in the Solution Explorer tree:

```xml
  <ItemGroup>
    <None Include="assets\sprites\Enemies\RobotSoldier\SoldierIdle_Spritesheet.png">
      <Filter>assets\sprites\Enemies\RobotSoldier</Filter>
    </None>
    <None Include="assets\sprites\Enemies\RobotSoldier\SoldierFordward_Spritesheet.png">
      <Filter>assets\sprites\Enemies\RobotSoldier</Filter>
    </None>
    <None Include="assets\sprites\Enemies\RobotSoldier\SoldierFire_Spritesheet.png">
      <Filter>assets\sprites\Enemies\RobotSoldier</Filter>
    </None>
  </ItemGroup>
```

You don't have to add matching `<None>` entries to the main
`.vcxproj` — the post-build robocopy rule mirrors the whole
`assets` tree, so the PNGs are already deployed.

---

## 2. Build

From a `Developer PowerShell for VS 2022` (or run
`vcvars64.bat` first):

```powershell
cd G:\Dev\HBE
msbuild HonestlyBadEngine.slnx /p:Configuration=Debug /p:Platform=x64 /t:MegaX /m /nologo /v:m
```

A successful build ends with `MegaX.vcxproj -> ...\MegaX.exe`
and `exit code 0`. The one benign warning to expect is:

```
Scene2D.h(20,11): warning C4099: 'HBE::Renderer::TileMap':
    type name first seen using 'struct' now seen using 'class'
```

That comes from including `TileCollision.h` (which pulls
`TileMap.h`) in a TU that also indirectly sees Scene2D's forward
declaration. It's harmless and pre-existing — leave it for the
future engine pass.

---

## 3. Run

```powershell
& "G:\Dev\HBE\MegaX\bin\x64\Debug\MegaX.exe"
```

Expected log lines during startup (order-sensitive; extra info
lines are fine):

```
[INFO]OpenGL initialized via GLAD. Version 3.3
[INFO]World loaded 'maps/level_01.json' (...)
[INFO]MegaX GameLayer attached (Play mode; press G for Ghost).
```

If you see `[ERROR]Enemy::init: failed to load Robot Soldier Idle
sheet ...`, jump to the Troubleshooting section below.

---

## 4. Verify checklist

Walk through every item — this is the acceptance test for Item 08.

* [ ] **Enemy is visible on the level.** A robot soldier stands
      ~5 tiles to the right of the player's spawn point, playing
      its idle animation (4 frames, looping).
* [ ] **Enemy faces left by default.** The gun points toward the
      player's spawn side.
* [ ] **Hit / hurt overlay off by default.** No boxes are drawn
      when the game starts.
* [ ] **B toggles the overlay.** Pressing `B` prints a
      `MegaX: hit/hurt box overlay ON.` log line. Pressing again
      prints `... OFF.`.
* [ ] **When overlay is ON:**
    * The player's collision box is outlined in **blue**.
    * The enemy's hurtbox is outlined in **green** and hugs the
      visible sprite silhouette (feet on the ground).
    * No red box appears (the enemy's hitbox is `active = false`
      in Item 08). This is expected — the red box shows up in
      Item 10 when the enemy starts attacking.
* [ ] **Firing at the enemy registers a hit.** Pressing `E` while
      facing the enemy produces the sleek bullet, and the moment
      the bullet's center enters the green hurtbox:
    * a yellow-tan bullet-impact burst spawns at the hit point,
    * the enemy briefly flashes red for ~0.1 s,
    * the bullet disappears (does not continue past the enemy).
* [ ] **Damage is cumulative.** Three hits (default `startHp = 3`,
      `damagePerHit = 1`) drop the enemy to zero HP.
* [ ] **On death the enemy fades out** over ~0.6 s. The sprite's
      alpha decays smoothly to 0, then the enemy is removed from
      the manager (the debug overlay stops drawing its green box
      the frame after fade-out completes).
* [ ] **After death, bullets pass through.** Any bullets still in
      flight travel past the (now-invisible) enemy without doing
      anything.
* [ ] **Ghost mode is neutral.** Pressing `G` still toggles
      ghost / play; bullets still fire, hit tests still work. The
      enemy does not chase you (it can't move yet — Item 09).
* [ ] **No new warnings or errors in the console** apart from
      the pre-existing C4099 warning listed above.

---

## 5. Tuning table

Runtime-adjustable knobs on `Enemy` (all `public:` fields — either
poke them in a debugger or override them in the spawn block):

| Field              | Default | What it does                                              |
|--------------------|---------|-----------------------------------------------------------|
| `maxHp`            | 3       | Bullets needed to kill. Re-call `Enemy::spawn(...)` after changing so the current HP re-snapshots. |
| `damagePerHit`     | 1       | Reference value; the current damage per bullet is the `damagePerBullet` arg you pass to `EnemyManager::checkBulletHits`. |
| `invulnAfterHit`   | 0.08 s  | Blocks double-counting a single bullet across two frames. Raise to `0.15` if you see instakills. |
| `hitFlashTime`     | 0.10 s  | How long the red hit-flash stays. Push to 0.18 for a more Mega-Man X feel. |
| `deathFadeTime`    | 0.60 s  | Total fade-out length. Item 12's explosion should sit inside this window. |
| `hurtHalfW / halfH`| 11 / 18 | Hurtbox half-extents in pixels. Match to the visible sprite silhouette in the B overlay. |
| `hurtOffsetX / Y`  | 0 / 0   | Move the hurtbox off-center. `OffsetX` mirrors with facing. |
| `hitHalfW / halfH` | 20 / 14 | Placeholder attack box; Item 10 will actually turn it on. |
| `hitOffsetX / Y`   | 20 / 0  | Attack box sits in front of the enemy by default.         |

---

## 6. Troubleshooting

| Symptom | Probable cause | Fix |
|---------|----------------|-----|
| `Enemy::init: failed to load Robot Soldier Idle sheet` at startup. | Sprite sheet not in the deployed `assets` folder. | Confirm `G:\Dev\HBE\MegaX\bin\x64\Debug\assets\sprites\Enemies\RobotSoldier\SoldierIdle_Spritesheet.png` exists. The post-build robocopy uses `/XO`, so a fresh PNG will be copied on the next build. |
| `C2011 SpriteRenderer2D redefinition` in `EnemyManager.cpp` or `Enemy.cpp`. | Added an ECS / Scene2D / ParticleSystem include to `Enemy.h` or `EnemyManager.h`. | Remove those includes. Only `Sprite2D.h`, `RenderItem.h`, `Material.h`, `TileCollision.h`, `DebugDraw2D.h` are safe. |
| Enemy is drawn but hovers above the floor. | `m_player.feetY()` isn't settled on the frame the spawn runs. | Replace `eGroundY = m_player.feetY();` with a hand-picked world Y (e.g. `startY + 20.0f`) that visually rests on the floor. |
| Enemy sprite is squashed to a horizontal line. | Frame size mismatch in `Enemy.cpp` — `kFrameW / kFrameH` differ from the sheet. | Sheet is 4c × 2r on a 256x128 image; each frame is exactly 64x64. |
| Only one frame of the idle animation plays. | Forgot the `m_idleAnim.play(true)` in `Enemy::init`. | Add it — `SpriteAnimation` starts stopped by default. |
| Bullets pass through the enemy. | `hurtbox()` returns a box the bullet never overlaps. | Turn the overlay on (`B`), watch where the box actually is — likely `hurtOffsetY` needs tweaking or `hurtHalfW/H` are too tight. Refer to the B-overlay green rectangle. |
| One bullet counts as two hits (enemy dies in one trigger pull with `startHp=3`). | `invulnAfterHit` is too low for your bullet cadence. | Raise it to `0.15`. |
| Enemy vanishes instantly instead of fading. | `deathFadeTime` was set to 0 (or an unrelated header was edited that dropped the value). | Confirm the tunable is `0.60f` in the `Enemy.h` `public:` section. |
| B does nothing / no log line. | The `B` block was pasted inside the `G` block's `{}` and got dead-coded. | Move the `if (Input::IsKeyPressed(SDL_SCANCODE_B))` to sit AFTER the closing brace of the `G` block. |
| Overlay boxes render but the ENEMY sprite is missing. | Enemy's render layer (99) is fine, but you may have swapped the `m_enemies.render` and `m_player.render` order and are now covering the enemy. Or your `m_material.texture` handle went stale. | Ensure `m_enemies.render(r2d)` sits between `m_world.render(r2d)` and `m_player.render(r2d)`. |
| Rare crash: `read access violation. this was nullptr` on first shot. | `EnemyManager::init` was never called (dupe-paste error like the one that hit Item 07 with `m_bullets.init`). | Confirm `onAttach` calls `m_enemies.init(...)` exactly once, before the spawn block. |

---

## 7. What comes next (heads-up for Item 09)

Item 09 adds patrol + player detection. When you write that code:

* Reuse `Enemy::spawn(x, groundY, facing)` — you'll just change
  `facing` per-tick as the enemy turns around at patrol
  waypoints. Keep the sprite anchor code identical.
* The Fordward (walk) sheet is 4 cols × 4 rows. Because
  `SpriteAnimation` only plays one row you'll either want to
  concatenate a small game-side "multi-row animator" or select
  the row that reads best as a walk cycle.
* Player detection can use the same `hurtbox()` you added to
  `Player.h` for line-of-sight / range checks, so you already
  have the accessor.
* The debug overlay you added in Item 08 is exactly the tool
  you'll want for verifying the detect-radius circle in Item 09.
  Extend `DebugDraw2D` (game-side wrapper) to draw circles too if
  the engine's `rect()` isn't enough.

---

Item 08 is done when every checklist item in section 4 passes.
