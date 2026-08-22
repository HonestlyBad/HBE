# NN — [TAG] Human Readable Title (overview)

<!--
TEMPLATE. Replace every <angle-bracket> placeholder and delete all
HTML comments. Prose wraps at ~60 columns. Linux absolute paths.
Model: Docs/13-HBE-CPU-Frame-And-System-Timing/00_overview.md
-->

Item N-1 finished <what the previous item delivered, one clause>.
Item NN <what this item adds and why it matters now>.

<!-- For [HBE] items, pin the scope explicitly: -->
Per the work-item tag `[HBE]`, **all API and implementation edits live
under `/home/atulo/Projects/HBE/<Project>/`**. The only MegaX edits are
the *verification* pass in doc `0X_...`. Sandbox and MapMaker are
**not** touched.

---

## 1. What Item NN delivers

* A new header `<Project>/include/HBE/<Area>/<Name>.h` and matching
  `<Name>.cpp`.
* A public data structure `HBE::<NS>::<Type>` carrying:
  * `<field>` — <meaning and units>
  * `<field>` — <meaning and units>
* <Every other deliverable, named concretely — types, functions,
  macros, constants, toggles.>
* Integration into `<existing subsystem>`:
  * <exact call site 1>
  * <exact call site 2>
<!-- For an [HBE] item, list the MegaX work as its own bullet and say
     which kind it is. Pick ONE (or both, in separate docs): -->
* **Required** MegaX updates in `<file>` (doc `0X`) — the engine change
  to `<symbol>` would otherwise break the build.
* **Verification** edits in `<consumer file>` (doc `0X`) that
  <satisfy which done criterion>.

<!-- Only if an [HBE] item touches MegaX. Use the matching variant: -->

<!-- variant A — verification only (the doc is skippable) -->
> **NOTE ON `[HBE]` SCOPE.** The engine changes above are the full
> deliverable. The MegaX edits in doc `0X` are strictly a **consumer**
> of the new HBE API — they add no game-specific logic to HBE and
> change no gameplay. Strip them out and HBE still builds and still
> ships the feature.

<!-- variant B — required (the doc is NOT skippable) -->
> **NOTE ON `[HBE]` SCOPE.** The engine changes above are the
> deliverable, but they change `<symbol>`, which MegaX calls in
> `<file>`. The MegaX edits in doc `0X` are **required** — without them
> MegaX does not compile. They add no game-specific logic to HBE and
> change no gameplay.

---

## 2. Success criteria

Item NN is complete when **all** of the following hold:

1. `<Project>` builds clean in the Debug config.
2. `<Project>` builds clean in the Release config, with
   <what must be true there>.
3. Running MegaX (Debug) produces <observable output>. Example:
   ```
   <expected output, real shape>
   ```
4. <Structural / API guarantee.>
5. <Numeric relationship that must hold, with tolerance.>
6. <Failure-mode behavior: what happens when unsupported / disabled.>

---

## 3. Golden rules (read before touching code)

1. **<Scope rule.>** Only touch files under
   `/home/atulo/Projects/HBE/<Project>/`. <Stated exception, if any.>
2. **<API shape decision.>** <What to do, and the alternative that was
   rejected, and why it was rejected.>
3. **<Ownership / lifetime rule.>**
4. **<Allocation & hot-path rule.>** <e.g. zero allocations per call;
   `const char*` not `std::string`; fixed-capacity storage.>
5. **<Thread affinity.>**
6. **<Ordering requirement.>** <What must run before what.>
7. **<Data lifetime for anything returned by reference.>**
8. **<Compile-time-off must be truly zero-cost / graceful failure.>**
9. **<What the item explicitly forbids.>**
10. **<What must keep working — don't break X.>**

---

## 4. Files touched

| File | Item N-1 | Item NN |
|---|---|---|
| `<Project>/include/HBE/<Area>/<Name>.h` | — | **NEW** — <what it declares> |
| `<Project>/src/<Area>/<Name>.cpp` | — | **NEW** — <what it implements> |
| `<Project>/include/HBE/<Area>/<Existing>.h` | <prior role> | <exact change> |
| `<Project>/src/<Area>/<Existing>.cpp` | <prior role> | <exact change> |
| `<Project>/CMakeLists.txt` | — | add `src/<Area>/<Name>.cpp` to the source list |
| `MegaX/src/Game/GameLayer.cpp` | <prior role> | **verification only**: <what> — *or* **required — API changed**: <what> |

<Everything not touched, named explicitly.> No `.vcxproj` / `.slnx`
edits — this project builds through CMake only.

---

## 5. Design shape

### <The core data structure>

```cpp
<the struct, with a comment per non-obvious field>
```

<Why this shape. What it costs. What it rules out.>

### <The algorithm / lifecycle>

<Numbered walk-through of what happens per frame / per call.>

### API surface (final)

Everything exposed to games lives in `namespace HBE::<NS>`:

```cpp
<the complete public API in one block — this is the contract the
implementation docs must produce exactly>
```

<One line on what is deliberately absent.>

---

## 6. Non-goals (deferred)

| Behavior | Deferred to |
|---|---|
| <capability> | Item NN |
| <capability> | Later (<reason>) |
| <capability> | **Never** — <why> |

---

<!--
=============================================================
CONDITIONAL SECTIONS — include when they apply. Items 13/14 omit
all three because they are engine-infrastructure items; items
08-12 carry them. Do not skip these on a gameplay item.

When you include one, NUMBER it into the sequence and place it
BEFORE "Reading order", which always comes last. So an item with
new controls and new assets runs: ... 6. Non-goals, 7. Controls,
8. Asset notes, 9. Reading order.
=============================================================
-->

## Controls added in Item NN

<!-- Include when the item binds a new key or changes an existing one.
     Cross-check against MegaX/src/Game/GameLayer.cpp ~lines 128-171. -->

| Key | Effect |
|---|---|
| `<KEY>` | <what it does, and what state it preserves> |

All existing controls (<list them: WASD, SPACE, S crouch, G ghost,
B debug, LMB fire, F1/F2/F3 difficulty, F5/F6/F7 reload, F8 profiler,
H helmet, R HP refill>) keep the same behavior. <Call out explicitly
any key that a reader might expect to change but must not.>

## Asset notes

<!-- Include when the item consumes new art / audio / map data.
     Check the item's `-ASSETS:` list in Docs/WorkItems.txt, then
     MEASURE the files — do not guess dimensions. -->

| Sheet | Image size | Grid | Frame size | Notes for Item NN |
|---|---|---|---|---|
| `<file>.png` | `<W x H>` | `<c × r>` | `<w × h>` | <which rows are used now vs later> |

<Where the visible pixels sit inside each cell, how that maps to the
anchoring convention already used by Player/Enemy, and any engine
limitation that constrains the approach.>

---

## 7. Reading order

1. `00_overview.md` — you are here
2. `01_<name>.md` — <one line>
3. `02_<name>.md` — <one line>
4. `03_<name>.md` — <one line>
5. `0N_build_run_and_verify.md` — build steps + checklist

Do them in that order. Each doc lists **exact** file paths, exact
insertion points ("right after line 334", "in the block that starts
with `for (std::size_t i = 0; ...`"), and full copy-paste code blocks.
