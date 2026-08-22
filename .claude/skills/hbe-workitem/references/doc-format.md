# Work item doc format

The structure below is the **item 13/14 form**, which the user named as
the quality bar, corrected against an audit of all fifteen existing
work item directories (00–14). Read `Docs/13-*/` if anything is
ambiguous.

---

## Read this first: 13/14 are not the whole pattern

Items 13 and 14 are engine-infrastructure items. They have no gameplay
feel, add no hotkeys, and ship no assets — so they omit three sections
that nearly every **gameplay** item carries. Copying 13/14 blindly onto
a MegaX item produces a doc set missing the parts the user leans on
most while tuning.

Measured across `Docs/00-*` … `Docs/14-*`:

| Section | Appears in | Absent from |
|---|---|---|
| **Tuning table** | 04, 05, 06, 07, 08, 09, 10, 11, 12 (9 of 10 gameplay items) | 13, 14 |
| **Controls added in Item NN** | 08, 09, 10, 11, 12 (every item that added a key) | 13, 14 |
| **Asset notes** | 01, 03, 04, 06, 08, 09 (every item that consumed new art) | 13, 14 |

So: **§"Which sections apply" below is the real spec.** Pick sections
by what the item does, not by which doc you last read.

### Format history — for reading the older dirs

Three eras. Don't imitate the first two; know them so old docs read
correctly.

* **00–07** — titles `# Item 02 · Doc 01 — …`, unnumbered `##` headings
  (`## Goal`, `## What we add`), prose at ~70 columns.
* **08–12** — titles `# 09 — Title (overview)`, numbered `## 1.` with
  `### 1a.` / `### 1b.` sub-steps, Group A/B/C verify checklists
  appear, prose narrows to ~57.
* **13–14** — current. Overview sections become the fixed numbered set
  below, sub-steps become `### 2.1`, prose at ~55.

Item 00 is entirely Visual Studio project configuration and is
**obsolete** — the project is CMake now. Never use it as a model.

---

## Which sections apply

Always, every item:

* overview §1 deliverables, §2 success criteria, §3 golden rules,
  §4 files touched, §5 design shape, §6 non-goals, §7 reading order
* implementation docs
* build / run / verify with Group A regression + troubleshooting matrix

Add when the item **changes gameplay feel or balance** (any `[MEGAX]`
item, and engine items with tunable output):

* **Tuning table** in the build/verify doc — the single most-used
  section in items 04–12.

Add when the item **binds a new key or changes an existing one**:

* **Controls added in Item NN** in the overview.

Add when the item **consumes new art, audio, or map data**:

* **Asset notes** in the overview, with measured facts.

Add when the item **defines or changes a data file format** (JSON map,
CSV capture, item/loot tables, room templates):

* a dedicated **data-format doc** — see its anatomy below.

---

## House style (all docs)

* **Prose wrapped at ~60 columns** (13/14 run a median of 55). These
  read as a narrow column. Tables, code blocks, and paths are exempt.
* **Linux absolute paths**, backticked:
  `/home/atulo/Projects/HBE/HBE.Core/include/HBE/Core/Profiler.h`.
  Repo-relative is acceptable inside a table or a `grep` command where
  the doc has already established the root.
* **fish** for shell snippets. `` ```fish `` fences.
* Second person, imperative: "Open X. Find line N. Insert this."
* Emphasis carries meaning: **NEW** / **EDIT** in tables, **Do not**
  for prohibitions, `>` blockquotes prefixed with a capitalized label
  for asides — `> **WHY HERE?**`, `> **PASTE NOTE.**`, `> **NOTE ON
  SCOPE.**`.
* Section numbering is `## 1.`, `## 2.` … with `### 2.1` subsections.
  Items 08–12 used `### 1a.` / `### 1b.` for sub-steps within one
  file; either reads fine, but stay consistent inside a doc set.
* `---` rules between top-level sections.
* Every doc except the last ends with `Next: NN_<name>.md`.

---

## `00_overview.md`

Title: `# NN — [TAG] Human Readable Title (overview)`

Opening 1–2 paragraphs, no heading: what the previous item finished,
what this item adds, and — for `[HBE]` items — the sentence that pins
scope, e.g. "Per the work-item tag `[HBE]`, all API and implementation
edits live under `HBE.Core/`."

### `## 1. What Item NN delivers`

Bulleted, concrete, naming real types and symbols. Nested bullets for
struct fields. This is a contract, not a summary — a reader should be
able to predict every signature in the later docs from this list.

When an `[HBE]` item also touches MegaX, close the section with a
blockquote saying **which of the two kinds** it is. They are not
interchangeable, and the difference decides whether the MegaX doc is
skippable.

Verification / demonstration — the doc is optional to apply:

```
> **NOTE ON `[HBE]` SCOPE.** The engine changes above are the full
> deliverable. The MegaX edits in doc `04` are strictly a consumer of
> the new API — if you strip them out, HBE still builds and still
> ships the feature.
```

Required because the engine change would break the game — the doc is
**not** optional:

```
> **NOTE ON `[HBE]` SCOPE.** The engine changes above are the
> deliverable, but they change `<symbol>`, which MegaX calls in
> `<file>`. The MegaX edits in doc `04` are **required** — without
> them MegaX does not compile. They add no game-specific logic to HBE
> and change no gameplay.
```

An item can carry both kinds, in which case say so and keep them in
separate docs. Never label a required fix "verification only" — the
user reads that label to decide what they can defer.

### `## 2. Success criteria`

"Item NN is complete when **all** of the following hold:" then a
numbered list of *observable* conditions — a build that succeeds, a
log line with a given shape, a measurable relationship between
numbers. Include a fenced example of expected output where the item
produces any. Never "the code is clean" or "it works well".

### `## 3. Golden rules (read before touching code)`

Numbered, **bolded lead phrase**, then the reasoning. This is where
design decisions get frozen so the implementation docs can be
mechanical. Cover: scope boundaries, the API shape that was chosen and
the one that was rejected and why, allocation/perf constraints, thread
affinity, ordering requirements, and what must keep working. 8–10
rules is normal.

### `## 4. Files touched`

| File | Item N-1 | Item N |
|---|---|---|
| `HBE.Core/include/HBE/Core/Foo.h` | — | **NEW** — public API, `Bar` struct, macros |
| `HBE.Core/src/Core/Application.cpp` | main loop | `BeginFrame/EndFrame` bracketing, one `HBE_PROFILE_SCOPE` |
| `HBE.Core/CMakeLists.txt` | — | add `src/Core/Foo.cpp` to `add_library` |

Repo-relative paths here. The "Item N-1" column shows what the file
was before, so the user can see what they're modifying versus creating.
Close with an explicit list of what is **not** touched.

**This table must exactly match what the implementation docs do.**
It is the item's blast radius.

### `## 5. Design shape`

The interesting part: the data structures, the algorithm, the final
public API surface as one fenced `cpp` block. Sub-headed with `###`
(e.g. `### The scope stack`, `### The rolling ring`,
`### Snapshot publication`). Explain *why* the shape is what it is.

### `## 6. Non-goals (deferred)`

| Behavior | Deferred to |
|---|---|
| GPU timer queries | Item 14 |
| ImGui profiler window | **Never** — item explicitly forbids it |

Cross-reference real backlog item numbers from `Docs/WorkItems.txt`.

### `## 7. Reading order`

Numbered list of every doc with a one-line purpose, then:

> Do them in that order. Each doc lists **exact** file paths, exact
> insertion points, and full copy-paste code blocks.

### Conditional: `## Controls added in Item NN`

Whenever the item binds a key. Items 08–12 all carry this; the user
relies on it to know what to press when verifying.

```
| Key | Effect |
|---|---|
| `F5` | Full scene reload — reloads `level_01.json`, clears entities, respawns, snaps camera. Preserves difficulty, mode, helmet. |
```

Then a paragraph confirming **every existing control keeps its current
behavior**, listing them. Item 11 explicitly notes that `R` stays
"refill HP" and does *not* become reload "even though the idea is
tempting" — that kind of stated non-change prevents drift.

Cross-check the table against the real key block in
`MegaX/src/Game/GameLayer.cpp` (~lines 128–171) before shipping.

### Conditional: `## Asset notes`

Whenever the item consumes new art, audio, or map data. **Measure, do
not guess** — item 08 verified sheets "by alpha-band scan", item 01
calls its sprite facts "measured, load-bearing".

```
| Sheet | Image size | Grid | Frame size | Notes |
|---|---|---|---|---|
| `SoldierIdle_Spritesheet.png` | 256 x 128 | 4c × 2r | 64 × 64 | Only row 0 used (4 frames). |
```

Follow with where visible pixels sit inside each cell, how that maps to
the anchoring convention, and any engine limitation that constrains the
approach (item 08: "`SpriteAnimation` only supports a single row").

Asset paths for an item are often listed under `-ASSETS:` in that
item's `Docs/WorkItems.txt` entry — start there, then verify on disk.

---

## Implementation docs (`01_*` … `NN-1_*`)

Title: `# NN — Short description (`path/or/subject`)`

Opening paragraph: what this doc does, which files it touches, what it
depends on. If it creates a file, say what that file may and may not
include.

### Optional opener: `## Key engine APIs used here`

Item 01 opened its implementation docs with a bulleted list of the
engine APIs the doc composes, each with its signature and the one fact
that bites:

```
- `SpriteAnimation(sheet*, colStart, colEnd, row, fps, loop)` — plays one
  **row** of frames. `apply(RenderItem&)` writes the current frame's `uvRect`.
- `Transform2D` — `{ posX, posY, rotation, scaleX, scaleY }`. **Negative
  `scaleX` mirrors horizontally** (that's our facing flip).
```

This dropped out after item 01. **Revive it** whenever a doc composes
engine APIs the user hasn't used in a recent item — it saves a trip to
`Docs/GameEngine/ENGINE_API_REFERENCE.md` mid-implementation. Verify
each signature against the real header, not the reference.

### Creating a new file

```
## 1. Create the file

**Full path:** `/home/atulo/Projects/HBE/HBE.Core/include/HBE/Core/Foo.h`

**Status:** file must not exist yet. If it does, stop and check —
someone likely started this work item already.

Paste the following content **verbatim** (no reformatting):
```

Then the **entire file** in one fenced block. Not a sketch. Comments
included — the comments in these files explain non-obvious choices and
are part of the deliverable.

Follow with a `> **PASTE NOTE.**` blockquote for anything an editor
might mangle: macro line-continuations, tabs vs spaces, trailing
whitespace.

### Registering a new `.cpp`

Every new `.cpp` needs a CMake entry. Headers need none.

````
## 2. Register the source in `HBE.Core/CMakeLists.txt`

Open `/home/atulo/Projects/HBE/HBE.Core/CMakeLists.txt`.

Inside `add_library(HBE.Core STATIC ...)`, the source list is
alphabetical within each subdirectory. Insert between
`src/Core/Log.cpp` (line 8) and `src/Core/Time.cpp` (line 10):

```cmake
    src/Core/Profiler.cpp
```

The final block should read:

```cmake
add_library(HBE.Core STATIC
    src/Core/Application.cpp
    ...
)
```
````

Show the surrounding lines so placement is unambiguous. Never mention
`.vcxproj`, `.filters`, or `.slnx`.

### Editing an existing file

For each edit:

1. `Open <absolute path>.`
2. Quote the **current** text with its line range: "The include block
   at the top currently reads (lines 3-11):" + fenced block.
3. State the insertion point twice — by line number **and** by the text
   at that line: "Insert right after `#include "HBE/Core/LayerStack.h"`
   (line 3) and before `#include "HBE/Core/AssetPaths.h"` (line 4)".
4. The new text alone, fenced.
5. "Final block should read:" + the complete patched region, fenced.

For a function body with several changes, print the whole patched
function once and say "Replace lines 333–482 inclusive with the
following" — do not make the user apply five separate hunks to one
function.

### Two escape hatches for heavily-edited files

When a file accumulates so many small edits that hunk-by-hunk becomes
error-prone, the earlier docs used one of these instead. Both are
legitimate; pick deliberately.

**Full replacement** — item 09 doc 02 is literally
`## Full replacement — src/Game/Enemy.cpp`, 500+ lines. Use when a
file changes more than it stays the same. Say plainly that the file is
being replaced wholesale so the user doesn't hunt for a diff.

**Full expected state** — item 10 doc 01 §1c
(`### 1c. Full expected top of EnemyManager.h after edits`) and item 11
doc 02 §1d (`Full expected private-block after doc 02 + doc 03 edits`).
After a run of small edits, print the complete resulting region as a
checkpoint. Use this whenever **a later doc in the same set edits the
same region** — it re-anchors the user before the line numbers shift
again.

### `## N. What NOT to touch`

Bulleted prohibitions with reasons. Prevents the user from
"helpfully" over-applying a pattern: *"Do not add a scope inside
`handleSDLEvent` — the callback fires 10-100 times per frame and would
flood the section table."* Include one per plausible mistake.

### `## N. Sanity check before doc NN+1`

fish commands with expected results as comments:

```fish
test -f /home/atulo/Projects/HBE/HBE.Core/include/HBE/Core/Profiler.h
# -> exit status 0

grep -c "Profiler.cpp" /home/atulo/Projects/HBE/HBE.Core/CMakeLists.txt
# -> 1
```

Say explicitly whether the project should compile at this point. Often
it should **not** yet — say so, and say why, so a failed build doesn't
read as a mistake.

End with `Next: NN_<name>.md`.

---

## `NN_build_run_and_verify.md` (always last)

### `## 1. Files touched recap`

Bulleted **NEW** / **EDIT** list, mirroring overview §4, plus an
explicit "no changes to X, Y, Z".

### `## 2. Pre-build sanity check`

fish greps proving each registration landed exactly once.

### `## 3. Build`

Real commands (see `build-and-run.md`), the expected compile output
lines, and then a bulleted list of **specific compiler/linker errors**
mapped to their cause:

```
* **`fatal error: 'HBE/Core/Profiler.h' file not found`**
  — the header wasn't saved to the right path.

* **`undefined reference to 'HBE::Core::Profiler::BeginScope(char const*)'`**
  — `Profiler.cpp` isn't in the `add_library` list (doc `02` §2).
```

Use real clang/GCC diagnostic wording, not MSVC's `C1083`/`LNK2019`.
Include the Release build as a separate check when the item has any
build-configuration behavior.

### `## 4. Run`

The exact run command, expected startup log lines, and a fenced block
of the expected new output with a note on what varies per machine and
what must match structurally.

### `## 5. Verify checklist`

`- [ ]` checkboxes grouped by concern:

* **Group A — previous item regression.** Always. Prove nothing broke.
* **Group B onward** — one group per new behavior, then edge cases,
  toggles, interaction with earlier features (hot reload, difficulty
  switching), and the Release build.

### Conditional: `## N. Tuning table — the flavor knobs`

**Required for any item that changes gameplay feel or balance.**
Present in 9 of 10 gameplay items (04–12). This is where the user
lives after the build works.

Not a list of variables — a **symptom → knob → direction** table, so
they can go from "this feels wrong" to an edit without reasoning:

```
| Symptom | Field | Direction |
|---|---|---|
| Casual is *too* easy — enemy never catches you | `Casual.chaseSpeedMul` | `0.50 → 0.65` |
| Challenging leads too aggressively (you die instantly) | `Challenging.leadFactor` | `1.0 → 0.6` |
| Knockback launches you off ledges | `Player::knockbackImpulse` | `260 → 150` |
```

Rules:

* Phrase symptoms in **feel** terms, the way the user would complain.
* Give **concrete current → suggested** values, not "increase it".
  Read the current value out of the code; never invent it.
* Open with where the knobs live and the iteration loop —
  "everything here lives in `EnemyManager.cpp`'s `MakeProfile` switch;
  edit and rebuild" or "edit + F5 to iterate".
* Sub-group with `###` once past ~10 rows. Item 12 uses
  **Cadence / intensity**, **Color**, **Layout / anchor**.
* Close with any **ordering constraint** on applying the knobs — item
  10 warns that per-enemy overrides must precede `snapshotBaseStats()`
  or the multiplier lands on the wrong base.

### `## N. Troubleshooting matrix`

| Symptom | Likely cause | Fix |
|---|---|---|

One row per failure the user could realistically hit. This is the most
valuable section in the whole set — be generous, 10–15 rows.

### `## 7. Done-criterion repro`

Quote the done criterion from `Docs/WorkItems.txt` verbatim and walk
through exactly what observation satisfies it.

### `## 8. What comes after this item`

Name the next backlog items that build on this one and what they will
consume, so the API's minimalism reads as deliberate.

Close with `Item NN is complete.`

---

## Data-format docs (conditional doc type)

Use whenever an item defines or changes a data file — a JSON map, a
CSV capture layout, item/loot tables, room templates. Modelled on
`Docs/02-World-From-Tilesets/02_map_json.md` and
`Docs/03-Animated-Tiles/02_map_json.md`.

Upcoming items that need one: 15 (CSV capture columns), 20 (golden
room), 33 (item definitions), 37 (loot tables), 51 (room-template
format), 58 (biome profiles).

Anatomy:

### `## 1. Schema (what each field means)`

An **annotated** `jsonc` block — every field carrying an inline comment
with units and constraints. The annotation is the deliverable; a bare
schema is not:

```jsonc
"tileSize":       { "w": 32, "h": 32 },   // px per tile in the atlas
"tilePixelScale": 1.0,                     // world px per atlas px
"tilesets": [ ... ],                       // index 0,1,2 referenced by layers
"solidTiles": [ 4, 10, 16 ]                // 1-based ids; collision (item 04)
```

### `### Critical authoring rules`

The conventions that silently corrupt the file when broken. Item 02's
list is the model: `data` is row-major **bottom-left origin**, ids are
**1-based and layer-local** (`0` = empty), `texture` resolves
**relative to the map file**, `data` length must equal `w * h`.

Every rule that has ever cost an hour goes here, bolded.

### `## 2. Full file`

The **complete** file, pasteable. For a map, choose content that
exercises every code path the item added (item 02's starter map uses
all three tilesets deliberately, "so you can confirm the multi-tileset
pipeline works").

### `## 3. What this produces`

Read the data back in plain language — what draws where, in what
order, and why. Item 02 walks its layers bottom → top and explains the
resulting depth order.

### `## 4. Designing your own`

How the user authors more of this data later: where to find valid ids,
one-layer-per-tileset style rules, the quick sanity check ("your floor
row is the **first** `data` row"), and which field to touch to opt into
a behavior added by a later item.

For an **edit** to an existing format rather than a new one, item 03's
doc splits into `## Edit 1 — fill "animatedTiles"` / `### Field
meanings` / `## Edit 2 — place the tiles` / `## How it renders`, which
is the same shape scoped to a diff.
