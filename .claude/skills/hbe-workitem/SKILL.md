---
name: hbe-workitem
description: Author the numbered step-by-step markdown doc set for an HBE or MegaX work item from Docs/WorkItems.txt, matching the precision of Docs/13-* and Docs/14-*. Use whenever the user says "let's do work item N", "next work item", "write the docs for item N", or asks to plan/spec a change to HBE.Core, HBE.Platform.SDL, HBE.Renderer.GL, or MegaX. Also use to update an existing work item's docs.
---

# Authoring an HBE work item

The user implements **every line by hand** from the docs you write.
A doc that says "add a scope around the update loop" is a failure. A
doc that says "open `<abs path>`, find line 363, insert this exact
block between it and line 367" is the product.

Optimize for: **can the user apply this without opening the codebase
themselves, and without guessing?**

## Step 1 — Read the item and the current code

1. Read the item's paragraph in
   `/home/atulo/Projects/HBE/Docs/WorkItems.txt`. Note its `[HBE]` /
   `[MEGAX]` tag — that fixes which projects you may touch, and the
   two tags are **not symmetric** (see `references/conventions.md`).
   `[MEGAX]` means the game only, no engine edits at all. `[HBE]`
   means the engine *plus* whatever MegaX needs to stay working.

   For an `[HBE]` item, before you plan anything, grep MegaX for every
   symbol the engine change touches:

   ```fish
   grep -rn "<Symbol>\|<Type>\|<MACRO>" /home/atulo/Projects/HBE/MegaX/
   ```

   Hits mean updating MegaX is part of this item, not optional. No
   hits still leaves the verification pass, which is expected.
2. Read `references/project-map.md` for where things live.
3. **Read every file you intend to instruct an edit to, in full.**
   Line numbers, existing indentation, and surrounding code go into
   the doc verbatim. Never cite a line number you have not seen this
   session.
4. Check `Docs/GameEngine/ENGINE_API_REFERENCE.md` for the APIs you
   will compose, then confirm the signature in the actual header. The
   header wins.
5. Read the previous item's `00_overview.md` so §1 can open by naming
   what the last item finished. Note that items 00–12 use two older
   heading styles — read them for *content*, imitate 13/14 for
   *structure*. Item 00 is Visual Studio project setup and is fully
   obsolete under CMake.

Do not skip 3. Stale line numbers are the single most damaging defect
in these docs.

## Step 2 — Plan the doc split

Create `/home/atulo/Projects/HBE/Docs/NN-Kebab-Case-Title/`, where
`NN` is the zero-padded item number and the title is the WorkItems.txt
title in Kebab-Case (drop the `[HBE]` / `[MEGAX]` tag; keep `HBE-` in
the name when the item is engine-side, matching
`13-HBE-CPU-Frame-And-System-Timing`).

Not every item is in `WorkItems.txt` — engine work that came up
mid-flight is normal. For an unlisted item, take the next unused `NN`
from the `Docs/` listing, derive the title yourself, decide the tag
from which projects the work touches, and state that decision in the
overview. Don't append it to `WorkItems.txt`; that file is the planned
backlog, not a log of everything done.

If the directory already exists, this is an **update** — read what's
there and revise in place.

Split by **file cohesion**, not by feature narrative — one doc per
file or tight file pair, so the user finishes a doc and has one file
in a consistent state:

```
00_overview.md                 always
01_<new_thing>_api.md          new header
02_<new_thing>_impl.md         new .cpp + CMakeLists registration
03_<existing_file>_wiring.md   edits to existing engine files
04_<name>_json.md              only if the item defines/changes a data file
05_megax_<purpose>.md          game-side consumption / verification
06_build_run_and_verify.md     always, always last
```

Typical count is 4–7 docs. Name them after what they change, in
`snake_case` (see the existing dirs for the vocabulary:
`_api`, `_impl`, `_wiring`, `_code`, `_integration`, `_instrumentation`,
`_json`, `_and_verify`).

### Then pick the conditional sections

Items 13/14 are engine-infrastructure items and omit three sections
that nearly every gameplay item carries. Decide explicitly:

| Include | When | Where |
|---|---|---|
| **Tuning table** | the item changes gameplay feel or balance (9 of 10 gameplay items have one; 13/14 don't) | build/verify doc |
| **Controls added in Item NN** | the item binds or changes a key | overview |
| **Asset notes** | the item consumes new art / audio / map data | overview |
| **Data-format doc** | the item defines or changes a data file | its own doc |

`references/doc-format.md` gives each one's anatomy. Getting this
wrong is the most likely way to ship a doc set that looks right and
is missing what the user actually reaches for.

## Step 3 — Write the docs

Read `references/doc-format.md` before writing the first line — it
specifies section-by-section what each doc type contains.

Use the templates as skeletons:

* `templates/00_overview.md`
* `templates/NN_implementation.md`
* `templates/NN_data_format.md` — only for data-file docs
* `templates/NN_build_run_and_verify.md`

Non-negotiables, every doc:

* **Linux absolute paths** — `/home/atulo/Projects/HBE/HBE.Core/include/HBE/Core/Foo.h`.
* **fish** for every shell snippet.
* **Complete, compilable code blocks.** No `// ...`, no "add the rest".
  If a function is 60 lines and 3 change, print all 63.
* **Anchored insertion points.** "Right after line 334
  (`HBE::Core::Profiler::BeginFrame();`)" — line number *and* the text
  at it, so a drifted number is self-correcting.
* **Prose wrapped at ~60 columns.** Match the existing docs; they read
  as a narrow column. Code blocks and tables are exempt.
* **A `Next: NN_<name>.md` line** at the end of every doc but the last.

## Step 4 — Verify before handing over

### 4a. Prove the pasteable code compiles

The user types this code by hand — shipping code that doesn't compile
costs them a debugging session on your mistake. **Never test in their
working tree.** Copy it to the scratchpad, apply your own edits there,
and compile:

```fish
cp -r /home/atulo/Projects/HBE $SCRATCH/hbe-check
# apply every edit the docs instruct, then:
cd $SCRATCH/hbe-check
cmake --preset linux-clang
cmake --build --preset linux-clang-debug --target MegaX
```

For a single new header/`.cpp` pair, a syntax-only pass is enough and
much faster:

```fish
clang++ -std=c++20 -fsyntax-only \
  -I HBE.Core/include -I HBE.Platform.SDL/include \
  -I HBE.Renderer.GL/include -I HBE.Renderer.GL/external/glad/include \
  -I external/nlohmann -I external/stb \
  $(pkg-config --cflags sdl3) \
  HBE.Core/src/Core/<New>.cpp
```

Fix the docs, not just the scratchpad copy. Then delete the copy.

### 4b. Check the docs themselves

```fish
cd /home/atulo/Projects/HBE

# Every path cited in the docs must exist (new files excepted).
grep -oh '/home/atulo/Projects/HBE/[^ `)]*' Docs/NN-*/*.md | sort -u

# No Windows paths or PowerShell leaked in from the old docs.
grep -n 'G:\\\|PowerShell\|Get-Content\|Test-Path\|vcxproj\|msbuild' Docs/NN-*/*.md
```

Then re-read `00_overview.md` §4 against the other docs: the files-touched
table must list exactly the files the other docs actually edit — no more,
no fewer. This table is what the user trusts to know the blast radius.

Finally, re-check every line number you cited against the real file —
if an earlier doc in the same set inserts lines into a file, the later
docs' line numbers for that file must account for the shift.

## Step 5 — Report

Tell the user the directory, the doc list with one line each, and the
reading order. Do **not** implement the code. If they then ask you to
implement it, that is a separate instruction — follow the docs you wrote.

## After the item is implemented

When the user reports an item done:

1. Offer to update `Docs/GameEngine/ENGINE_API_REFERENCE.md` with any
   new public engine surface (new header → new `###` subsection under
   the right numbered section, plus a TOC entry if a whole section is
   new).
2. Offer to update the "items complete" line in `CLAUDE.md`.

## References

* `references/project-map.md` — every source file, what owns what, how the libs layer
* `references/doc-format.md` — the required anatomy of each doc type
* `references/build-and-run.md` — build commands, run paths, MegaX hotkeys, verification patterns
* `references/conventions.md` — code style, CMake registration, naming, git
